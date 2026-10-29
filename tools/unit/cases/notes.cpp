// The note sequence: the grammar the parser refuses, the frequencies the part's range holds it to,
// and the schedule it drives over a fake piezo on the suite's clock.

#include <doctest.h>

#include <string>

#include "Clock.h"
#include "fakes/Parts.h"
#include "panel/parts/BuzzerPlayer.h"

namespace {

using gardener::panel::parts::BuzzerPlayer;
using gardener::test::FakeBuzzer;

bool refused(const char* sequence, std::string& token) {
  const char* bad = nullptr;
  const bool valid = BuzzerPlayer::validate(sequence, &bad);
  token = bad == nullptr ? "" : bad;
  return !valid;
}

}  // namespace

TEST_CASE("a note sequence parses") {
  CHECK(BuzzerPlayer::validate("a4:1000 c6:500"));
}

TEST_CASE("a token that is not a note names itself") {
  std::string token;
  CHECK(refused("h4:100", token));
  CHECK_EQ(token, std::string("h4:100"));
}

TEST_CASE("a note below the part's range is refused") {
  std::string token;
  CHECK(refused("a0:100", token));
  CHECK_EQ(token, std::string("a0:100"));
}

TEST_CASE("a sharp below the part's range is refused") {
  std::string token;
  CHECK(refused("c#0:100", token));
  CHECK_EQ(token, std::string("c#0:100"));
}

TEST_CASE("a note above the part's range is refused") {
  std::string token;
  CHECK(refused("b9:100", token));
  CHECK_EQ(token, std::string("b9:100"));
}

TEST_CASE("a note at the top of the part's range plays") {
  FakeBuzzer buzzer;
  BuzzerPlayer player(buzzer);
  REQUIRE(player.play("b8:100"));
  CHECK_EQ(buzzer.frequencyHz(), static_cast<uint16_t>(7902));
}

TEST_CASE("a duration of zero is refused") {
  std::string token;
  CHECK(refused("a4:0", token));
  CHECK_EQ(token, std::string("a4:0"));
}

TEST_CASE("a note with no duration is refused") {
  std::string token;
  CHECK(refused("a4", token));
  CHECK_EQ(token, std::string("a4"));
}

TEST_CASE("a duration with junk after it is refused") {
  std::string token;
  CHECK(refused("a4:100x", token));
  CHECK_EQ(token, std::string("a4:100x"));
}

TEST_CASE("an empty sequence is refused") {
  std::string token;
  CHECK(refused("", token));
}

TEST_CASE("a refused play leaves the sound that was playing") {
  FakeBuzzer buzzer;
  BuzzerPlayer player(buzzer);
  gardener::host::reset();
  REQUIRE(player.play("a4:1000"));
  CHECK_EQ(buzzer.frequencyHz(), static_cast<uint16_t>(440));
  CHECK_FALSE(player.play("h4:100"));
  CHECK_EQ(buzzer.frequencyHz(), static_cast<uint16_t>(440));
  CHECK(buzzer.sounding());
}

TEST_CASE("a sequence steps through its notes and ends in silence") {
  FakeBuzzer buzzer;
  BuzzerPlayer player(buzzer);
  gardener::host::reset();
  REQUIRE(player.play("a4:1000 c6:500"));
  player.update(gardener::host::nowMs());
  CHECK_EQ(buzzer.frequencyHz(), static_cast<uint16_t>(440));

  gardener::host::advance(1000);
  player.update(gardener::host::nowMs());
  CHECK_EQ(buzzer.frequencyHz(), static_cast<uint16_t>(1047));

  gardener::host::advance(500);
  player.update(gardener::host::nowMs());
  CHECK_FALSE(buzzer.sounding());
  CHECK_FALSE(player.playing());
}

TEST_CASE("a continuous tone has no remaining time") {
  FakeBuzzer buzzer;
  BuzzerPlayer player(buzzer);
  gardener::host::reset();
  player.tone(440, 0);
  CHECK_EQ(player.remainingMs(gardener::host::nowMs()), static_cast<uint32_t>(0));
}

TEST_CASE("a timed note counts down to nothing") {
  FakeBuzzer buzzer;
  BuzzerPlayer player(buzzer);
  gardener::host::reset();
  player.tone(440, 500);
  CHECK_EQ(player.remainingMs(gardener::host::nowMs() + 200), static_cast<uint32_t>(300));
  CHECK_EQ(player.remainingMs(gardener::host::nowMs() + 500), static_cast<uint32_t>(0));
}
