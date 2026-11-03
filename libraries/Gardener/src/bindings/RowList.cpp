#include <string.h>

#include <utility>
#include <vector>

#include <Arduino.h>
#include <berry.h>

#include "BindingContext.h"
#include "DrawTree.h"
#include "Module.h"
#include "Modules.h"
#include "core/SystemState.h"
#include "shapes/RowList.h"

namespace gardener {
namespace {

// The global class a page builds a list with. The model lives in the instance's comobj, the way a
// setting's row does, and the pairs the page handed stay in `.rows` for as long as the instance
// does, which is what keeps a row's borrowed text where the interpreter put it.
constexpr const char* ClassName = "RowList";

int listRelease(bvm* vm) {
  delete static_cast<gfx::RowList*>(be_tocomptr(vm, 1));
  be_return_nil(vm);
}

// The model an instance holds, borrowed: the comobj is what carries a C++ object into the image,
// and the box is what the interpreter frees, never the model in it.
gfx::RowList* modelOf(bvm* vm, int index) {
  const int base = be_top(vm);
  gfx::RowList* list = nullptr;
  if (be_getmember(vm, index, ".p") && be_iscomobj(vm, -1)) {
    list = static_cast<gfx::RowList*>(be_tocomptr(vm, -1));
  }
  be_pop(vm, be_top(vm) - base);
  return list;
}

// Whether the value at `index` is a list a page built: an instance of the list class, which is what
// `[...]` makes, or the interpreter's own. Its items are left on top when it is.
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

// The messages below are stack buffers because be_raise longjmps past any C++ destructor a String
// would need. Every check runs before the first String is built, so a refused call leaves nothing
// behind.

// The `element` of the pair at `pair`, left on the stack.
void pushElement(bvm* vm, int pair, int element) {
  be_pushint(vm, element);
  be_getindex(vm, pair);
  be_remove(vm, -2);  // the index the read left under the element
}

bool isJustify(const char* word) {
  return strcmp(word, "start") == 0 || strcmp(word, "center") == 0 || strcmp(word, "end") == 0;
}

gfx::Justify justifyOf(const char* word) {
  if (strcmp(word, "start") == 0) return gfx::Justify::Start;
  if (strcmp(word, "end") == 0) return gfx::Justify::End;
  return gfx::Justify::Center;
}

// The pair at `pair` as a row: a value and what draws it — a title, with an optional bitmap in the
// columns before it and an optional alignment for the title — or a draw record. Anything else is
// refused at the call rather than drawn as nothing.
void requireRow(bvm* vm, int pair) {
  const int count = be_data_size(vm, pair);
  if (count < 2 || count > 4) {
    be_raise(vm, "type_error", "RowList: a row is [value, draw] or [value, title, bitmap, align]");
  }

  pushElement(vm, pair, 0);
  const bool value = be_isstring(vm, -1);
  be_pop(vm, 1);
  if (!value) be_raise(vm, "type_error", "RowList: a row's value is a string");

  pushElement(vm, pair, 1);
  const bool title = be_isstring(vm, -1);
  const bool draw = title || be_isinstance(vm, -1);
  be_pop(vm, 1);
  if (!draw) be_raise(vm, "type_error", "RowList: a row's draw is a draw record or a title");

  // A draw record is a pair and nothing more: a bitmap and an alignment are a title's.
  if (!title) {
    if (count > 2) be_raise(vm, "type_error", "RowList: a draw row is a pair");
    return;
  }
  if (count >= 3) {
    pushElement(vm, pair, 2);
    const bool bitmap = be_isnil(vm, -1) || be_isstring(vm, -1);
    be_pop(vm, 1);
    if (!bitmap) be_raise(vm, "type_error", "RowList: a row's bitmap is a string");
  }
  if (count >= 4) {
    pushElement(vm, pair, 3);
    const bool align =
        be_isnil(vm, -1) || (be_isstring(vm, -1) && isJustify(argText(vm, -1)));
    be_pop(vm, 1);
    if (!align) be_raise(vm, "value_error", "RowList: an alignment is start, center or end");
  }
}

// One row from the pair's items at `pair`: the value, and the paint the second element describes.
// A title row is built by the model's own helper; a draw record is walked into a paint the list
// keeps, taken from the state the list was built with.
gfx::RowList::Row rowOf(bvm* vm, int pair, const SystemState& state) {
  pushElement(vm, pair, 0);
  const String value(argText(vm, -1));
  be_pop(vm, 1);

  pushElement(vm, pair, 1);
  const int draw = be_absindex(vm, -1);
  if (!be_isstring(vm, draw)) {
    gfx::PaintNodeRef paint = paintFromTree(vm, draw, state);
    be_pop(vm, 1);
    return gfx::RowList::Row{value, std::move(paint)};
  }
  const String title(argText(vm, draw));
  be_pop(vm, 1);

  String bitmap;
  if (be_data_size(vm, pair) >= 3) {
    pushElement(vm, pair, 2);
    if (be_isstring(vm, -1)) bitmap = String(argText(vm, -1));
    be_pop(vm, 1);
  }
  gfx::Justify along = gfx::Justify::Center;
  if (be_data_size(vm, pair) >= 4) {
    pushElement(vm, pair, 3);
    if (be_isstring(vm, -1)) along = justifyOf(argText(vm, -1));
    be_pop(vm, 1);
  }
  if (bitmap.isEmpty()) return gfx::RowList::textRow(value, title, along);
  return gfx::RowList::textRow(value, title, bitmap, along);
}

// RowList(row_height, rows, selected, wrap): the pairs a page built, the value the keys start on —
// nil for none — and the word that says whether the ends ring or stop. The rows are checked whole
// before any of them is built, so a refused call leaves nothing behind.
int rowListInit(bvm* vm) {
  BindingContext* bindings = boundContext(vm);
  if (bindings == nullptr) be_raise(vm, "runtime_error", "RowList: no panel");
  if (be_top(vm) < 5) {
    be_raise(vm, "type_error", "RowList: init(row_height, rows, selected, wrap) expected");
  }
  if (!be_isnumber(vm, 2) || be_toint(vm, 2) < 1) {
    be_raise(vm, "value_error", "RowList: row_height is a whole number of at least one");
  }
  if (!pushItems(vm, 3)) be_raise(vm, "type_error", "RowList: rows is a list of rows");
  const int list = be_absindex(vm, -1);
  const int count = be_data_size(vm, list);
  for (int i = 0; i < count; i++) {
    be_pushint(vm, i);
    be_getindex(vm, list);
    be_remove(vm, -2);  // the index the read left under the row
    if (!pushItems(vm, -1)) be_raise(vm, "type_error", "RowList: every row is a list");
    requireRow(vm, be_absindex(vm, -1));
    be_pop(vm, 2);  // the row's items and the row
  }
  if (!be_isnil(vm, 4) && !be_isstring(vm, 4)) {
    be_raise(vm, "type_error", "RowList: selected is a string or nil");
  }
  const char* wrap = be_isstring(vm, 5) ? argText(vm, 5) : nullptr;
  if (wrap == nullptr || (strcmp(wrap, "wrap") != 0 && strcmp(wrap, "clamp") != 0)) {
    be_raise(vm, "value_error", "RowList: wrap is \"wrap\" or \"clamp\"");
  }

  const SystemState state = bindings->systemState();
  std::vector<gfx::RowList::Row> rows;
  rows.reserve((size_t)count);
  for (int i = 0; i < count; i++) {
    be_pushint(vm, i);
    be_getindex(vm, list);
    be_remove(vm, -2);
    pushItems(vm, -1);
    rows.push_back(rowOf(vm, be_absindex(vm, -1), state));
    be_pop(vm, 2);  // the row's items and the row
  }
  be_pop(vm, 1);  // the rows' items

  const bool wraps = strcmp(wrap, "clamp") != 0;
  const String selected = be_isnil(vm, 4) ? String() : String(argText(vm, 4));
  be_newcomobj(vm, new gfx::RowList((int16_t)be_toint(vm, 2), std::move(rows), selected, wraps),
               listRelease);
  be_setmember(vm, 1, ".p");
  be_pop(vm, 1);
  // The pairs stay on the instance for as long as it lives: the strings a row's paint borrows are
  // the page's own, and the record keeps them where the interpreter put them.
  be_pushvalue(vm, 3);
  be_setmember(vm, 1, "rows");
  be_pop(vm, 1);
  be_return_nil(vm);
}

// nudge(direction): one row in the direction's sign, answering whether the current row changed.
int rowListNudge(bvm* vm) {
  if (be_top(vm) < 2 || !be_isnumber(vm, 2)) {
    be_raise(vm, "type_error", "RowList.nudge(direction)");
  }
  gfx::RowList* list = modelOf(vm, 1);
  be_pushbool(vm, list != nullptr && list->nudge((int)be_toint(vm, 2)));
  be_return(vm);
}

// getSelectedValue(): the value the keys are on, or nil when no row holds it — a `nil` selection
// and a value no row names both answer nil.
int rowListSelected(bvm* vm) {
  gfx::RowList* list = modelOf(vm, 1);
  if (list == nullptr || list->current() < 0) be_return_nil(vm);
  be_pushstring(vm, list->selected().c_str());
  be_return(vm);
}

// draw(): the node the page hands its drawing, naming this instance.
int rowListDraw(bvm* vm) {
  pushRowListDraw(vm, 1);
  be_return(vm);
}

const bnfuncinfo RowListMembers[] = {
    {".p", nullptr},             {"rows", nullptr},
    {"init", rowListInit},       {"nudge", rowListNudge},
    {"getSelectedValue", rowListSelected}, {"draw", rowListDraw},
    {nullptr, nullptr},
};

}  // namespace

void installRowListClass(bvm* vm, BindingContext& bindings) {
  pushClass(vm, ClassName, RowListMembers, bindings);
  be_setglobal(vm, ClassName);
  // be_setglobal leaves the value it stored on the stack; the class is done with here.
  be_pop(vm, 1);
}

}  // namespace gardener
