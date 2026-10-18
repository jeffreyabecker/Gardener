#pragma once

#include <Arduino.h>

#include "app/AppContext.h"
#include "app/StreamLogger.h"
#include "app/serial/SerialLink.h"


// The application loop: boot, the service order, the run log and the splash. It knows only the
// AppContext, so which parts are fitted is the build function's business.
//
// The device is a serial peripheral: the front panel keys are read on demand, but no key changes
// what the device does, so the loop services the devices, the control loop and the link, in that
// order. App is also the only place that reports anything: the context returns values, and the
// run and the keys are logged from here.

namespace gardener {

    class UserInterface {
    public:
        virtual ~UserInterface() = default;

        virtual bool begin() = 0;
        virtual void update() = 0;
    };

    class ConnectedUI : public UserInterface {
    public:
        ConnectedUI(AppContext context);

        bool begin() override;
        void update() override;
    };
}