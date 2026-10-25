# settings.be — the two helpers both halves of a duration use: whole seconds held to a setting's
# bounds, and those seconds as the compact text the panel prints — plus the record a choice setting's
# entries are held as.
#
# The editing scenes live in scenes/ and the settings a routine builds in configs/; both read a
# duration through these, and a choice through the record and the normalizer below.

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

# One entry of a choice setting: the value the setting stores, the label the row and the editor print,
# a glyph to draw beside it, and the gestures that entry answers itself — a dict of gesture to
# callback, nil when it offers nothing beyond being picked. A choice is written as a bare value, a
# [value, title] pair or one of these; it is always held as one of these.
class ChoiceItem
  var key, title, icon, extra_actions

  def init(key, title, icon, extra_actions)
    self.key = key
    self.title = title == nil ? str(key) : title
    self.icon = icon
    self.extra_actions = extra_actions
  end
end

# A setting's choices as ChoiceItems, whatever the declaration was written with: a bare value is both
# the key and its own title, a [value, title] pair splits, and a record is already one.
def choice_items(values)
  var out = []
  if values == nil
    return out
  end
  for entry : values
    if isinstance(entry, ChoiceItem)
      out.push(entry)
    elif isinstance(entry, list)
      out.push(ChoiceItem(entry[0], entry.size() > 1 ? entry[1] : nil, nil, nil))
    else
      out.push(ChoiceItem(entry, nil, nil, nil))
    end
  end
  return out
end
