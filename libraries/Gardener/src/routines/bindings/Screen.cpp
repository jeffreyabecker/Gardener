#include <stdio.h>
#include <string.h>

#include <Gfx.h>

#include "BindingContext.h"
#include "Module.h"
#include "Modules.h"

namespace gardener {
namespace berry {
namespace {

// The record a page's drawing is made of, installed by this module. Every call below fills one in and
// hands it back: what is drawn, not the drawing. Nothing here reaches a canvas.
constexpr const char* DrawClass = "draw";

// The fields one node of a page's drawing carries. `kind` names what the node is — text, centred,
// icon, inverted, bar, scrollbar, duration, pressure_readout, layout, row or column — and the fields
// that kind uses are set, the rest nil. A leaf names its box in pixels; a layout, a row and a column
// name the cell they declare as a share of what holds them, nil for the whole side, so a row sets `h`
// and a column `w`. A layout's `children` is a list of [bounds, draw] pairs — the cell a child
// declares and the child — and `alignment` is where it puts them, the record below. An `inverted` node
// holds the draw it flips in `child`, and an `icon` node holds its fixed bitmap in `glyph` as the hex
// digits that name it: two per byte, the width, the height, then one row per byte-run, most
// significant bit first and each row padded to a whole byte. The panel reads them when it draws the
// node.
const bnfuncinfo DrawFields[] = {
    {"init", recordInit},
    {"kind", nullptr},
    {"x", nullptr},
    {"y", nullptr},
    {"w", nullptr},
    {"h", nullptr},
    {"value", nullptr},
    {"font", nullptr},
    {"scale", nullptr},
    {"secondary_scale", nullptr},
    {"size_hint", nullptr},
    {"format", nullptr},
    {"seconds", nullptr},
    {"known", nullptr},
    {"percent", nullptr},
    {"index", nullptr},
    {"count", nullptr},
    {"visible", nullptr},
    {"hollow", nullptr},
    {"children", nullptr},
    {"child", nullptr},
    {"glyph", nullptr},
    {"alignment", nullptr},
    {nullptr, nullptr},
};

// `bounds(w, h)`, and `bounds()` for the record `screen.measure` fills: a side left out stays nil,
// which the drawing reads as the whole of the canvas the node sits in.
int boundsInit(bvm* vm) {
  if (be_top(vm) >= 2 && !be_isnil(vm, 2)) setValue(vm, 1, "w", 2);
  if (be_top(vm) >= 3 && !be_isnil(vm, 3)) setValue(vm, 1, "h", 3);
  be_return_nil(vm);
}

// The width and height of a box, in pixels; the cell a Layout child declares, each a share of the
// parent in [0, 1], a number of pixels when it is above 1, or nil for the whole of the parent; and the
// ink box `screen.measure` answers.
const bnfuncinfo BoundsFields[] = {
    {"init", boundsInit},
    {"w", nullptr},
    {"h", nullptr},
    {nullptr, nullptr},
};

// `alignment(along, across)`: a side left out is the start of it, which is what a layout, a row and
// a column draw by when the node carries no record.
int alignmentInit(bvm* vm) {
  setText(vm, 1, "along", be_top(vm) >= 2 && be_isstring(vm, 2) ? argText(vm, 2) : "start");
  setText(vm, 1, "across", be_top(vm) >= 3 && be_isstring(vm, 3) ? argText(vm, 3) : "start");
  be_return_nil(vm);
}

// Where a layout puts what it holds: `along` places the run of cells on the stacking axis and
// `across` each cell on the other, each start, center or end and nil for the start. A layout, a row
// and a column take one after their children, and the default is the start of both.
const bnfuncinfo AlignmentFields[] = {
    {"init", alignmentInit},
    {"along", nullptr},
    {"across", nullptr},
    {nullptr, nullptr},
};

// The messages below are stack buffers because be_raise longjmps past any C++ destructor a String
// would need.

// The face a call names, held to the table the renderer will look it up in. Nil is the built-in 5x7,
// and a name that is not a compiled face is a typo at the call that made it rather than a blank line
// when the tree is drawn.
const gfx::Font* argFace(bvm* vm, int index, const char* function) {
  if (be_top(vm) < index || be_isnil(vm, index)) return nullptr;
  if (!be_isstring(vm, index)) {
    char message[64];
    snprintf(message, sizeof(message), "screen.%s: font name expected", function);
    be_raise(vm, "type_error", message);
  }
  const char* name = argText(vm, index);
  if (name[0] == '\0') return nullptr;
  const gfx::Font* face = nullptr;
  if (!gfx::fonts::find(name, face)) {
    char message[64];
    snprintf(message, sizeof(message), "screen.%s: unknown font %s", function, name);
    be_raise(vm, "value_error", message);
  }
  return face;
}

// The size a face is drawn at: 1 for the face as designed, which is what an absent argument means.
uint8_t argScale(bvm* vm, int index, const char* function, uint8_t fallback) {
  if (be_top(vm) < index || be_isnil(vm, index)) return fallback;
  if (!be_isnumber(vm, index)) {
    char message[64];
    snprintf(message, sizeof(message), "screen.%s: scale expected", function);
    be_raise(vm, "type_error", message);
  }
  const bint scale = argInt(vm, index);
  return scale < 1 ? 1 : (uint8_t)scale;
}

// A number the call has a default for, for an argument a page may leave out or pass nil.
float argNumber(bvm* vm, int index, const char* function, float fallback) {
  if (be_top(vm) < index || be_isnil(vm, index)) return fallback;
  if (!be_isnumber(vm, index)) {
    char message[64];
    snprintf(message, sizeof(message), "screen.%s: number expected", function);
    be_raise(vm, "type_error", message);
  }
  return (float)be_toreal(vm, index);
}

// The format a call names, held to the three the duration formatter knows and handed back as written,
// so a record keeps the name the page used rather than a number nothing can read.
const char* argFormat(bvm* vm, int index, const char* function) {
  const char* name = be_isstring(vm, index) ? argText(vm, index) : nullptr;
  if (name != nullptr) {
    if (strcmp(name, "minutes") == 0 || strcmp(name, "seconds") == 0 || strcmp(name, "hours") == 0) {
      return name;
    }
  }
  char message[64];
  snprintf(message, sizeof(message), "screen.%s: minutes, seconds or hours expected", function);
  be_raise(vm, name == nullptr ? "type_error" : "value_error", message);
  return "minutes";
}

gfx::DurationFormat formatOf(const char* name) {
  if (strcmp(name, "seconds") == 0) return gfx::DurationFormat::Seconds;
  if (strcmp(name, "hours") == 0) return gfx::DurationFormat::Hours;
  return gfx::DurationFormat::Minutes;
}

// The node of the named kind: the record this module installed, with the one field every kind has set.
int beginDraw(bvm* vm, const char* kind) {
  const int draw = beginRecord(vm, DrawClass);
  setText(vm, draw, "kind", kind);
  return draw;
}

// One side of a leaf's box. A side the page leaves out, or passes nil, stays nil — and the drawing
// reads that as the whole of the canvas the node is drawn in, which is how a node fills the cell a
// layout gave it without the page computing a number.
void setBoxSide(bvm* vm, int draw, const char* field, int index) {
  if (be_top(vm) < index || be_isnil(vm, index)) return;
  setInt(vm, draw, field, argInt(vm, index));
}

// The four sides of a leaf's box, in pixels from the corner of the canvas it is drawn in.
void setBox(bvm* vm, int draw) {
  setBoxSide(vm, draw, "x", 1);
  setBoxSide(vm, draw, "y", 2);
  setBoxSide(vm, draw, "w", 3);
  setBoxSide(vm, draw, "h", 4);
}

// The share of what holds it a layout, a row or a column declares. A side the page leaves out is the
// whole of it, which is the nil the drawing reads as the whole side.
void setCellSide(bvm* vm, int draw, const char* field, int index) {
  if (be_top(vm) < index || be_isnil(vm, index)) return;
  setReal(vm, draw, field, (float)argReal(vm, index));
}

// Whether the value at `index` is a list a page can build: an instance of the list class, which is
// what `[...]` makes, or the interpreter's own list.
bool isListValue(bvm* vm, int index) {
  return be_islist(vm, index) || be_islistinstance(vm, index);
}

// The items of a list value, pushed on top: the interpreter keeps a list instance's items in its
// `.p`, which is where the data calls read. False when the value is not a list, and then nothing is
// pushed.
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

// The children a layout is given: a list of [bounds, draw] pairs. What the node inside is belongs to
// whoever walks the tree, but a list that is not pairs is the mistake of the call that made it, so it
// is answered here rather than where the tree is drawn.
void requireChildren(bvm* vm, int index, const char* function) {
  if (!pushItems(vm, index)) return;
  const int list = be_absindex(vm, -1);
  const int count = be_data_size(vm, list);
  for (int i = 0; i < count; i++) {
    be_pushint(vm, i);
    be_getindex(vm, list);
    be_remove(vm, -2);  // the index be_getindex left under the child
    bool pair = false;
    if (pushItems(vm, -1)) {
      pair = be_data_size(vm, -1) == 2;
      be_pop(vm, 1);
    }
    be_pop(vm, 1);  // the child itself
    if (pair) continue;
    char message[64];
    snprintf(message, sizeof(message), "screen.%s: a child is [bounds, draw]", function);
    be_raise(vm, "type_error", message);
  }
  be_pop(vm, 1);  // the items
}

// The justification a layout is given, passed through as the record it is, or left nil for the
// start of both.
void setAlignment(bvm* vm, int draw, int index) {
  if (be_top(vm) < index || be_isnil(vm, index)) return;
  setValue(vm, draw, "alignment", index);
}

// --- the nodes -------------------------------------------------------------

int screenText(bvm* vm) {
  if (be_top(vm) < 5 || !be_isstring(vm, 5)) {
    be_raise(vm, "type_error", "screen.text(x, y, w, h, value[, font[, scale]])");
  }
  const gfx::Font* face = argFace(vm, 6, "text");
  const int draw = beginDraw(vm, "text");
  setBox(vm, draw);
  setText(vm, draw, "value", argText(vm, 5));
  if (face != nullptr) setText(vm, draw, "font", argText(vm, 6));
  setInt(vm, draw, "scale", argScale(vm, 7, "text", 1));
  be_return(vm);
}

// The same string centred across and down its box.
int screenCentred(bvm* vm) {
  if (be_top(vm) < 5 || !be_isstring(vm, 5)) {
    be_raise(vm, "type_error", "screen.centred(x, y, w, h, value[, font[, scale]])");
  }
  const gfx::Font* face = argFace(vm, 6, "centred");
  const int draw = beginDraw(vm, "centred");
  setBox(vm, draw);
  setText(vm, draw, "value", argText(vm, 5));
  if (face != nullptr) setText(vm, draw, "font", argText(vm, 6));
  setInt(vm, draw, "scale", argScale(vm, 7, "centred", 1));
  be_return(vm);
}

// A fixed glyph in the box: the argument is the hex digits that name the bitmap itself, in
// MonoBitmap's format — the width, the height, then one row per byte-run, most significant bit first
// and each row padded to a whole byte. The node keeps the string rather than the bytes, and the
// panel reads them when it draws the node; a string that names no glyph is refused here rather than
// drawn as nothing.
int screenIcon(bvm* vm) {
  if (be_top(vm) < 5 || !be_isstring(vm, 5)) {
    be_raise(vm, "type_error", "screen.icon(x, y, w, h, glyph)");
  }
  if (!gfx::shapes::isMonoBitmap(argText(vm, 5))) {
    be_raise(vm, "value_error",
             "screen.icon: the glyph is hex digits, the width, the height and the rows");
  }
  const int draw = beginDraw(vm, "icon");
  setBox(vm, draw);
  setValue(vm, draw, "glyph", 5);
  be_return(vm);
}

// A node that draws the tree it is given and then inverts the whole cell it lands in: how a page
// makes a selected row read as paper on ink.
int screenInverted(bvm* vm) {
  if (be_top(vm) < 1 || !be_isinstance(vm, 1)) {
    be_raise(vm, "type_error", "screen.inverted(draw)");
  }
  const int draw = beginDraw(vm, "inverted");
  setValue(vm, draw, "child", 1);
  be_return(vm);
}

// A framed bar filled to a percentage, held to its ends by whoever draws it.
int screenBar(bvm* vm) {
  if (be_top(vm) < 5) {
    be_raise(vm, "type_error", "screen.bar(x, y, w, h, percent)");
  }
  const int draw = beginDraw(vm, "bar");
  setBox(vm, draw);
  setReal(vm, draw, "percent", (float)argReal(vm, 5));
  be_return(vm);
}

int screenScrollbar(bvm* vm) {
  if (be_top(vm) < 7) {
    be_raise(vm, "type_error",
             "screen.scrollbar(x, y, w, h, index, count, visible[, hollow])");
  }
  const int draw = beginDraw(vm, "scrollbar");
  setBox(vm, draw);
  setInt(vm, draw, "index", argInt(vm, 5));
  setInt(vm, draw, "count", argInt(vm, 6));
  setInt(vm, draw, "visible", argInt(vm, 7));
  setFlag(vm, draw, "hollow", be_top(vm) >= 8 && !be_isnil(vm, 8) && be_tobool(vm, 8));
  be_return(vm);
}

// Whole seconds in the format's text, drawn from the top of the box. A count that is not known reads
// `--`, which is what an absent `known` means the opposite of.
int screenDuration(bvm* vm) {
  if (be_top(vm) < 6) {
    be_raise(vm, "type_error",
             "screen.duration(x, y, w, h, format, seconds[, font[, known]])");
  }
  const char* format = argFormat(vm, 5, "duration");
  const gfx::Font* face = argFace(vm, 7, "duration");
  const bool known = be_top(vm) < 8 || be_isnil(vm, 8) || be_tobool(vm, 8);
  const int draw = beginDraw(vm, "duration");
  setBox(vm, draw);
  setText(vm, draw, "format", format);
  setInt(vm, draw, "seconds", argInt(vm, 6));
  if (face != nullptr) setText(vm, draw, "font", argText(vm, 7));
  setFlag(vm, draw, "known", known);
  be_return(vm);
}

// The machine's own readout: the depth it is at, and its target as a second reading, both printed in
// the panel's unit. Nothing about the machine is in the node — it names where the readout goes and
// how it is set, and the numbers are the ones in force when it is drawn. Below a size hint of 1 the
// second reading sits small in the bottom right of the box; at 1 or more the two sit side by side at
// the main scale, the second on the right.
int screenPressureReadout(bvm* vm) {
  const gfx::Font* face = argFace(vm, 6, "pressure_readout");
  const int draw = beginDraw(vm, "pressure_readout");
  setBox(vm, draw);
  setReal(vm, draw, "size_hint", argNumber(vm, 5, "pressure_readout", 0.5f));
  if (face != nullptr) setText(vm, draw, "font", argText(vm, 6));
  setInt(vm, draw, "scale", argScale(vm, 7, "pressure_readout", 2));
  setInt(vm, draw, "secondary_scale", argScale(vm, 8, "pressure_readout", 1));
  be_return(vm);
}

// --- the layouts -----------------------------------------------------------

// The children it holds, divided by the cell it declares. `w` and `h` are the share of the parent a
// layout takes; a row states the height it takes and a column the width.
int screenLayout(bvm* vm) {
  if (be_top(vm) < 3 || !isListValue(vm, 3)) {
    be_raise(vm, "type_error", "screen.layout(w, h, children[, alignment])");
  }
  requireChildren(vm, 3, "layout");
  const int draw = beginDraw(vm, "layout");
  setCellSide(vm, draw, "w", 1);
  setCellSide(vm, draw, "h", 2);
  setValue(vm, draw, "children", 3);
  setAlignment(vm, draw, 4);
  be_return(vm);
}

int screenRow(bvm* vm) {
  if (be_top(vm) < 2 || !isListValue(vm, 2)) {
    be_raise(vm, "type_error", "screen.row(h, children[, alignment])");
  }
  requireChildren(vm, 2, "row");
  const int draw = beginDraw(vm, "row");
  setCellSide(vm, draw, "h", 1);
  setValue(vm, draw, "children", 2);
  setAlignment(vm, draw, 3);
  be_return(vm);
}

int screenColumn(bvm* vm) {
  if (be_top(vm) < 2 || !isListValue(vm, 2)) {
    be_raise(vm, "type_error", "screen.column(w, children[, alignment])");
  }
  requireChildren(vm, 2, "column");
  const int draw = beginDraw(vm, "column");
  setCellSide(vm, draw, "w", 1);
  setValue(vm, draw, "children", 2);
  setAlignment(vm, draw, 3);
  be_return(vm);
}

// --- the answers that are values, not trees --------------------------------

// A count of seconds as the text the duration node would draw, for a page that centres or measures it.
int screenDurationText(bvm* vm) {
  if (be_top(vm) < 2) {
    be_raise(vm, "type_error", "screen.duration_text(format, seconds[, known])");
  }
  const char* format = argFormat(vm, 1, "duration_text");
  const bool known = be_top(vm) < 3 || be_isnil(vm, 3) || be_tobool(vm, 3);
  char out[16];
  be_pushstring(vm, gfx::durationText(formatOf(format), (int32_t)argInt(vm, 2), known, out,
                                          sizeof(out)));
  be_return(vm);
}

// The ink a string takes at the face it will be drawn in, which is what a page places a suffix by.
int screenMeasure(bvm* vm) {
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) {
    be_raise(vm, "type_error", "screen.measure(value[, font[, scale]])");
  }
  const gfx::Font* face = argFace(vm, 2, "measure");
  const uint8_t scale = argScale(vm, 3, "measure", 1);
  const gfx::Size box = gfx::shapes::measureText(face, argText(vm, 1), scale);

