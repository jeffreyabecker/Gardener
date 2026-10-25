#include "Build.h"

#if GARDENER_PANEL

#include <berry.h>

#include <vector>

#include "PanelContext.h"
#include "routines/bindings/Module.h"
#include "routines/bindings/Modules.h"
#include "storage/Config.h"

namespace gardener {
namespace berry {
namespace {

// This Berry has no userdata slot, so the instance's `.p` member holds the Config as a comobj: the
// value owns the C++ object and the interpreter frees it with the instance. Nothing a page can call
// releases the document while a caller still holds the instance.
int configRelease(bvm* vm) {
  delete static_cast<Config*>(be_tocomptr(vm, 1));
  be_return_nil(vm);
}

// The document this instance wraps. A method that runs without one is a broken call rather than a
// missing preference.
Config* configOf(bvm* vm) {
  if (!be_getmember(vm, 1, ".p") || !be_iscomobj(vm, -1)) {
    be_pop(vm, 1);
    be_raise(vm, "type_error", "config: not initialized");
    return nullptr;
  }
  Config* config = static_cast<Config*>(be_tocomptr(vm, -1));
  be_pop(vm, 1);
  return config;
}

// init: a Config over the panel's library, read once. A second init hands over a fresh document,
// dropping the first to the interpreter, so an instance never wraps two.
int configInit(bvm* vm) {
  if (context == nullptr) {
    be_raise(vm, "runtime_error", "config: no panel context");
  }
  Config* config = new Config(&context->library());
  config->begin();
  be_newcomobj(vm, config, configRelease);
  be_setmember(vm, 1, ".p");
  be_pop(vm, 1);
  be_return_nil(vm);
}

// config.get(section, key, fallback): the fallback is the type the text is read back as.
int configGet(bvm* vm) {
  if (be_top(vm) < 4 || !be_isstring(vm, 2) || !be_isstring(vm, 3)) {
    be_raise(vm, "type_error", "config.get(section, key, fallback)");
  }
  Config* config = configOf(vm);
  const char* section = argText(vm, 2);
  const char* key = argText(vm, 3);
  if (be_isbool(vm, 4)) {
    be_pushbool(vm, config->get<bool>(section, key, be_tobool(vm, 4)));
  } else if (be_isint(vm, 4)) {
    be_pushint(vm, config->get<int>(section, key, (int)be_toint(vm, 4)));
  } else if (be_isreal(vm, 4)) {
    be_pushreal(vm, config->get<float>(section, key, (float)be_toreal(vm, 4)));
  } else {
    const char* fallback = be_isstring(vm, 4) ? argText(vm, 4) : "";
    const String stored = config->get<String>(section, key, String(fallback));
    be_pushstring(vm, stored.c_str());
  }
  be_return(vm);
}

// config.set(section, key, value[, decimals]): the value decides the text form.
int configSet(bvm* vm) {
  if (be_top(vm) < 4 || !be_isstring(vm, 2) || !be_isstring(vm, 3)) {
    be_raise(vm, "type_error", "config.set(section, key, value)");
  }
  Config* config = configOf(vm);
  const char* section = argText(vm, 2);
  const char* key = argText(vm, 3);
  const uint8_t decimals =
      be_top(vm) >= 5 && be_isint(vm, 5) ? (uint8_t)be_toint(vm, 5) : 2;
  bool written = false;
  if (be_isbool(vm, 4)) {
    written = config->set(section, key, (bool)be_tobool(vm, 4));
  } else if (be_isint(vm, 4)) {
    written = config->set(section, key, (int)be_toint(vm, 4));
  } else if (be_isreal(vm, 4)) {
    written = config->set(section, key, (float)be_toreal(vm, 4), decimals);
  } else if (be_isstring(vm, 4)) {
    written = config->set(section, key, String(argText(vm, 4)));
  } else {
    be_raise(vm, "type_error", "config.set(section, key, value)");
  }
  be_pushbool(vm, written);
  be_return(vm);
}

// config.section_keys(): the non-empty section names, in document order.
int configSectionKeys(bvm* vm) {
  Config* config = configOf(vm);
  std::vector<String> keys;
  config->sectionKeys(keys);
  be_newobject(vm, "list");
  const int list = be_absindex(vm, -1);
  for (const String& key : keys) {
    be_pushstring(vm, key.c_str());
    be_data_push(vm, list);
    be_pop(vm, 1);
  }
  be_pop(vm, 1);
  be_return(vm);
}

const bnfuncinfo ConfigMembers[] = {
    {".p", nullptr},
    {"init", configInit},
    {"get", configGet},
    {"set", configSet},
    {"section_keys", configSectionKeys},
    {nullptr, nullptr},
};

}  // namespace

void installConfigClass(bvm* vm) {
  be_pushclass(vm, "Config", ConfigMembers);
  be_setglobal(vm, "Config");
  be_pop(vm, 1);
}

}  // namespace berry
}  // namespace gardener
#endif
