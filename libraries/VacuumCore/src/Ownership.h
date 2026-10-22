#pragma once

#include <stdint.h>

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

  // The top of the stack: who drives the machine.
  Link& owner() const { return *links_[count_ - 1]; }

  // True when `link` is on top: the only link that commands the machine.
  bool ownedBy(const Link& link) const { return &link == links_[count_ - 1]; }

  // A claim moves the link to the top, wherever it sat. The floor cannot be claimed, and there is
  // always a slot for a link the build has, so a claim from anything else succeeds.
  bool acquire(Link& link) {
    if (&link == &none_) return false;
    remove(link);
    if (count_ >= MaxLinks) return false;
    links_[count_++] = &link;
    link.claimed();
    return true;
  }

  // Only the link on top may leave; the one beneath is the controller again. False when it is not
  // on top, so a transport that has gone keeps asking until it gets there.
  bool release(Link& link) {
    if (&link == &none_ || !ownedBy(link)) return false;
    --count_;
    return true;
  }

 private:
  bool remove(Link& link) {
    for (uint8_t i = 0; i < count_; ++i) {
      if (links_[i] != &link) continue;
      for (uint8_t j = i; j + 1 < count_; ++j) links_[j] = links_[j + 1];
      --count_;
      return true;
    }
    return false;
  }

  // The stack, bottom first. Written on the link tasks and read on the control and panel beats; a
  // stale top reads as a spurious vent, which is safe, so no lock is worth it.
  NoneLink none_;
  Link* links_[MaxLinks] = {&none_};
  uint8_t count_ = 1;
};

}  // namespace gardener
