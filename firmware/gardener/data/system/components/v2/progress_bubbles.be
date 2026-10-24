# progress_bubbles.be — the v2 ProgressBubbles: a wrapping layout of bubbles, the first count
# filled, laid out in the fill direction the field is set to.

import math

class ProgressBubblesV2 : LayoutV2

  var _bubble_style
  var _unfilled
  var count

  static def _gen_bubbles(max_count, bubble_style)
    var bubbles = []
    var i = 0
    while i < max_count
      var b = BubbleV2(bubble_style, false)
      bubbles.push(b)
      i += 1
    end
    return bubbles
  end

  def init(max_count, bubble_style, unfilled, fill_direction, spacing)
    #children, fill_direction, spacing, wrap
    spacing = spacing == nil ? 0 : spacing
    super(self).init( _gen_bubbles(max_count, bubble_style), fill_direction, spacing, true)

    self._bubble_style = bubble_style
    unfilled = unfilled == nil || unfilled == "filled" ? "outline" : unfilled
    self._unfilled = unfilled == nil ? "outline" : unfilled
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
      self.children[i].filled = i < self.count
      if i < self.count
        self.children[i].fill_state = "filled"
      end
      i += 1
    end
    super(self).draw()
  end
end
