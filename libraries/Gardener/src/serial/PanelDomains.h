#pragma once

namespace gardener {
inline namespace machine { class CommandOutbox; }

namespace panel::parts {
class BuzzerPlayer;
}  // namespace panel::parts

class Domain;

// The panel's own slice of the serial surface: the buzzer, which only a device with a panel has.
// The files ride with the composition, which owns the mount.
Domain* createBuzzerDomain(panel::parts::BuzzerPlayer& buzzer, CommandOutbox& outbox);

}  // namespace gardener
