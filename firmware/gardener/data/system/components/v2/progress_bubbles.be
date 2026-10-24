# progress_bubbles.be — the v2 ProgressBubbles: a count drawn as bubbles laid out in a grid that
# fills the box, modeled on the v1 ProgressBubbles. The first `count` bubbles are filled.

import math

class ProgressBubblesV2 : LayoutV2
  var pos
  var dim
  var _shape
  var _fill_direction
  var count
  var max_count

  def init(pos, dim, max_count, shape, fill_direction)
    super(self).init([])
    self.pos = pos == nil ? Position(0, 0) : pos
    self.dim = dim == nil ? Size(0, 0) : dim
    self._shape = shape == nil ? "circle" : shape
    self._fill_direction = fill_direction == nil ? "lr-tb" : fill_direction
    self.max_count = 0
    self.count = 0
    self.set_count(max_count)
  end

  # Rebuild the field for a new total and lay the bubbles out for the box; the filled count is set
  # separately before a draw.
  def set_count(total)
    var n = total == nil ? 0 : total
    if n != self.max_count
      self.max_count = n
      self.children = []
      var i = 0
      while i < n
        self.children.push(BubbleV2(nil, nil, self._shape, "unfilled"))
        i += 1
      end
    end
    self._compute()
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

  # The grid the bubbles fill the box with: sqrt-based columns, each bubble a square of the cell
  # size, with the gaps between cells carrying the leftover. The fill direction names the order the
  # cells are assigned.
  def _compute()
    var cw = self.dim.w
    var ch = self.dim.h
    if self.max_count <= 0 || cw < 1 || ch < 1
      return
    end
    var cols = int(math.sqrt(self.max_count * cw / ch))
    if cols < 1
      cols = 1
    end
    if cols > self.max_count
      cols = self.max_count
    end
    var rows = int((self.max_count + cols - 1) / cols)
    var size = int(cw / cols)
    var hsize = int(ch / rows)
    if hsize < size
      size = hsize
    end
    if size < 1
      size = 1
    end
    var gap = 0
    if cols > 1 && cols * size < cw
      gap = int((cw - cols * size) / (cols - 1))
    end
    var vgap = 0
    if rows > 1 && rows * size < ch
      vgap = int((ch - rows * size) / (rows - 1))
    end
    var i = 0
    while i < self.max_count
      var col
      var row
      if self._fill_direction == "rl-tb"
        col = (cols - 1) - (i % cols)
        row = int(i / cols)
      elif self._fill_direction == "tb-lr"
        col = int(i / rows)
        row = i % rows
      elif self._fill_direction == "bt-lr"
        col = int(i / rows)
        row = (rows - 1) - (i % rows)
      else
        col = i % cols
        row = int(i / cols)
      end
      self.children[i].pos = Position(self.pos.x + col * (size + gap), self.pos.y + row * (size + vgap))
      self.children[i].dim = Size(size, size)
      i += 1
    end
  end
end