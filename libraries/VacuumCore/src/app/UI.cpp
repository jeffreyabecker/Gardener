#include "UI.h"
#include "eggplant-bitmap.h"

namespace gardener {

    ConnectedUI::ConnectedUI(AppContext context) : UserInterface(context) {}

    bool ConnectedUI::begin() {
        context_.display().clear();
        context_.display().bitmap(0, 0, splashBitmap, 128, 64);
        return true;
    }

    void ConnectedUI::update() {
        // Implementation for updating the connected UI
    }

}  // namespace gardener