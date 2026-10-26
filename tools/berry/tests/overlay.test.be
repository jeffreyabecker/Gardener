# overlay.test.be — the band the ux component owns: a layout of four cells that ux positions over
# the bottom rows with to_box(). Each cell's fraction of the panel width resolves to the box its
# field has always occupied.

suite = "overlay"

def band_cells_fill_the_bottom_rows()
  var band = ux.band
  var band_y = screen.height() - overlay_band_height
  band.to_box(0, band_y, screen.width(), overlay_band_height)

  var cells = band.children
  assert_eq(cells.size(), 4, "the band does not hold its four cells")

  assert_eq(cells[0].pos.x, 0, "the unit cell is not at the left")
  assert_eq(cells[0].pos.y, band_y, "the unit cell is not in the band")
  assert_eq(cells[0].dim.w, 53, "the unit cell is not its share")
  assert_eq(cells[0].dim.h, overlay_band_height, "the unit cell is not the band's height")

  assert_eq(cells[1].pos.x, 53, "the links cell does not follow the unit")
  assert_eq(cells[1].dim.w, 50, "the links cell is not its share")

  assert_eq(cells[2].pos.x, 103, "the state cell does not follow the links")
  assert_eq(cells[2].dim.w, 13, "the state cell is not its share")

  assert_eq(cells[3].pos.x, 116, "the battery cell does not follow the state")
  assert_eq(cells[3].dim.w, 12, "the battery cell is not its share")
  assert_eq(cells[3].pos.x + cells[3].dim.w, screen.width(),
            "the battery is not at the right edge")
end

test("the band lays its four cells across the bottom rows", band_cells_fill_the_bottom_rows)
