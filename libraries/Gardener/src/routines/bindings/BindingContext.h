#pragma once

#include <memory>

#include <Arduino.h>

namespace gardener
{

    struct MachineCommand;
    struct SystemState;
    class Logger;
    class RoutineRegistry;
    class Routines;

    namespace gfx
    {
        class DisplayDevice;
    }

    namespace panel
    {
        class GestureQueue;
        class PanelContext;
        namespace parts
        {
            class BuzzerPlayer;
        }
        namespace page
        {
            class Ux;
        }
    } // namespace panel

    namespace berry
    {

        // What the native modules reach of the app: one VM and one app, so a callback -- a plain C function
        // with no user data -- gets at the panel, the runtime behind the image and the device's log through
        // this, and no module names a global of its own. The accessors are the ones the modules need and
        // nothing more, so what a page can reach is this list rather than whatever a PanelContext or a
        // Routines happens to hold.
        class BindingContext
        {
        public:
            BindingContext(panel::PanelContext &context, Routines &routines, Logger &logger)
                : context_(context), routines_(routines), logger_(logger) {}

            // --- the panel's parts ---------------------------------------------------
            // The glass, the published state, the piezo, and the queue a command is posted to.
            gfx::DisplayDevice &display();
            SystemState &systemState();
            panel::parts::BuzzerPlayer &buzzer();
            bool post(const MachineCommand &command);

            // --- the runtime behind the image ----------------------------------------
            // The presses waiting for a page, the stack a launch pushes on, the routines the image registered,
            // and the queue the `device` module drains.
            panel::GestureQueue &presses();
            panel::page::Ux &ux();
            RoutineRegistry &registry();
            bool takePress(String &gesture);

            // --- the device's log ----------------------------------------------------
            void info(const char *message);
            void error(const char *message);

        private:
            panel::PanelContext &context_;
            Routines &routines_;
            Logger &logger_;
        };

        // The one install() builds, and nothing until it has: a module refuses a call made before the panel
        // runs, because the parts a scene, a row and a page are built from do not exist yet.
        extern std::unique_ptr<BindingContext> bindings;

    } // namespace berry
} // namespace gardener
