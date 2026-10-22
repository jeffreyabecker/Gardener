# settings.be — the two helpers both halves of a duration use: whole seconds held to a setting's
# bounds, and those seconds as the compact text the panel prints.
#
# The editing scenes live in settings_edit.be and the editor a setting carries in
# settings_editors.be; both read a duration through these.

import string

# Whole seconds, held to the setting's bounds. The floor is 0 unless a non-negative `min` raises
# it; a `max` below the floor pins the value there rather than leaving an empty range.
def duration_seconds(seconds, min, max)
  var low = 0
  if min != nil
    low = int(min)
    if low < 0
      low = 0
    end
  end
  var high = nil
  if max != nil
    high = int(max)
    if high < low
      high = low
    end
  end
  var value = int(seconds)
  if value < low
    value = low
  end
  if high != nil && value > high
    value = high
  end
  return value
end

def duration_text(seconds)
  var whole = int(seconds)
  if whole <= 0
    return "0s"
  end
  var minutes = whole / 60
  var rest = whole % 60
  if minutes == 0
    return str(rest) + "s"
  end
  if rest == 0
    return str(minutes) + "m"
  end
  return str(minutes) + "m" + str(rest) + "s"
end
