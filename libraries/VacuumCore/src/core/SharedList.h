#pragma once

#include <atomic>
#include <cstdint>
#include <new>
#include <utility>

// Single-writer, many-reader linked list.
//
// A node is immutable once it is published: pushFront links a new node in front of the head
// and never edits a node a reader can see. A reader takes a Handle and walks it without the
// lock, while the writer moves the head on. Reclamation is by intrusive reference count, so a
// snapshot stays valid after the writer has released everything.
//
// Lock is a policy with enter()/exit(): a critical section, not a mutex. It is held for the
// head swap and one reference count increment, and never for allocation, a walk or a
// destruction. enter() must have acquire semantics and exit() release semantics, which a
// FreeRTOS critical section does.
//
// Handles free their chain when the last copy is released, so releasing one is a task-context
// operation: dropping the last reference in an ISR would call free() from an ISR.
namespace gardener {

template <typename T, typename Lock>
class SharedList {
 private:
  struct Node {
    std::atomic<uint32_t> refs{1};  // the list head, or the previous node, owns this one
    T value;
    Node* next = nullptr;

    template <typename... Args>
    explicit Node(Args&&... args) : value(std::forward<Args>(args)...) {}

    void addRef() { refs.fetch_add(1, std::memory_order_relaxed); }
  };

 public:
  // An owning view of a node and everything after it. Copyable, so it can be handed to another
  // task; the chain lives as long as one copy does.
  class Handle {
   public:
    Handle() = default;

    Handle(const Handle& other) : node_(other.node_) {
      if (node_) node_->addRef();
    }

    Handle(Handle&& other) : node_(other.node_) { other.node_ = nullptr; }

    Handle& operator=(const Handle& other) {
      if (this != &other) {
        Node* keep = node_;
        node_ = other.node_;
        if (node_) node_->addRef();
        release(keep);
      }
      return *this;
    }

    Handle& operator=(Handle&& other) {
      if (this != &other) {
        Node* keep = node_;
        node_ = other.node_;
        other.node_ = nullptr;
        release(keep);
      }
      return *this;
    }

    ~Handle() { release(node_); }

    explicit operator bool() const { return node_ != nullptr; }

    // Null when the snapshot is empty.
    const T* front() const { return node_ ? &node_->value : nullptr; }

    class Iterator {
     public:
      explicit Iterator(const Node* node) : node_(node) {}

      const T& operator*() const { return node_->value; }
      const T* operator->() const { return &node_->value; }

      Iterator& operator++() {
        node_ = node_->next;
        return *this;
      }

      bool operator==(const Iterator& other) const { return node_ == other.node_; }
      bool operator!=(const Iterator& other) const { return node_ != other.node_; }

     private:
      const Node* node_;
    };

    Iterator begin() const { return Iterator(node_); }
    Iterator end() const { return Iterator(nullptr); }

   private:
    friend class SharedList;

    explicit Handle(Node* node) : node_(node) {}

    Node* node_ = nullptr;
  };

  SharedList() = default;
  SharedList(const SharedList&) = delete;
  SharedList& operator=(const SharedList&) = delete;

  ~SharedList() { clear(); }

  // Writer: link a new node in front. Returns false when the node cannot be allocated.
  // The node is complete before it becomes visible, so a reader never sees a partial value.
  template <typename... Args>
  bool pushFront(Args&&... args) {
    Node* node = new (std::nothrow) Node(std::forward<Args>(args)...);
    if (!node) return false;

    lock_.enter();
    node->next = head_;  // the list's reference on the old head moves into the new node
    head_ = node;
    lock_.exit();
    return true;
  }

  // Writer: unlink the head. A reader that already took a Handle keeps the chain it has.
  bool dropFront() {
    Node* old;
    lock_.enter();
    old = head_;
    if (old) {
      if (old->next) old->next->addRef();  // the list takes over the successor
      head_ = old->next;
    }
    lock_.exit();

    release(old);
    return old != nullptr;
  }

  // Writer: unlink everything. Open Handles keep their chains alive.
  void clear() {
    Node* old;
    lock_.enter();
    old = head_;
    head_ = nullptr;
    lock_.exit();

    release(old);
  }

  // Reader: take an owning snapshot, then walk it without the lock.
  Handle snapshot() const {
    lock_.enter();
    Node* node = head_;
    if (node) node->addRef();
    lock_.exit();

    return Handle(node);
  }

  // Reader: whether the list is empty at this instant.
  bool empty() const {
    lock_.enter();
    const bool none = head_ == nullptr;
    lock_.exit();
    return none;
  }

 private:
  static void release(Node* node) {
    while (node && node->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      Node* next = node->next;
      delete node;
      node = next;
    }
  }

  mutable Lock lock_;
  Node* head_ = nullptr;
};

}  // namespace gardener
