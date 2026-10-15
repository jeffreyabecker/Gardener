#pragma once

// Umbrella header for the Eggplan Vacuum firmware core.
#include "app/App.h"
#include "core/AppContext.h"
#include "hosts/ble/BleService.h"
#include "hal/Buttons.h"
#include "ui/DisplayUI.h"
#include "core/Log.h"
#include "hal/PressureSensor.h"
#include "services/ProgramStore.h"
#include "hosts/commands/Protocol.h"
#include "hal/Pump.h"
#include "hosts/script/ScriptRuntime.h"
#include "core/Util.h"
#include "core/VacuumConfig.h"
#include "services/VacuumController.h"
