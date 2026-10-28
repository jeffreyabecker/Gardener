#include "Build.h"

#if GARDENER_PANEL

#include "routines/DrawTree.h"

#include <berry.h>
#include <string.h>

#include <vector>

#include <Gfx.h>

#include "PanelContext.h"
#include "panel/PressureReadout.h"
#include "panel/Units.h"
#include "storage/Config.h"
#include "storage/DeviceConfig.h"

namespace gardener {
namespace {

using gfx::ui::Alignment;
using gfx::ui::Cell;
using gfx::ui::Child;
using gfx::ui::Horizontal;
using gfx::ui::Paint;
using gfx::ui::Vertical;

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

gfx::Rect boxOf(bvm* vm, int node) {
  return gfx::Rect((int16_t)intField(vm, node, "x"), (int16_t)intField(vm, node, "y"),
                   (int16_t)intField(vm, node, "w"), (int16_t)intField(vm, node, "h"));
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

gfx::ui::DurationFormat formatOf(const char* name) {
  if (name != nullptr && strcmp(name, "seconds") == 0) return gfx::ui::DurationFormat::Seconds;
  if (name != nullptr && strcmp(name, "hours") == 0) return gfx::ui::DurationFormat::Hours;
  return gfx::ui::DurationFormat::Minutes;
}

// Where a layout puts what it holds: the two names of the alignment record, the left and the top
// when the node carries none.
Alignment alignmentOf(bvm* vm, int node) {
  Alignment alignment;
  be_getmember(vm, node, "alignment");
  if (be_isinstance(vm, -1)) {
    const int record = be_absindex(vm, -1);
    const char* across = textField(vm, record, "align");
    const char* down = textField(vm, record, "v_align");
    if (across != nullptr && strcmp(across, "center") == 0) {
      alignment.horizontal = Horizontal::Center;
    } else if (across != nullptr && strcmp(across, "right") == 0) {
      alignment.horizontal = Horizontal::Right;
    }
    if (down != nullptr && strcmp(down, "center") == 0) {
      alignment.vertical = Vertical::Center;
    } else if (down != nullptr && strcmp(down, "bottom") == 0) {
      alignment.vertical = Vertical::Bottom;
    }
  }
  be_pop(vm, 1);
  return alignment;
}

Paint paintOf(bvm* vm, int node, PanelContext& panel, DisplayUnit unit);

// The [bounds, draw] pairs a layout holds, as the layout's own children: the cell each declares and
// the paint each answers. A child that is not a pair is skipped rather than raised on.
std::vector<Child> childrenOf(bvm* vm, int node, PanelContext& panel, DisplayUnit unit) {
  std::vector<Child> children;

  be_getmember(vm, node, "children");
  const int list = be_absindex(vm, -1);
  const int count = be_islist(vm, list) ? be_data_size(vm, list) : 0;
  for (int i = 0; i < count; i++) {
    be_pushint(vm, i);
    be_getindex(vm, list);
    be_remove(vm, -2);  // the index the read left under the pair
    if (!be_islist(vm, -1) || be_data_size(vm, -1) != 2) {
      be_pop(vm, 1);
      continue;
    }

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
    const Paint paint = paintOf(vm, be_absindex(vm, -1), panel, unit);
    be_pop(vm, 1);

    children.push_back(Child{Cell(width, height), paint});
    be_pop(vm, 1);  // the pair
  }
  be_pop(vm, 1);  // the list

  return children;
}

// The node as the paint it describes. A kind the drawing has no case for answers an empty paint,
// which is a node that draws nothing.
Paint paintOf(bvm* vm, int node, PanelContext& panel, DisplayUnit unit) {
  if (!be_isinstance(vm, node)) return Paint();
  const char* kind = textField(vm, node, "kind");
  if (kind == nullptr) return Paint();

  if (strcmp(kind, "text") == 0) {
    const gfx::Rect box = boxOf(vm, node);
    const char* value = textField(vm, node, "value");
    const gfx::Font* face = faceOf(vm, node);
    return gfx::ui::text(box, value, face, (uint8_t)intField(vm, node, "scale"));
  }

  if (strcmp(kind, "centred") == 0) {
    const gfx::Rect box = boxOf(vm, node);
    const char* value = textField(vm, node, "value");
    const gfx::Font* face = faceOf(vm, node);
    const uint8_t scale = (uint8_t)intField(vm, node, "scale");
    return [box, value, face, scale](gfx::Canvas& canvas) {
      gfx::Canvas own = canvas.child(box);
      gfx::ui::drawCentred(own, value, face, scale);
    };
  }

  if (strcmp(kind, "icon") == 0) {
    const gfx::Rect box = boxOf(vm, node);
    be_getmember(vm, node, "glyph");
    const Paint glyph =
        be_isinstance(vm, -1) ? paintOf(vm, be_absindex(vm, -1), panel, unit) : Paint();
    be_pop(vm, 1);
    // An icon of nothing is nothing: ui::icon runs the glyph it is given and has no empty case.
    if (!glyph) return Paint();
    return gfx::ui::icon(box, glyph);
  }

  if (strcmp(kind, "bar") == 0) {
    const gfx::Rect box = boxOf(vm, node);
    return gfx::ui::progressBar(box, realField(vm, node, "percent"));
  }

  if (strcmp(kind, "scrollbar") == 0) {
    const gfx::Rect box = boxOf(vm, node);
    return gfx::ui::scrollbar(box, (int16_t)intField(vm, node, "index"),
                              (int16_t)intField(vm, node, "count"),
                              (int16_t)intField(vm, node, "visible"),
                              flagField(vm, node, "hollow", false));
  }

  if (strcmp(kind, "duration") == 0) {
    const gfx::Rect box = boxOf(vm, node);
    const gfx::ui::DurationFormat format = formatOf(textField(vm, node, "format"));
    const gfx::Font* face = faceOf(vm, node);
    const int seconds = intField(vm, node, "seconds");
    return gfx::ui::duration(box, format, seconds, face, flagField(vm, node, "known", true));
  }

  // The readout takes the machine and the unit rather than a number, so it prints what is true when
  // it is drawn — which is why one node serves every beat.
  if (strcmp(kind, "pressure_readout") == 0) {
    const gfx::Rect box = boxOf(vm, node);
    float sizeHint = realField(vm, node, "size_hint");
    if (sizeHint <= 0.0f) sizeHint = 0.5f;
    const gfx::Font* face = faceOf(vm, node);
    return pressureReadout(box, panel.vacuum(), unit, sizeHint, face,
                           (uint8_t)intField(vm, node, "scale"),
                           (uint8_t)intField(vm, node, "secondary_scale"));
  }

  if (strcmp(kind, "layout") == 0) {
    const Cell cell(cellSide(vm, node, "w"), cellSide(vm, node, "h"));
    return gfx::ui::layout(cell, childrenOf(vm, node, panel, unit), alignmentOf(vm, node));
  }

  if (strcmp(kind, "row") == 0) {
    const float height = cellSide(vm, node, "h");
    return gfx::ui::row(height, childrenOf(vm, node, panel, unit), alignmentOf(vm, node));
  }

  if (strcmp(kind, "column") == 0) {
    const float width = cellSide(vm, node, "w");
    return gfx::ui::column(width, childrenOf(vm, node, panel, unit), alignmentOf(vm, node));
  }

  return Paint();
}

}  // namespace

void drawTree(bvm* vm, int node, PanelContext& panel, gfx::Canvas& canvas) {
  // The unit the readout prints in belongs to the device, resolved once for the whole tree rather
  // than once per node: one drawing shows one glass.
  DisplayUnit unit = DisplayUnit::Kpa;
  parseDisplayUnit(panel.config().get<String>(DeviceSection, UnitKey, "").c_str(), unit);

  const Paint paint = paintOf(vm, be_absindex(vm, node), panel, unit);
  if (paint) paint(canvas);
}

}  // namespace gardener
#endif
