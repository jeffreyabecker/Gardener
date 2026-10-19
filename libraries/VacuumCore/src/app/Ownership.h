#pragma once

#include <stdint.h>

namespace gardener {

// The transports that can ask to drive the machine. `None` is the record's empty state, not a link.
enum class Link : uint8_t { None, Serial, Bluetooth };

// Who may change the machine. Connecting is not controlling: a link that only connects can read,
// and the panel and the machine belong to whoever holds this record.
//
// One controller at a time. A link that drops while owning leaves the record held but stale: the
// machine keeps running and the panel keeps showing that link, while a reconnected link can only
// read until the local reset clears the record.
class Ownership {
 public:
  // True only for the live owner. A stale record answers false for the link that left it, which
  // is what makes a reconnected link a read-only observer.
  bool ownedBy(Link link) const { return owner_ == link && !stale_; }

  // True while any link owns the machine, including one that has dropped and left it stale. The
  // panel uses this, because the machine is still that link's responsibility until the reset.
  bool held() const { return owner_ != Link::None; }

  Link owner() const { return owner_; }

  // True when `link` owns the device after the call. Acquiring again as the owner is idempotent;
  // any other link — or a stale record — is refused as busy.
  bool acquire(Link link) {
    if (link == Link::None) return false;
    if (ownedBy(link)) return true;
    if (held()) return false;
    owner_ = link;
    return true;
  }

  // True when `link` owned it and the caller must now release the chamber. A release from anyone
  // else, including after a drop, is a no-op: only reset() clears a held record.
  bool release(Link link) {
    if (!ownedBy(link)) return false;
    owner_ = Link::None;
    return true;
  }

  // The transport went away. Ownership is not released; the record stays held and cannot be
  // acquired again until reset().
  void drop(Link link) {
    if (owner_ == link) stale_ = true;
  }

  // The local reset is the only thing that clears a held record. A fresh boot starts empty.
  void reset() {
    owner_ = Link::None;
    stale_ = false;
  }

 private:
  Link owner_ = Link::None;
  bool stale_ = false;
};

}  // namespace gardener
