# number.be — NumericEditCfg: a number a routine edits, whole or real, and the setting rules that read
# and hold it. It is the one kind with no scene of its own: a number is an EditScene with the
# increment, the bounds and the `whole` flag the declaration gave it, so this file is only what the
# store and the row ask of the value.
#
# `whole` is the whole of the difference between what were two kinds: a whole setting parses through
# int() and holds whole numbers, and a real one parses through real(). The editor reads the same flag
# to keep a whole number whole as it steps.

class NumericEditCfg : EditCfg
  var whole

  def init(key, default_value, label, icon, step, min, max, nullable, whole)
    super(self).init(key, default_value, label, icon, nullable)
    self.bounds(step, min, max)
    self.whole = whole == true
  end

  # The store hands a number back as the text the file holds, and the flag decides what number it is:
  # a whole setting reads a whole number and a real one reads a real.
  def parse(raw)
    if self.whole
      return int(raw)
    end
    return real(raw)
  end

  def scene(value)
    return EditScene(self.label, value, self.step, self.min, self.max, self.nullable, self.whole)
  end
end
