# fixed_text.be — FixedText: one string, fitted once and cached, drawn in the content
# rect.

class FixedText : Text
  var value
  var candidate

  def init()
    super(self).init()
    self.value = ""
    self.candidate = nil
  end

  def draw()
    if self.candidate == nil
      self.candidate = self.fit(self.value)
    end
    self.draw_fitted(self.value, self.candidate)
    self.flip()
  end
end
