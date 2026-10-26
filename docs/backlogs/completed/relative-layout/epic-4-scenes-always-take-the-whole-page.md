# Epic 4 — scenes always take the whole page

`Scene` in `firmware/gardener/data/system/scene.be` and the stack in
`firmware/gardener/data/system/ux.be`, with the two settings scenes and the case that pinned the
window.

- 4.1 — `Scene.init()` takes no box and sets `Position.top_left()` and `Size.page()` every time.
  Home, the browser and both routine pages drop the arguments they forwarded, and `EditScene` and
  `SettingsEditScene` stop reading the stack's box, so an editor is framed centred in the page
  rather than in a window the size of the scene beneath it. The settings case that pinned the
  window became its opposite: a list and a value editor opened over a page scene both start at the
  page's top left and take its width and height.
- 4.2 — `ux.box()`, the stack's lookup of the top scene's box, was deleted; the two settings scenes
  were its only callers.

Verified: `python tools/berry/check.py --tests` passes 88 cases, and `python tools/berry/check.py`
loads the image, drives every page and passes. No file in the image names `ux.box()`.
