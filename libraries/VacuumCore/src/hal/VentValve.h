#pragma once

namespace gardener {

// The vent valve: the only way back up, since the pump can pull down and nothing else can.
//
// This is the interface the plant holds. The part on this rig is a normally open solenoid on a
// GPIO, in hal/impl/, and that inversion is the rig's one hardware guarantee: de-energised means
// released, so a crash, a brownout or a cut wire vents the chamber whether or not the firmware
// noticed. It is worth saying out loud here because it is what makes `close()` the act of running a
// vacuum rather than `open()`.
//
// Vent *timing* is the run model's, not the valve's: this only drives the pin.
class VentValve {
 public:
  virtual ~VentValve() = default;

  virtual void begin() = 0;
  virtual void open() = 0;   // release the coil: the chamber vents
  virtual void close() = 0;  // energise it: the chamber can be pumped
  virtual bool energised() const = 0;
};

}  // namespace gardener
