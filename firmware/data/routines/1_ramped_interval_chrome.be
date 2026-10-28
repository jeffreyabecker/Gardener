# 1_ramped_interval_chrome.be — the chrome both interval pages draw, as a tree from the values it is
# given. It loads after the machine and before either page, because the pages name it and it names
# nothing of theirs.

# The interval chrome: the glyph and title row, the pressure and the set count, and a
# status-plus-bar row, laid out as fractions of the page. Every value is handed in, so a case draws
# the chrome from numbers rather than from a page or a machine.
def ramped_interval_chrome(icon, title, current_set, total_sets, status, remaining_percent)
  var sets
  if current_set > 0
    if current_set > total_sets
      current_set = total_sets
    end
    sets = str(current_set) + " / " + str(total_sets)
  else
    sets = str(total_sets) + (total_sets == 1 ? " set" : " sets")
  end
  var percent = remaining_percent == nil ? 0.0 : remaining_percent
  return screen.column(nil, [
    [bounds(1, 0.25), screen.row(nil, [
      [bounds(0.125, 1), screen.icon(nil, nil, nil, nil, icon)],
      [bounds(0.875, 1), screen.centred(nil, nil, nil, nil, title)]
    ])],
    [bounds(1, 0.5), screen.row(nil, [
      [bounds(0.5, 1), screen.pressure_readout()],
      [bounds(0.5, 1), screen.centred(nil, nil, nil, nil, sets)]
    ])],
    [bounds(1, 0.25), screen.row(nil, [
      [bounds(0.5, 1), screen.text(nil, nil, nil, nil, status)],
      [bounds(0.5, 1), screen.bar(nil, nil, nil, nil, percent)]
    ])]
  ])
end
