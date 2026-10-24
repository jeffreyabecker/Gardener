# dynamic_text.be — DynamicText: a string fitted and drawn on every call, because the
# value changes.

class DynamicText : Text
  var value

  def init()
    super(self).init()
    self.value = ""
  end

  def draw(value)
    if value == nil
      value = self.value
    end
    self.draw_fitted(value, self.fit(value))
    self.flip()
  end
end
