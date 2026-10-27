#pragma once

#include <stdint.h>

#include <Arduino.h>

#include "ui/EditorScene.h"

namespace gardener::gfx::ui {

// The value editor of a duration, from duration_scene.be: whole seconds drawn as two fields, the
// minutes and the seconds, with Up and Down stepping the one the editor is in — whole minutes, or
// the step the setting declares of seconds — and Forward and Back moving between the two. OK keeps
// the seconds and Cancel puts back the ones it opened on; a nullable setting's nothing sits below
// the lowest, where Up enters it and Down clears back to nothing.
//
// The bounds are the setting's, in seconds: a `min` raises the floor of zero, and a `max` below it
// pins the value there rather than leaving an empty range.
class DurationScene : public EditorScene {
 public:
  DurationScene(GestureQueue& gestures, String& label, float current, float step, float min,
                float max, bool nullable);

  // The seconds the keys left it on — whole ones, and what the band prints for them; `--` is a
  // setting holding nothing.
  float current() const { return current_; }
  bool hasValue() const;
  String text() const;

 protected:
  bool handle(const char* gesture) override;
  void nudge(int8_t direction) override;
  void canceled() override;
  String value() override;
  Paint pageContents() override;
  const char* hint() override;

 private:
  enum class Stage : uint8_t { Minutes, Seconds };

  int32_t whole() const;
  int32_t floor() const;
  int32_t held(int32_t seconds) const;
  String minutesText() const;
  String secondsText() const;

  float current_;
  float opened_;
  int32_t step_;
  float min_;
  float max_;
  bool nullable_;
  Stage stage_ = Stage::Minutes;

  // What the band prints. The paints close over pointers into these, so the scene keeps the strings
  // rather than locals pageContents() returns without.
  String minutes_;
  String seconds_;
};

}  // namespace gardener::gfx::ui
