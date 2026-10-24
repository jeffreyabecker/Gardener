# progress_bubbles.be — the v2 ProgressBubbles: a wrapping layout of bubbles, the first `count`
# filled, laid out in the fill direction the field is set to.

class ProgressBubblesV2 : LayoutV2
  var _shape
  var count
  var max_count

  static def _gen_bubbles(total, shape)
    var n = total == nil ? 0 : total
    var bubbles = []
    var i = 0
    while i < n
      bubbles.push(BubbleV2(shape, "unfilled"))
      i += 1
    end
    return bubbles
  end

  def init(max_count, shape, fill_direction, spacing)
    super(self).init(nil, ProgressBubblesV2._gen_bubbles(max_count, shape), fill_direction, spacing, true)
    self._shape = shape
    self.max_count = max_count == nil ? 0 : max_count
    self.count = 0
  end

  # Rebuild the field for a new total; the filled count is set separately before a draw.
  def set_count(total)
    if total != self.max_count
      self.max_count = total
      self.children = ProgressBubblesV2._gen_bubbles(total, self._shape)
    end
  end

  def draw()
    if self.count < 0
      self.count = 0
    end
    if self.count > self.max_count
      self.count = self.max_count
    end
    var i = 0
    while i < self.children.size()
      self.children[i].fill_state = i < self.count ? "filled" : "unfilled"
      i += 1
    end
    super(self).draw()
  end
end