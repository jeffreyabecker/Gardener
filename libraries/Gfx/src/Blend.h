#pragma once

#include <stdint.h>

namespace gardener::gfx {

// What happens to a value on its way into the frame, as a short chain of stages. apply() takes the
// value a shape asked to write and answers the byte to store, and each stage after the first sees
// what the one before it produced — so a wash that is then inverted is the byte Dim scaled and
// complemented. A chain with no stage answers the value unchanged, which is what a caller that does
// not care gets.
//
// Eight stages at most, trivially copyable, and a verb answers the chained blend rather than
// changing the one it was asked on — the way a transform's verb answers a new transform.
class Blend {
 public:
  enum class Rule : uint8_t { Plain, Invert, Dim };

  Blend() = default;
  explicit Blend(Rule rule, uint8_t level = 255)
      : rules_{rule}, levels_{level}, count_(1) {}

  // No blend at all: the value goes in and comes out.
  static Blend none() { return Blend(); }

  // Each verb adds one stage to the chain. Plain is the stage that changes nothing, so a plain
  // chain and no chain answer the same value.
  Blend plain() const { return then(Blend(Rule::Plain)); }
  Blend invert() const { return then(Blend(Rule::Invert)); }
  Blend dim(uint8_t level) const { return then(Blend(Rule::Dim, level)); }

  // This blend, then the other: the other's stages follow this one's. Chained rather than folded,
  // so a page can hold a base blend and lay a highlight over it. A Plain stage is no stage, so it
  // is skipped rather than filling a slot.
  Blend then(const Blend& outer) const;

  // The value to store, for the value a shape asked to write.
  uint8_t apply(uint8_t value) const;

 private:
  static constexpr uint8_t MaxStages = 8;

  Rule rules_[MaxStages] = {};
  uint8_t levels_[MaxStages] = {};
  uint8_t count_ = 0;
};

}  // namespace gardener::gfx
