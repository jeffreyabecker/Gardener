#pragma once

#include <WString.h>

#include <cstring>
#include <utility>

#include "core/SharedList.h"

// A map of string keys to String values: one writer, any number of readers.
//
// The backing store is a SharedList, so a Handle is an immutable snapshot. It keeps the chain
// it took, and the writer can set, clear or publish a whole new map without invalidating it.
// A lookup is a linear scan, which is the right shape for the handful of named values a reply
// or the panel reads.
//
// Keys are borrowed, not copied. A const char* has to outlive every Handle that can still
// reach the entry, so keys are string literals or other static storage; a String built at
// runtime and passed as a key would dangle.
//
// Later entries shadow earlier ones, so set() is a persistent update. A whole rebuild is a
// scratch map filled with set() and published in one step, which is what a reader should see
// instead of the intermediate states of a clear-then-set loop.
namespace gardener {

template <typename Lock>
class SharedMap {
 public:
  struct Entry {
    const char* key;
    String value;

    Entry(const char* k, const String& v) : key(k), value(v) {}
    Entry(const char* k, String&& v) : key(k), value(std::move(v)) {}
  };

  using List = SharedList<Entry, Lock>;

  class Handle {
   public:
    Handle() = default;

    explicit operator bool() const { return static_cast<bool>(list_); }

    // Null when the key is absent.
    const String* find(const char* key) const {
      if (key == nullptr) return nullptr;
      for (const Entry& entry : list_) {
        if (entry.key != nullptr && std::strcmp(entry.key, key) == 0) return &entry.value;
      }
      return nullptr;
    }

    bool has(const char* key) const { return find(key) != nullptr; }

    // Empty when the key is absent.
    String get(const char* key) const {
      const String* value = find(key);
      return value ? *value : String();
    }

    using Iterator = typename List::Handle::Iterator;

    Iterator begin() const { return list_.begin(); }
    Iterator end() const { return list_.end(); }

   private:
    friend class SharedMap;

    explicit Handle(typename List::Handle list) : list_(std::move(list)) {}

    typename List::Handle list_;
  };

  SharedMap() = default;
  SharedMap(const SharedMap&) = delete;
  SharedMap& operator=(const SharedMap&) = delete;

  // Writer: prepend an entry, shadowing an earlier set of the same key. False when the key is
  // null or the entry cannot be allocated.
  bool set(const char* key, const String& value) {
    return key != nullptr && list_.pushFront(key, value);
  }

  bool set(const char* key, String&& value) {
    return key != nullptr && list_.pushFront(key, std::move(value));
  }

  // Writer: unlink everything. Open Handles keep their chains.
  void clear() { list_.clear(); }

  // Writer: adopt `next`'s entries as this map's contents and leave `next` empty, ready to be
  // filled again. Readers see either the old map or the new one, never the rebuild. `next`
  // must not be written concurrently.
  void publish(SharedMap& next) { list_.replace(next.list_); }

  // Reader: take an immutable view, then look up or iterate without the lock.
  Handle snapshot() const { return Handle(list_.snapshot()); }

  // Reader: whether the map is empty at this instant.
  bool empty() const { return list_.empty(); }

 private:
  List list_;
};

}  // namespace gardener
