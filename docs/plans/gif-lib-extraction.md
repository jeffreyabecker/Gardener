# Rip the GIF display rendering out of tools/berry

## The problem

Everything that turns a rendered panel into a GIF animation lives beside the Berry harness:
`host/GifWriter.{h,cpp}` (the GIF89a encoder, the 2 x scale and the frame delay),
`host/GestureCallout.{h,cpp}` and `host/OverlayCallout.{h,cpp}` (the two host-only annotations
drawn into a captured frame), and the capture bookkeeping inside `HarnessSession`
(`capturePath_`, `captureFrames_`, `capturing_`, `beginCapture()`, `endCapture()`, `saveStill()`).
None of that knows what Berry is, so it does not belong in `tools/berry/`.

## The approach

Move it to a sibling folder, `tools/gif/`, and let `tools/berry/` compile it in, the way
`tools/berry` already compiles `libraries/Gfx` and the firmware's own sources. The move is a move:
the encoder and the two callout drawings keep their code and their `gardener` namespace. The
capture bookkeeping becomes one small class in the new folder, so `HarnessSession` keeps only the
part that is the harness's: drawing the gesture strip and the overlay hash onto the frame it is
about to hand over.

## Todos

1. **Create `tools/gif/` and move the encoder.** `GifWriter.h`, `GifWriter.cpp` from
   `tools/berry/host/` unchanged — `RenderScale`, `GifDelayCs`, `writeGif`.

2. **Move the callouts.** `GestureCallout.h/.cpp` and `OverlayCallout.h/.cpp` into `tools/gif/`
   unchanged: `gestureLabel()`, `drawGestureStrip()`, `GestureCalloutBeats`, `drawOverlayHash()`.

3. **Add `GifCapture.{h,cpp}`.** A `gardener::GifCapture` that owns the frame list, the armed path
   and the panel geometry:
   - `begin(path, width, height)` arms it and forgets the previous run's frames
   - `armed()` tells the beat whether to snapshot
   - `add(packed)` copies one packed one-bit frame
   - `end()` writes the animation and clears; nothing captured is not a failure
   - `static still(path, width, height, packed)` writes one frame as a GIF

4. **Rewire `HarnessSession`.** Include `GifCapture.h` instead of `GifWriter.h`; drop
   `capturePath_`, `captureFrames_`, `capturing_`; hold a `GifCapture capture_`. `beginCapture()`,
   `endCapture()` and `saveStill()` delegate to it, and the beat snapshots through `capture_.armed()`
   and `capture_.add()`. `captureStem_` stays — the per-case GIF name is the runner's business.

5. **Teach `check.py` about the new folder.** A `GIF` path beside `HOST`, added to the include list
   and the header-mtime scan, with its four sources in the compile list.

6. **Update the documentation that names these files.**
   - `docs/specs/berry-harness.md`: a `tools/gif/` row in the parts table, the frame section's
     paths and the constant names (`RenderScale`, `GifDelayCs`, `GestureCalloutBeats` — the prose
     still says `kRenderScale`, `kGifDelayCs`, `kGestureCalloutBeats`).
   - `.github/skills/berry/SKILL.md`: the same constant names, and where the writer now lives.

7. **Verify.** `python tools/berry/check.py --compile` — one host compile of the moved sources and
   the rewired session, then the image loads. Nothing else in the tree reaches this code.

## Notes

- Folder name confirmed with the user: `tools/gif`.
- Scope confirmed: encoder + callouts + capture bookkeeping. `BitmapPanel` stays in
  `tools/berry/host/` — it is the host's display device, not the GIF renderer.
- The tree already has uncommitted work in `libraries/` and `tools/berry/host/`; this plan touches
  only the files listed above and leaves that work alone.
- `build/` is ignored globally, so nothing new needs a `.gitignore` line.
