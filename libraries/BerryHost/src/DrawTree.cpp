#include "DrawTree.h"

#include <berry.h>
#include <string.h>

#include <vector>

#include <Gfx.h>

#include "core/SystemState.h"
#include "panel/PanelContext.h"
#include "panel/page/PressureReadout.h"

namespace gardener {
namespace {

using gfx::Cell;
using gfx::Child;
using gfx::Justification;
using gfx::Justify;

// --- reading a node --------------------------------------------------------
//
// A kind sets the fields it uses and leaves the rest nil, so every read has the answer a missing
// field has. None of these raises: a node the walk cannot make sense of draws less, it does not take
// the beat down.

int intField(bvm* vm, int node, const char* field) {
  if (!be_getmember(vm, node, field)) return 0;
  const bint value = be_isnumber(vm, -1) ? be_toint(vm, -1) : 0;
  be_pop(vm, 1);
  return (int)value;
}

float realField(bvm* vm, int node, const char* field) {
  if (!be_getmember(vm, node, field)) return 0.0f;
  const float value = be_isnumber(vm, -1) ? (float)be_toreal(vm, -1) : 0.0f;
  be_pop(vm, 1);
  return value;
}

bool flagField(bvm* vm, int node, const char* field, bool fallback) {
  if (!be_getmember(vm, node, field)) return fallback;
  const bool value = be_isbool(vm, -1) ? be_tobool(vm, -1) != 0 : fallback;
  be_pop(vm, 1);
  return value;
}

// A string field, or nullptr. The answer points into the interpreter's own string, which lives as
// long as the node it was read from — and the node lives on the stack for the whole walk.
const char* textField(bvm* vm, int node, const char* field) {
  if (!be_getmember(vm, node, field)) return nullptr;
  const char* value = be_isstring(vm, -1) ? be_tostring(vm, -1) : nullptr;
  be_pop(vm, 1);
  return value;
}

// A declared side of a cell: nil is the whole of what holds it, which is what Cell says with Whole.
float cellSide(bvm* vm, int record, const char* field) {
  if (!be_getmember(vm, record, field)) return Cell::Whole;
  const float value = be_isnumber(vm, -1) ? (float)be_toreal(vm, -1) : Cell::Whole;
  be_pop(vm, 1);
  return value;
}

bool numberField(bvm* vm, int node, const char* field) {
  if (!be_getmember(vm, node, field)) return false;
  const bool present = be_isnumber(vm, -1);
  be_pop(vm, 1);
  return present;
}

// The box a leaf declares: x and y in pixels from the corner of the canvas it is drawn in, and a
// side the page left out is the whole of that canvas — which is how a node fills the cell a layout
// gave it without the page computing a number.
gfx::Box boxOf(bvm* vm, int node) {
  const int16_t x = (int16_t)intField(vm, node, "x");
  const int16_t y = (int16_t)intField(vm, node, "y");
  const int16_t w = (int16_t)intField(vm, node, "w");
  const int16_t h = (int16_t)intField(vm, node, "h");
  const bool wholeWidth = !numberField(vm, node, "w");
  const bool wholeHeight = !numberField(vm, node, "h");
  return gfx::Box(x, y, w, h, wholeWidth, wholeHeight);
}

// The face a node names. A name the library does not hold is drawn in the built-in one: the binding
// refused it when the page built the node, so there is nothing left to say about it here.
const gfx::Font* faceOf(bvm* vm, int node) {
  const char* name = textField(vm, node, "font");
  if (name == nullptr) return nullptr;
  const gfx::Font* face = nullptr;
  gfx::fonts::find(name, face);
  return face;
}

gfx::DurationFormat formatOf(const char* name) {
  if (name != nullptr && strcmp(name, "seconds") == 0) return gfx::DurationFormat::Seconds;
  if (name != nullptr && strcmp(name, "hours") == 0) return gfx::DurationFormat::Hours;
  return gfx::DurationFormat::Minutes;
}

// The justify a record's field names: the centre and the end, and the start for a name that is
// neither, which is what the binding's default leaves.
Justify justifyOf(bvm* vm, int record, const char* field) {
  const char* name = textField(vm, record, field);
  if (name != nullptr && strcmp(name, "center") == 0) return Justify::Center;
  if (name != nullptr && strcmp(name, "end") == 0) return Justify::End;
  return Justify::Start;
}

// Where a layout puts what it holds: the two names of the alignment record, the start of each when
// the node carries none.
Justification justificationOf(bvm* vm, int node) {
  Justification justification;
  be_getmember(vm, node, "alignment");
  if (be_isinstance(vm, -1)) {
    const int record = be_absindex(vm, -1);
    justification.along = justifyOf(vm, record, "along");
    justification.across = justifyOf(vm, record, "across");
  }
  be_pop(vm, 1);
  return justification;
}

gfx::PaintNodeRef paintOf(bvm* vm, int node, const SystemState& state);

// The items of a list value, pushed on top: a page's `[...]` is an instance of the list class and the
// interpreter keeps its items in the instance's `.p`, which is what the data calls read. False when
// the value is not a list, and then nothing is pushed.
bool pushItems(bvm* vm, int index) {
  if (be_islist(vm, index)) {
    be_pushvalue(vm, index);
    return true;
  }
  if (!be_islistinstance(vm, index)) return false;
  be_getmember(vm, index, ".p");
  if (be_islist(vm, -1)) return true;
  be_pop(vm, 1);
  return false;
}

// The [bounds, draw] pairs a layout holds, as the layout's own children: the cell each declares and
// the paint each answers. A child that is not a pair is skipped rather than raised on.
gfx::Children childrenOf(bvm* vm, int node, const SystemState& state) {
  gfx::Children children;

  be_getmember(vm, node, "children");
  if (!pushItems(vm, -1)) {
    be_pop(vm, 1);
    return children;
  }
  const int list = be_absindex(vm, -1);
  const int count = be_data_size(vm, list);
  for (int i = 0; i < count; i++) {
    be_pushint(vm, i);
    be_getindex(vm, list);
    be_remove(vm, -2);  // the index the read left under the child
    if (pushItems(vm, -1)) {
      if (be_data_size(vm, -1) == 2) {
        const int pair = be_absindex(vm, -1);

        be_pushint(vm, 0);
        be_getindex(vm, pair);
        be_remove(vm, -2);
        float width = Cell::Whole;
        float height = Cell::Whole;
        if (be_isinstance(vm, -1)) {
          const int cell = be_absindex(vm, -1);
          width = cellSide(vm, cell, "w");
          height = cellSide(vm, cell, "h");
        }
        be_pop(vm, 1);

        be_pushint(vm, 1);
        be_getindex(vm, pair);
        be_remove(vm, -2);
        gfx::PaintNodeRef paint = paintOf(vm, be_absindex(vm, -1), state);
        be_pop(vm, 1);

        children.push_back(gfx::Child{Cell(width, height), std::move(paint)});
      }
      be_pop(vm, 1);  // the pair's items
    }
    be_pop(vm, 1);  // the child
  }
  be_pop(vm, 2);  // the items, and the field they came from

  return children;
}

// The node as the paint it describes. A kind the drawing has no case for answers an empty paint,
// which is a node that draws nothing.
gfx::PaintNodeRef paintOf(bvm* vm, int node, const SystemState& state) {
  if (!be_isinstance(vm, node)) return gfx::PaintNodeRef();
  const char* kind = textField(vm, node, "kind");
  if (kind == nullptr) return gfx::PaintNodeRef();

  if (strcmp(kind, "text") == 0) {
    const char* value = textField(vm, node, "value");
    return gfx::borrowedText(boxOf(vm, node), value, faceOf(vm, node),
                             (uint8_t)intField(vm, node, "scale"));
  }

  if (strcmp(kind, "centred") == 0) {
    // The ink the string takes, centred in the box the node declares: the same arithmetic the call
    // this replaces made, said in the layout's own words.
    const char* value = textField(vm, node, "value");
    const gfx::Font* face = faceOf(vm, node);
    const uint8_t raw = (uint8_t)intField(vm, node, "scale");
    const uint8_t scale = raw == 0 ? 1 : raw;
    const gfx::Size ink = gfx::shapes::measureText(face, value, scale);
    return gfx::icon(
        boxOf(vm, node),
        gfx::aligned(gfx::sized(gfx::borrowedText(gfx::Box::whole(), value, face, scale), ink)));
  }

  if (strcmp(kind, "icon") == 0) {
    // The hex digits point into the node's own string, which lives as long as the node the walk is
    // holding — the same lifetime a text field's string has. The bitmap is read when the paint runs.
    const char* glyph = textField(vm, node, "glyph");
    if (glyph == nullptr) return gfx::PaintNodeRef();
    return gfx::icon(boxOf(vm, node), glyph);
  }

  if (strcmp(kind, "bar") == 0) {
    return gfx::progressBar(boxOf(vm, node), realField(vm, node, "percent"));
  }

  if (strcmp(kind, "scrollbar") == 0) {
    return gfx::scrollbar(boxOf(vm, node), (int16_t)intField(vm, node, "index"),
                          (int16_t)intField(vm, node, "count"),
                          (int16_t)intField(vm, node, "visible"),
                          flagField(vm, node, "hollow", false));
  }

  if (strcmp(kind, "duration") == 0) {
    return gfx::duration(boxOf(vm, node), formatOf(textField(vm, node, "format")),
                         intField(vm, node, "seconds"), faceOf(vm, node),
                         flagField(vm, node, "known", true));
  }

  // The readout takes the state the tree was built with rather than a number, so one node serves
  // every beat: the tree is built again for each draw.
  if (strcmp(kind, "pressure_readout") == 0) {
    float sizeHint = realField(vm, node, "size_hint");
    if (sizeHint <= 0.0f) sizeHint = 0.5f;
    return panel::page::pressureReadout(boxOf(vm, node), state, sizeHint, faceOf(vm, node),
                                       (uint8_t)intField(vm, node, "scale"),
                                       (uint8_t)intField(vm, node, "secondary_scale"));
  }

  if (strcmp(kind, "layout") == 0) {
    const Cell cell(cellSide(vm, node, "w"), cellSide(vm, node, "h"));
    return gfx::layout(cell, childrenOf(vm, node, state), justificationOf(vm, node));
  }

  if (strcmp(kind, "row") == 0) {
    const float height = cellSide(vm, node, "h");
    return gfx::row(height, childrenOf(vm, node, state), justificationOf(vm, node));
  }

  if (strcmp(kind, "column") == 0) {
    const float width = cellSide(vm, node, "w");
    return gfx::column(width, childrenOf(vm, node, state), justificationOf(vm, node));
  }

  // The draw the node holds, run and then inverted in the cell the node was given: the step that
  // makes a row read as paper on ink after it has been painted.
  if (strcmp(kind, "inverted") == 0) {
    be_getmember(vm, node, "child");
    gfx::PaintNodeRef child =
        be_isinstance(vm, -1) ? paintOf(vm, be_absindex(vm, -1), state) : gfx::PaintNodeRef();
    be_pop(vm, 1);
    return gfx::inverted(std::move(child));
  }

  return gfx::PaintNodeRef();
}

}  // namespace

void drawTree(bvm* vm, int node, panel::PanelContext& panel, gfx::Canvas& canvas) {
  // One copy of the published state for the whole tree rather than one per node: one drawing shows
  // one glass, and the unit the readout prints in comes with it.
  const SystemState state = panel.systemState();
  const gfx::PaintNodeRef paint = paintOf(vm, be_absindex(vm, node), state);
  if (paint) paint->draw(canvas);
}

}  // namespace gardener
