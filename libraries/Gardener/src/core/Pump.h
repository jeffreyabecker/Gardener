#pragma once

namespace gardener {

// The pump: the one actuator the control loop has, so the vacuum system can pull down and never
// vent. It is a switch and nothing more: the fitted diaphragm pump does not meter, so the loop runs
// it flat out or not at all, and the chamber's leak is what takes the depth back down.
//
// This is the interface the vacuum system holds. What drives a pump on this rig — one GPIO into a
// low side MOSFET — is in core/hal/.
class Pump {
 public:
  virtual ~Pump() = default;

  virtual void begin() = 0;
  virtual void on() = 0;
  virtual void off() = 0;

  // True while the pump is being driven. It is what the loop reads to know where it left the
  // actuator, and what the wire reports as the duty.
  virtual bool active() const = 0;
};

}  // namespace gardener