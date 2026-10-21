#pragma once

#include <stdint.h>

namespace gardener {

// The links that can drive the machine, newest claim on top. `None` is the floor — it sits at the
// bottom and vents — and a panel build pushes `Panel` above it at boot, so the panel is the
// default controller beneath every external link.
enum class Link : uint8_t { None, Serial, Bluetooth, Panel };

// The machine's controller, read as a stack: whoever is on top drives, and a claim, a release or a
// drop changes the top, which vents the chamber. `None` is always the floor — an empty stack is
// the idle owner, and its only effect is to vent.
//
// Serial and Bluetooth push themselves when they claim and are removed when they release or drop;
// a panel build pushes the panel at boot. There is no busy and no stale record: a claim always
// succeeds, and a vanished link is simply gone.
class Ownership {
 public:
  static constexpr uint8_t MaxLinks = 3;  // serial, bluetooth, panel; None is the implicit floor

  // True when `link` is on top: the only link that commands the machine.
  bool ownedBy(Link link) const { return top() == link; }

  // True while an external link is on top: the panel is not driving. The panel's pages and the
  // screen follow this rather than the cable.
  bool external() const { return top() == Link::Serial || top() == Link::Bluetooth; }

  // The top of the stack: who drives the machine. None when nobody does.
  Link owner() const { return top(); }

  // A claim always succeeds: the link goes on top unless it is already there. The floor cannot be
  // claimed.
  bool acquire(Link link) {
    if (link == Link::None) return false;
    if (indexOf(link) >= 0) return true;
    if (count_ >= MaxLinks) return false;
    links_[count_++] = link;
    return true;
  }

  // A release removes the link wherever it sits; the one beneath is the controller again.
  bool release(Link link) {
    if (link == Link::None) return false;
    return remove(link);
  }

  // The transport went away. The link is removed, so it cannot leave the machine held.
  void drop(Link link) { remove(link); }

 private:
  Link top() const { return count_ == 0 ? Link::None : links_[count_ - 1]; }

  int8_t indexOf(Link link) const {
    for (uint8_t i = 0; i < count_; ++i) {
      if (links_[i] == link) return static_cast<int8_t>(i);
    }
    return -1;
  }

  bool remove(Link link) {
    const int8_t index = indexOf(link);
    if (index < 0) return false;
    for (uint8_t i = static_cast<uint8_t>(index); i + 1 < count_; ++i) {
      links_[i] = links_[i + 1];
    }
    --count_;
    return true;
  }

  // The stack above the floor, bottom first. Written on the link tasks and read on the control and
  // panel beats; a stale top reads as a spurious vent, which is safe, so no lock is worth it.
  Link links_[MaxLinks] = {};
  uint8_t count_ = 0;
};

inline const char* linkName(Link link) {
  switch (link) {
    case Link::Serial: return "serial";
    case Link::Bluetooth: return "bluetooth";
    case Link::Panel: return "panel";
    default: return "none";
  }
}

}  // namespace gardener
