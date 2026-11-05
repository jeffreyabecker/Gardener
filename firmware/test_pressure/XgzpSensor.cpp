// The driver itself, compiled into this sketch rather than copied beside it: the firmware's own
// translation unit, so what the bench checks is what the device runs. It is named by a library
// path, which is what build_opt.h beside this file puts on the include path.
#include "hal/impl/XgzpSensor.cpp"