  const int record = beginRecord(vm, "bounds");
  setInt(vm, record, "w", box.width);
  setInt(vm, record, "h", box.height);
  be_return(vm);
}

// The pixels a declared share takes of a total: nil for the whole, a share rounded to the nearest
// pixel, or a count of pixels clamped to the total. The same rule the layout draws by.
int screenCell(bvm* vm) {
  if (be_top(vm) < 2) {
    be_raise(vm, "type_error", "screen.cell(total, declared)");
  }
  be_pushint(vm, gfx::resolveSpan((int16_t)argInt(vm, 1), (float)argReal(vm, 2)));
  be_return(vm);
}

// The offset a justification takes of the room left over: none at the start, half at the centre and
// all of it at the end.
int screenAlign(bvm* vm) {
  if (be_top(vm) < 2) {
    be_raise(vm, "type_error", "screen.align(leftover, where)");
  }
  const int16_t leftover = (int16_t)argInt(vm, 1);
  const char* where = be_isstring(vm, 2) ? argText(vm, 2) : nullptr;
  gfx::Justify justify = gfx::Justify::Start;
  if (where != nullptr && strcmp(where, "center") == 0) {
    justify = gfx::Justify::Center;
  } else if (where != nullptr && strcmp(where, "end") == 0) {
    justify = gfx::Justify::End;
  } else if (where == nullptr || strcmp(where, "start") != 0) {
    be_raise(vm, "value_error", "screen.align: start, center or end expected");
  }
  be_pushint(vm, gfx::alignedOffset(leftover, justify));
  be_return(vm);
}

// The panel's size, which is what a page's outermost box is.
int screenWidth(bvm* vm) {
  be_pushint(vm, bindings->display().width());
  be_return(vm);
}

int screenHeight(bvm* vm) {
  be_pushint(vm, bindings->display().height());
  be_return(vm);
}

const bnfuncinfo ScreenLibrary[] = {
    {"text", screenText},
    {"centred", screenCentred},
    {"icon", screenIcon},
    {"inverted", screenInverted},
    {"bar", screenBar},
    {"scrollbar", screenScrollbar},
    {"duration", screenDuration},
    {"pressure_readout", screenPressureReadout},
    {"layout", screenLayout},
    {"row", screenRow},
    {"column", screenColumn},
    {"duration_text", screenDurationText},
    {"measure", screenMeasure},
    {"cell", screenCell},
    {"align", screenAlign},
    {"width", screenWidth},
    {"height", screenHeight},
    {nullptr, nullptr},
};

}  // namespace

// The module a page describes its drawing through, and the three records it answers with: the node
// every call builds, the ink box `measure` answers, and where a layout puts what it holds.
void installScreenModule(bvm* vm) {
  installRecord(vm, DrawClass, DrawFields);
  installRecord(vm, "bounds", BoundsFields);
  installRecord(vm, "alignment", AlignmentFields);
  installModule(vm, "screen", ScreenLibrary);
}

}  // namespace berry
}  // namespace gardener
