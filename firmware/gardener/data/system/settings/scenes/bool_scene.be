# bool_scene.be — BoolScene: true or false, walked with Up/Down, and nil when the setting is
# nullable.

class BoolScene : ValueScene
  var current
  var nullable

  def init(label, current, nullable)
    super(self).init(label)
    self.current = current
    self.nullable = nullable
  end

  def nudge(dir)
    if self.nullable
      if self.current == nil
        if dir < 0
          return
        end
        self.current = false
        return
      end
      if dir < 0 && self.current == false
        self.current = nil
        return
      end
    end
    self.current = !self.current
  end

  def value()
    return self.current
  end

  def text()
    if self.current == nil
      return "--"
    end
    return self.current ? "ON" : "OFF"
  end
end

