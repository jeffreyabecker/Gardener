# bool_editor.be — BoolEditor: a setting whose value is true or false.

class BoolEditor : Editor
  def kind()
    return "bool"
  end

  def bounded(value)
    return value ? true : false
  end

  def text(value)
    return value ? "ON" : "OFF"
  end

  def lowest()
    return false
  end

  def scene(label, value)
    return BoolScene(label, value, self.nullable)
  end
end

