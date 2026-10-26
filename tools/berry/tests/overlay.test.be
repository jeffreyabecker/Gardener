# overlay.test.be — the band the ux component owns: a layout of four fields that ux positions over the
# bottom rows with to_box(). What the layout promises is that the fields tile the band — each one
# starting where the one before it ended and the last one reaching the right edge — in the order the
# band reads, so a change to a cell's share moves it rather than leaving a gap or a field off the
# glass.

suite = "overlay"

def band_cells_tile_the_bottom_rows()
  var band = ux.band
  var band_y = screen.height() - overlay_band_height
  band.to_box(0, band_y, screen.width(), overlay_band_height)

  var cells = band.children
  assert_eq(cells.size(), 4, "the band does not hold its four fields")

  var fields = ["UnitText", "LinksCell", "StateCell", "BatteryCell"]
  var x = 0
  var i = 0
  while i < cells.size()
    assert_eq(classname(cells[i].children[0]), fields[i],
              "the band's read order is not the unit, the links, the state and the battery")
    assert_eq(cells[i].pos.x, x, "a field does not start where the one before it ended")
    assert_eq(cells[i].pos.y, band_y, "a field is not in the band")
    assert_eq(cells[i].dim.h, overlay_band_height, "a field is not the band's height")
    assert_true(cells[i].dim.w > 0, "a field has no width")
    x += cells[i].dim.w
    i += 1
  end
  assert_eq(x, screen.width(), "the fields do not reach the right edge")
end

test("the band tiles the bottom rows with its four fields", band_cells_tile_the_bottom_rows)
