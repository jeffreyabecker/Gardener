# jingles_edit.be — the jingle library's row as an editor: the lookups a row makes of
# `device.all_jingles()` and the choice pair that auditions what it shows.
#
# It loads after browser.be and before readout_settings.be, whose ReadoutSettings builds the jingle
# row into the readout's set. The editor sounds what it is showing as it opens and steps, so a jingle
# is chosen by ear rather than by identifier: `JingleEditCfg` declares the choice and
# `JingleEditScene` is the editor that plays it.

# The jingle the device answers, or nil for an identifier the library does not hold: the list is the
# whole of what a page knows about the library, so every lookup scans it.
def jingle_notes(id)
  for j : device.all_jingles()
    if j.key == id
      return j.notes
    end
  end
  return nil
end

# A setting that was never written, a hand-edited value or a jingle this image does not ship all
# land on the first entry, which is what `device.play_current_jingle()` plays for the same reason.
def stored_jingle()
  var all = device.all_jingles()
  if all.size() == 0
    return nil
  end
  var id = device.jingle()
  if id == nil || id == "" || jingle_notes(id) == nil
    return all[0].key
  end
  return id
end

# The identifier the device stores and the label the panel prints, one [key, value] pair per jingle,
# so the row and the editor name the jingle from the list the device answers.
def jingle_choices()
  var out = []
  for j : device.all_jingles()
    out.push([j.key, j.short_title])
  end
  return out
end

# The jingle row plays what it is showing as it opens and steps, so a jingle is chosen by ear.
class JingleEditScene : ChoiceEditScene
  def init(current, values)
    super(self).init("JINGLE", current, values, false)
  end

  def activate()
    self.play()
    self.draw()
  end

  def nudge(dir)
    super(self).nudge(dir)
    self.play()
  end

  def cancel()
    # What was auditioning is the value being refused, so it goes quiet on the way out.
    device.silence()
    super(self).cancel()
  end

  def play()
    var notes = jingle_notes(self.value())
    if notes != nil
      device.play_sequence(notes)
    end
  end
end

class JingleEditCfg : ChoiceEditCfg
  def init()
    super(self).init("jingle", stored_jingle(), "\x0D \x0E \x0D", nil, jingle_choices(), false)
  end

  def scene(value)
    return JingleEditScene(value, self.values)
  end
end
