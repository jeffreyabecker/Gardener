#include "services/ProgramUploader.h"

#include <string.h>

#include "core/Log.h"
#include "services/ProgramStore.h"

namespace eggplan {
namespace {

// Frame grammar: exactly one opcode per write, see docs/ble-protocol.md.
constexpr uint8_t kOpBegin = 'B';  // <name>
constexpr uint8_t kOpData = 'D';   // <raw bytes>
constexpr uint8_t kOpEnd = 'E';    // <checksum:u32> <size:u32>
constexpr uint8_t kOpAbort = 'X';

String ok(const String& text) { return "ok " + text; }
String err(const String& text) { return "err " + text; }

}  // namespace

bool ProgramUploader::active() const { return store_.writing(); }

uint32_t ProgramUploader::bytes() const { return store_.writeBytes(); }

String ProgramUploader::handle(const uint8_t* data, size_t length) {
  if (length == 0) return err("empty frame");

  uint8_t opcode = data[0];
  const uint8_t* payload = data + 1;
  size_t payloadLength = length - 1;

  switch (opcode) {
    case kOpBegin: {
      String name((const char*)payload, payloadLength);
      name.trim();
      if (!ProgramStore::validName(name.c_str())) return err("invalid name");
      if (!store_.writeBegin(name.c_str())) return err("cannot start upload");
      chunks_ = 0;
      return ok("begin " + name);
    }

    case kOpData: {
      if (!store_.writing()) return err("no upload in progress");
      if (!store_.writeAppend(payload, payloadLength)) {
        store_.writeAbort();
        return err("write failed");
      }
      chunks_++;
      return ok("data " + String((unsigned)store_.writeBytes()));
    }

    case kOpEnd: {
      if (!store_.writing()) return err("no upload in progress");
      uint32_t checksum = 0;
      uint32_t size = 0;
      if (payloadLength >= 4) memcpy(&checksum, payload, 4);
      if (payloadLength >= 8) memcpy(&size, payload + 4, 4);

      uint32_t bytes = store_.writeBytes();
      String name = store_.writingName();
      String error;
      if (!store_.writeCommit(checksum, size, error)) return err(error);
      return ok("stored " + name + " " + String((unsigned)bytes));
    }

    case kOpAbort:
      abort("client aborted");
      return ok("aborted");

    default:
      return err("unknown opcode");
  }
}

void ProgramUploader::abort(const char* reason) {
  if (store_.writing()) {
    store_.writeAbort();
    LOGW("upload: aborted (%s)", reason ? reason : "?");
  }
  chunks_ = 0;
}

}  // namespace eggplan
