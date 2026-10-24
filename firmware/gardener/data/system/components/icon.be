# icon.be — an icon is a lighter primitive than a Component: an intrinsic size and a
# draw(x, y), no box.

class Icon
  var w
  var h

  def init(w, h)
    self.w = w
    self.h = h
  end

  def draw(x, y)
  end
end
