#pragma once

#include <stdint.h>

#include "core/Config.h"
#include "hal/PressureSensor.h"
#include "hal/Pump.h"
#include "hal/VentValve.h"

namespace eggplan {

// The plant: the pressure loop and the release path, between the drivers and the run.
//
// It is told what to do — regulate to this target, vent, rest — and it reports what is true:
// the pressure, the duty, whether a release has finished. It decides nothing. When to start,
// what a stop means, and whether a fault is permanent are run rules, and they live in
// VacuumControl.
//
// The loop is a plain PID on absolute pressure with the pump as the only actuator, so it can
// pull down but never vent; the valve is the only way back up, which is why every release
// path here stops the pump before it opens the valve. Gains live in core/Config.h.
class VacuumSystem {
 public:
  VacuumSystem(Pump& pump, PressureSensor& sensor, VentValve& vent)
      : pump_(pump), sensor_(sensor), vent_(vent) {}

  void begin();                  // pump off, valve open: the safe resting state
  void service(uint32_t nowMs);  // the PID step and the fault detection

  // --- the target ---------------------------------------------------------
  void setTarget(float kpa);
  void clearTarget();
  bool targetActive() const { return targetActive_; }
  float targetKpa() const { return targetKpa_; }
  // Close enough to the target that the loop has nothing left to do, deadband included.
  bool atTarget() const;

  // --- the loop and the release path --------------------------------------
  void start();  // close the valve, put the loop at zero, begin regulating
  void vent();   // stop, then open: the one release path
  void rest();   // the resting state without releasing: pump off, valve open
  bool vented() const;  // settled within VENT_MARGIN_KPA, or VENT_TIMEOUT_MS elapsed

  // --- what the chamber is doing ------------------------------------------
  float kpa() const { return sensor_.kpa(); }
  float vacuumKpa() const { return sensor_.vacuumKpa(); }
  float vacuumPercent() const { return sensor_.vacuumPercent(); }
  float ambientKpa() const { return sensor_.ambientKpa(); }
  bool sensorOk() const { return sensor_.present(); }
  bool readingValid() const { return sensor_.readingValid(); }
  float duty() const { return pump_.dutyPercent(); }

  // Deepest target the pump can actually reach: ambient minus its suction rating,
  // floored by the sanity limit.
  float minReachableKpa() const;

  // The reference "vacuum" is measured against, so the definition sits with the loop.
  bool setAmbientKpa(float kpa) { return sensor_.setAmbientKpa(kpa); }
  void captureAmbient() { sensor_.captureAmbient(); }

  // --- what went wrong ----------------------------------------------------
  // True once the plant has failed and its outputs are safe. The run decides that a fault
  // is permanent; this only says the pump is stopped and the chamber is open.
  bool faulted() const { return faulted_; }
  const char* faultCode() const { return pump_.faultCode(); }
  void resetFault();

 private:
  // Three values rather than two booleans, because regulating and venting at once is not a
  // state this plant can be in.
  enum class Activity : uint8_t { Resting, Regulating, Venting };

  void regulate(uint32_t nowMs);
  void fail(const char* code);

  void pidReset() { pidIntegral_ = 0.0f; }
  float pidUpdate(float dt, float error, float measurementSlew);

  Pump& pump_;
  PressureSensor& sensor_;
  VentValve& vent_;

  Activity activity_ = Activity::Resting;
  bool targetActive_ = false;
  float targetKpa_ = 0.0f;
  bool faulted_ = false;
  uint32_t ventStartMs_ = 0;

  float pidIntegral_ = 0.0f;
  float lastMeasured_ = 0.0f;
  float output_ = 0.0f;
  uint32_t lastControlMs_ = 0;
  uint32_t lastDerivativeMs_ = 0;
};

}  // namespace eggplan
