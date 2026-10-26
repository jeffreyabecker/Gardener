# 1_ramped_interval_chrome.be — the chrome both interval pages draw. It loads after the machine and
# before either page, because the pages name it and it names nothing of theirs.

# The interval chrome: the glyph and title row, the pressure and the set count, and a
# status-plus-bar row, laid out as fractions of the page. The glyph and the title are given to it,
# so a subclass of the page names its own and nothing is swapped after the fact.
class RampedIntervalChrome : AutoLayout
  var icon
  var title
  var sets
  var status
  var bar

  def init(icon, title_text)
    self.icon = icon
    self.title = Text(title_text)
    self.sets = Text("")
    self.status = Text("", text_settings("sans", "none", "left", "center"))
    self.bar = ProgressBar(nil)
    super(self).init(bounds(nil, nil), [
      AutoLayout(bounds(1, 0.25), [
        AutoLayout(bounds(0.125, 1), [self.icon]),
        AutoLayout(bounds(0.875, 1), [self.title])
      ]),
      AutoLayout(bounds(1, 0.50), [
        AutoLayout(bounds(0.5, 1),
                   [AutoPressureDisplay("pressure:target", "br", 0.25)]),
        AutoLayout(bounds(0.5, 1), [self.sets])
      ]),
      AutoLayout(bounds(1, 0.25), [
        AutoLayout(bounds(0.5, 1), [self.status]),
        AutoLayout(bounds(0.5, 1), [self.bar])
      ])
    ])
  end

  # A page with a set under way shows where it has got to, `2 / 12`; a ready page has no set to count
  # and shows how many the run will make, `12 sets`.
  def draw(current_set, total_sets, status, remaining_percent)
    if current_set > 0
      if current_set > total_sets
        current_set = total_sets
      end
      self.sets.value = str(current_set) + " / " + str(total_sets)
    else
      self.sets.value = str(total_sets) + (total_sets == 1 ? " set" : " sets")
    end
    self.status.value = status
    self.bar.value = remaining_percent
    super(self).draw()
  end
end
