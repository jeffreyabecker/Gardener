#include "hosts/script/NullRuntime.h"

#include "core/Log.h"

namespace eggplan {

bool NullRuntime::start(const char* programName, const String& source, String& error) {
  (void)programName;
  (void)source;
  error = "firmware built without the Berry runtime";
  LOGW("runtime: %s", error.c_str());
  return false;
}

void NullRuntime::stop(const char* reason) { (void)reason; }

bool NullRuntime::evaluate(const String& line, String& out) {
  (void)line;
  out = "firmware built without the Berry runtime";
  return false;
}

}  // namespace eggplan
