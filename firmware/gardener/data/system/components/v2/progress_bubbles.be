# progress_bubbles.be — the v2 ProgressBubbles: a wrapping layout of bubbles, the first count
# filled, laid out in the fill direction the field is set to.

import math

class ProgressBubblesV2 : LayoutV2
  var max_count
  var bubble_style
  var unfilled

  def init()
    super(self).init()
    self.wrap = true
    self.max_count = 0
    self.bubble_style = "circle"
    self.unfilled = "outline"
  end

  # Rebuild the children when the count changes, one bubble per set, sized to fill the box.
  def set_count(n)
    if n == self.max_count
      return
    end
    self.max_count = n
    self.children = []
    var s = self.bubble_size()
    self.spacing = self.bubble_gap(s)
    var i = 0
    while i < n
      var b = BubbleV2()
      b.shape = self.bubble_style
      b.w = s
      b.h = s
      self.children.push(b)
      i += 1
    end
  end

  def columns()
    if self.max_count <= 0
      return 1
    end
    var cols = int(math.sqrt(self.max_count * self.w / self.h))
    if cols < 1
      cols = 1
    end
    if cols > self.max_count
      cols = self.max_count
    end
    return cols
  end

  def bubble_size()
    if self.max_count <= 0 || self.w < 1 || self.h < 1
      return 1
    end
    var cols = self.columns()
    var rows = int((self.max_count + cols - 1) / cols)
    var size = int(self.w / cols)
    var hsize = int(self.h / rows)
    if hsize < size
      size = hsize
    end
    if size < 1
      size = 1
    end
    return size
  end

  def bubble_gap(size)
    if self.max_count <= 1
      return 0
    end
    var cols = self.columns()
    if cols > 1 && cols * size < self.w
      return int((self.w - cols * size) / (cols - 1))
    end
    return 0
  end

  def draw(count)
    if count < 0
      count = 0
    end
    if count > self.max_count
      count = self.max_count
    end
    var i = 0
    while i < self.children.size()
      self.children[i].filled = i < count
      i += 1
    end
    self.place()
    for child : self.children
      if child.filled || self.unfilled == "outline"
        child.draw()
      end
    end
    self.flip()
  end
end
