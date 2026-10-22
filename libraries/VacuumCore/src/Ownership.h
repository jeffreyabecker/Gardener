#pragma once

#include <algorithm>
#include <stdint.h>
#include <vector>

namespace gardener {

// Something that can drive the machine. The stack holds these, and the control beat tells the one
// that gains control and the one that loses it, because that beat is the one place the vacuum
// system is written.
class Link {
 public:
  virtual ~Link() {}

  // The name `control get`, the logs and the panel's pages use; and the value the BLE `control`
  // characteristic reports: none 0, serial 1, bluetooth 2, panel 3.
  virtual const char* name() const = 0;
  virtual uint8_t code() const = 0;

  // The control beat tells the link when the stack makes it, or stops making it, the controller.
  // A link reacts here; the vent that a hand-over causes is the beat's, not the callback's.
  virtual void gainedControl() {}
  virtual void lostControl() {}

  // The stack's own word, said as the claim lands. A link that comes back and claims again is
  // still on the stack, so this is where it cancels a departure that has not been applied yet.
  virtual void claimed() {}
};

// The floor of the stack: nobody is driving. It is on the stack from construction and never moves,
// so `owner()` always has an answer, and a machine whose links have gone reads `none`.
class NoneLink : public Link {
 public:
  static constexpr uint8_t Code = 0;

  const char* name() const override { return "none"; }
  uint8_t code() const override { return Code; }
};

// The machine's controller, read as a stack: whoever is on top drives. `None` is always the floor;
// a panel build pushes the panel above it at boot, so the panel is the default controller, and
// serial and Bluetooth push themselves when they claim.
//
// Only the top may pop itself, so a link beneath another cannot take the machine from it. A claim
// always succeeds and moves the claimant to the top, so a repeated claim from the top leaves the
// stack as it was.
class Ownership {
 public:
  static constexpr uint8_t MaxLinks = 4;  // the floor, serial, bluetooth and the panel

  // The floor goes on first and never comes off, so `owner()` always has an answer.
  Ownership() {
    links_.reserve(MaxLinks);
    links_.push_back(&none_);
  }

  // The top of the stack: who drives the machine.
  Link& owner() const { return *links_.back(); }

  // True when `link` is on top: the only link that commands the machine.
  bool ownedBy(const Link& link) const { return &link == links_.back(); }

  // A claim moves the link to the top, wherever it sat. The floor cannot be claimed, and there is
  // always a slot for a link the build has, so a claim from anything else succeeds.
  bool acquire(Link& link) {
    if (&link == &none_) return false;
    remove(link);
    links_.push_back(&link);
    link.claimed();
    return true;
  }

  // Only the link on top may leave; the one beneath is the controller again. False when it is not
  // on top, so a transport that has gone keeps asking until it gets there.
  bool release(Link& link) {
    if (&link == &none_ || !ownedBy(link)) return false;
    links_.pop_back();
    return true;
  }

 private:
  // Drops the link if it is on the stack, keeping the order of the ones above it.
  void remove(Link& link) {
    auto found = std::find(links_.begin(), links_.end(), &link);
    if (found != links_.end()) links_.erase(found);
  }

  // The stack, bottom first. Written on the link tasks and read on the control and panel beats; a
  // stale top reads as a spurious vent, which is safe, so no lock is worth it.
  NoneLink none_;
  std::vector<Link*> links_;
};

}  // namespace gardener
