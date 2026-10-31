# device.test.be — the device module's own answers: the record `device.display_unit()` presents
# the panel's unit as, and the class behind it. Both are answers across the Berry boundary, so the
# host unit suite cannot see them: the module needs the interpreter to exist at all.

suite = "device"

def display_unit_answers_the_documents_row()
  # The host session boots with no document of its own, so the unit is the built-in kPa row.
  var unit = device.display_unit()
  assert_eq(unit.name, "kpa", "the unit does not carry its wire token")
  assert_eq(unit.suffix, "kPa", "the unit does not carry the suffix the glass prints")
  assert_eq(unit.per_kpa, 1.0, "kPa is not the whole conversion it is")
  assert_eq(unit.whole_step, 1.0, "the whole step is not the row's")
  assert_eq(unit.sub_step, 0.1, "the fine step is not the row's")
  # A tenth is what the fine step can express, so the panel prints one decimal.
  assert_eq(unit.decimals, 1, "a tenth step does not print one decimal")
end

test("display_unit answers the document's row", display_unit_answers_the_documents_row)

def the_record_is_a_class_a_page_can_build()
  # The getter is not the only way to one: the class is a global, so a page that wants a unit of its
  # own builds the same record. A record with no constructor fails here rather than at the call.
  var unit = DisplayUnit()
  assert_nil(unit.name, "a record the page built carries a name")
  assert_nil(unit.decimals, "a record the page built carries a precision")
end

test("the record is a class a page can build", the_record_is_a_class_a_page_can_build)
