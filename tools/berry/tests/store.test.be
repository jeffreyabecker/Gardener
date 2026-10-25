# store.test.be — what a document promises beyond one key at a time: it holds every pair its text
# holds, hands them all back, and takes a new key whatever the count.

suite = "store"

def a_document_holds_every_pair_its_text_holds()
  var values = {}
  var i = 0
  while i < 300
    values["key" + str(i)] = str(i)
    i += 1
  end
  assert_true(store.set("store_test", values), "a document of 300 keys would not take them")

  var all = store.get("store_test")
  assert_eq(all.size(), 300, "a document of 300 keys does not read back every key")
  assert_eq(all["key0"], "0", "the first key of a long document is not its value")
  assert_eq(all["key299"], "299", "the last key of a long document is not its value")

  assert_true(store.set("store_test", {"key300": "300"}), "a key past 300 would not be added")
  all = store.get("store_test")
  assert_eq(all.size(), 301, "a key added past 300 is not kept with the rest")
  assert_eq(all["key300"], "300", "the key added past 300 is not its value")

  var typed = store.get("store_test", {"key299": 0})
  assert_eq(typed["key299"], 299, "a key past 255 is not read back as its default's type")
end

test("a document holds every pair its text holds", a_document_holds_every_pair_its_text_holds)
