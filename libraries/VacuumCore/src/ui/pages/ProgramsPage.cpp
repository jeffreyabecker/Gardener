#include <stdio.h>

#include "ui/Renderer.h"
#include "ui/pages/Pages.h"

namespace eggplan {
namespace pages {

void renderPrograms(Renderer& renderer, const SystemSnapshot& snapshot, size_t cursor) {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "PROGRAMS %u", (unsigned)snapshot.programCount);
  renderer.header(buffer, snapshot.bleConnected);

  if (snapshot.programCount == 0 || snapshot.programs == nullptr) {
    renderer.line(0, "no programs stored");
    renderer.line(2, "upload over BLE:");
    renderer.line(3, "  prog characteristic");
    return;
  }

  const uint8_t rows = 5;
  size_t first = 0;
  if (cursor >= rows) first = cursor - rows + 1;

  for (uint8_t row = 0; row < rows; row++) {
    size_t index = first + row;
    if (index >= snapshot.programCount) break;
    const ProgramInfo& info = snapshot.programs[index];
    snprintf(buffer, sizeof(buffer), "%c%-12s%5u", index == cursor ? '>' : ' ',
             info.name, (unsigned)info.size);
    renderer.line(row, buffer);
  }

  renderer.footer("OK=run  BACK=stop");
}

}  // namespace pages
}  // namespace eggplan
