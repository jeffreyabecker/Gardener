#pragma once

namespace gardener {

// The vent valve: the only way back toward ambient, since the pump can pull down and nothing else
// can.
//
// This is the interface the vacuum system holds. The verbs are the chamber's, not the coil's: `open()`
// vents and `close()` makes the chamber pumpable, whichever way the fitted solenoid happens to wire
// them. The part on this rig is a normally open solenoid on a GPIO, in hal/impl/, so de-energised
// means vented and a crash, a brownout or a cut wire vents the chamber whether or not the firmware
// noticed — the rig's one hardware guarantee. A normally closed part drives the same interface but
// inverts that relationship, so it only earns its place when holding vacuum through a power loss is
// what you want.
//
// Vent *timing* is the run model's, not the valve's: this only drives the pin.
class VentValve {
 public:
  virtual ~VentValve() = default;

  virtual void begin() = 0;
  virtual void open() = 0;   // vent: the chamber returns to ambient
  virtual void close() = 0;  // seal: the chamber can be pumped
  virtual bool energised() const = 0;
  // True when energising the coil opens the valve (normally closed), false when it closes it
  // (normally open). The one fact about the fitted part the boot report needs.
  virtual bool energiseWhenOpen() const = 0;
};

}  // namespace gardener
