# Existing portrait feature placement candidates

All 632 patches for 34 portraits were prepared exclusively from the existing isolated feature images in `../generated-features/`.
The original 632 source PNGs and 34 base portraits are unchanged, verified by SHA-256.
No image-generation service was called for this preparation.
The two earlier nose correction experiments under `work/direct-placement-pilot-contours/` are excluded from every manifest and source record.

## Status and correction count

**422 distinct option composites are flagged for fit corrections.** Multiple problems in one option count once.
This is a count of the current placement/compositing candidates, **not** a count of source images that must be regenerated.
It includes silhouette coverage, perspective, occlusion and blend problems; source regeneration is not authorized.
All 34 per-option overview boards and all 632 individual 50 x 100 previews were inspected; full native-resolution visual acceptance remains open, including the other 210 options.
The list is the documented correction backlog, not a guarantee that further native-resolution review will find no additional defects.
Technical validation is separate: `validation.json` records format, dimensions, bounds, exact prompts and unchanged input hashes.
These candidates do not satisfy the plan's seamless-composition acceptance criteria yet.

## Files and reproduction

Each portrait folder contains `manifest.cfg`, slot-sized RGBA patches, the exact original `prompts.md`, and `placement.json` with original-source hashes and affine placement parameters.
Slot rectangles use base-image coordinates (887 x 1774); option 0 remains the unchanged base and has no PNG.
The source canvas is mapped into the patch using its visible alpha bounds, preserving aspect ratio, with a 16-pixel outer alpha fade and soft accessory edges.
This is deterministic resizing/placement of existing isolated features; no feature is extracted from a generated full portrait.

From the worktree root, using the already available Python/Pillow environment:

```powershell
python tools/portraits/prepare_existing_features.py
python tools/portraits/validate_existing_features.py
```

Review-only composites are under `work/existing-feature-review/<portrait>/`: one native-size composite and one 50 x 100 preview per option, twelve deterministic random composites, and 4 x 3 sheets.
The pilot sheets are also in `docs/internal/portrait-variants/` (local ignored review artifacts).
No game renderer, runtime configuration, version or dependency changed; in-game composition and tint retuning remain outside this task.
The project version remains 0.7.3 because this is asset preparation, without a runtime/release change.

## Per-portrait backlog

| Portrait | Prepared options | Options needing correction |
|---|---:|---:|
| Kobold.mesh | 19 | 15 |
| Kobold.mesh-female | 21 | 15 |
| Goblin.mesh | 19 | 15 |
| Goblin.mesh-female | 21 | 15 |
| Orc.mesh | 19 | 15 |
| Orc.mesh-female | 21 | 15 |
| Dwarf1.mesh | 19 | 12 |
| Dwarf1.mesh-female | 19 | 14 |
| Dwarf2.mesh | 19 | 16 |
| Dwarf2.mesh-female | 19 | 16 |
| RunelordDwarf.mesh | 19 | 14 |
| RunelordDwarf.mesh-female | 19 | 12 |
| Gnome.mesh | 17 | 12 |
| Gnome.mesh-female | 19 | 12 |
| Adventurer.mesh | 21 | 12 |
| Adventurer.mesh-female | 21 | 12 |
| Monk.mesh | 21 | 12 |
| Monk.mesh-female | 21 | 12 |
| Defender.mesh | 21 | 14 |
| Defender.mesh-female | 21 | 14 |
| Wizard.mesh | 21 | 12 |
| Wizard.mesh-female | 21 | 12 |
| Elf.mesh-male | 21 | 13 |
| Elf.mesh | 21 | 13 |
| DarkElf.mesh-male | 21 | 13 |
| DarkElf.mesh | 21 | 13 |
| Troll.mesh | 19 | 15 |
| Troll.mesh-female | 19 | 15 |
| Lizardman.mesh | 17 | 13 |
| Lizardman.mesh-female | 19 | 15 |
| Knight.mesh | 9 | 2 |
| Knight.mesh-female | 9 | 4 |
| Cultist.mesh | 9 | 4 |
| Cultist.mesh-female | 9 | 4 |
| **Total** | **632** | **422** |

## Recorded failures

### Kobold.mesh

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](Kobold.mesh/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](Kobold.mesh/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [ears-1-notched.png](Kobold.mesh/ears-1-notched.png) | The new ear does not replace the original ear silhouette; both tips/contours remain visible or the feature sits in front of the original attachment. |
| [ears-2-rounder.png](Kobold.mesh/ears-2-rounder.png) | The new ear does not replace the original ear silhouette; both tips/contours remain visible or the feature sits in front of the original attachment. |
| [eyes-1-narrow.png](Kobold.mesh/eyes-1-narrow.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-2-round.png](Kobold.mesh/eyes-2-round.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-3-eyepatch.png](Kobold.mesh/eyes-3-eyepatch.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [nose-1-crooked.png](Kobold.mesh/nose-1-crooked.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [nose-2-broad.png](Kobold.mesh/nose-2-broad.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [mouth-1-teeth.png](Kobold.mesh/mouth-1-teeth.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-2-grin.png](Kobold.mesh/mouth-2-grin.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-3-cigar.png](Kobold.mesh/mouth-3-cigar.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [chin-1-forked.png](Kobold.mesh/chin-1-forked.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [chin-2-braided.png](Kobold.mesh/chin-2-braided.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [neck-2-torque.png](Kobold.mesh/neck-2-torque.png) | The complete accessory loop floats in front of the collar, beard or neck instead of wrapping behind it; perspective/occlusion needs correction. |

### Kobold.mesh-female

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](Kobold.mesh-female/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](Kobold.mesh-female/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [ears-1-notched.png](Kobold.mesh-female/ears-1-notched.png) | The new ear does not replace the original ear silhouette; both tips/contours remain visible or the feature sits in front of the original attachment. |
| [ears-2-rounder.png](Kobold.mesh-female/ears-2-rounder.png) | The new ear does not replace the original ear silhouette; both tips/contours remain visible or the feature sits in front of the original attachment. |
| [eyes-1-narrow.png](Kobold.mesh-female/eyes-1-narrow.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-2-round.png](Kobold.mesh-female/eyes-2-round.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-3-eyepatch.png](Kobold.mesh-female/eyes-3-eyepatch.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [nose-1-crooked.png](Kobold.mesh-female/nose-1-crooked.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [nose-2-broad.png](Kobold.mesh-female/nose-2-broad.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [mouth-1-teeth.png](Kobold.mesh-female/mouth-1-teeth.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-2-grin.png](Kobold.mesh-female/mouth-2-grin.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-3-cigar.png](Kobold.mesh-female/mouth-3-cigar.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [chin-1-square.png](Kobold.mesh-female/chin-1-square.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [chin-2-rounded.png](Kobold.mesh-female/chin-2-rounded.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [neck-2-torque.png](Kobold.mesh-female/neck-2-torque.png) | The complete accessory loop floats in front of the collar, beard or neck instead of wrapping behind it; perspective/occlusion needs correction. |

### Goblin.mesh

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](Goblin.mesh/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](Goblin.mesh/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [ears-1-notched.png](Goblin.mesh/ears-1-notched.png) | The new ear does not replace the original ear silhouette; both tips/contours remain visible or the feature sits in front of the original attachment. |
| [ears-2-rounder.png](Goblin.mesh/ears-2-rounder.png) | The new ear does not replace the original ear silhouette; both tips/contours remain visible or the feature sits in front of the original attachment. |
| [eyes-1-narrow.png](Goblin.mesh/eyes-1-narrow.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-2-round.png](Goblin.mesh/eyes-2-round.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-3-eyepatch.png](Goblin.mesh/eyes-3-eyepatch.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [nose-1-crooked.png](Goblin.mesh/nose-1-crooked.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [nose-2-broad.png](Goblin.mesh/nose-2-broad.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [mouth-1-teeth.png](Goblin.mesh/mouth-1-teeth.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-2-grin.png](Goblin.mesh/mouth-2-grin.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-3-cigar.png](Goblin.mesh/mouth-3-cigar.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [chin-1-square.png](Goblin.mesh/chin-1-square.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [chin-2-rounded.png](Goblin.mesh/chin-2-rounded.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [neck-2-torque.png](Goblin.mesh/neck-2-torque.png) | The complete accessory loop floats in front of the collar, beard or neck instead of wrapping behind it; perspective/occlusion needs correction. |

### Goblin.mesh-female

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](Goblin.mesh-female/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](Goblin.mesh-female/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [ears-1-notched.png](Goblin.mesh-female/ears-1-notched.png) | The new ear does not replace the original ear silhouette; both tips/contours remain visible or the feature sits in front of the original attachment. |
| [ears-2-rounder.png](Goblin.mesh-female/ears-2-rounder.png) | The new ear does not replace the original ear silhouette; both tips/contours remain visible or the feature sits in front of the original attachment. |
| [eyes-1-narrow.png](Goblin.mesh-female/eyes-1-narrow.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-2-round.png](Goblin.mesh-female/eyes-2-round.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-3-eyepatch.png](Goblin.mesh-female/eyes-3-eyepatch.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [nose-1-crooked.png](Goblin.mesh-female/nose-1-crooked.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [nose-2-broad.png](Goblin.mesh-female/nose-2-broad.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [mouth-1-teeth.png](Goblin.mesh-female/mouth-1-teeth.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-2-grin.png](Goblin.mesh-female/mouth-2-grin.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-3-cigar.png](Goblin.mesh-female/mouth-3-cigar.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [chin-1-square.png](Goblin.mesh-female/chin-1-square.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [chin-2-rounded.png](Goblin.mesh-female/chin-2-rounded.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [neck-2-torque.png](Goblin.mesh-female/neck-2-torque.png) | The complete accessory loop floats in front of the collar, beard or neck instead of wrapping behind it; perspective/occlusion needs correction. |

### Orc.mesh

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](Orc.mesh/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](Orc.mesh/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [ears-1-notched.png](Orc.mesh/ears-1-notched.png) | The new ear does not replace the original ear silhouette; both tips/contours remain visible or the feature sits in front of the original attachment. |
| [ears-2-rounder.png](Orc.mesh/ears-2-rounder.png) | The new ear does not replace the original ear silhouette; both tips/contours remain visible or the feature sits in front of the original attachment. |
| [eyes-1-narrow.png](Orc.mesh/eyes-1-narrow.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-2-round.png](Orc.mesh/eyes-2-round.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-3-eyepatch.png](Orc.mesh/eyes-3-eyepatch.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [nose-1-crooked.png](Orc.mesh/nose-1-crooked.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [nose-2-broad.png](Orc.mesh/nose-2-broad.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [mouth-1-teeth.png](Orc.mesh/mouth-1-teeth.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-2-grin.png](Orc.mesh/mouth-2-grin.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-3-cigar.png](Orc.mesh/mouth-3-cigar.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [chin-1-square.png](Orc.mesh/chin-1-square.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [chin-2-rounded.png](Orc.mesh/chin-2-rounded.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [neck-2-torque.png](Orc.mesh/neck-2-torque.png) | The complete accessory loop floats in front of the collar, beard or neck instead of wrapping behind it; perspective/occlusion needs correction. |

### Orc.mesh-female

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](Orc.mesh-female/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](Orc.mesh-female/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [ears-1-notched.png](Orc.mesh-female/ears-1-notched.png) | The new ear does not replace the original ear silhouette; both tips/contours remain visible or the feature sits in front of the original attachment. |
| [ears-2-rounder.png](Orc.mesh-female/ears-2-rounder.png) | The new ear does not replace the original ear silhouette; both tips/contours remain visible or the feature sits in front of the original attachment. |
| [eyes-1-narrow.png](Orc.mesh-female/eyes-1-narrow.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-2-round.png](Orc.mesh-female/eyes-2-round.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-3-eyepatch.png](Orc.mesh-female/eyes-3-eyepatch.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [nose-1-crooked.png](Orc.mesh-female/nose-1-crooked.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [nose-2-broad.png](Orc.mesh-female/nose-2-broad.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [mouth-1-tusks.png](Orc.mesh-female/mouth-1-tusks.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-2-grin.png](Orc.mesh-female/mouth-2-grin.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-3-cigar.png](Orc.mesh-female/mouth-3-cigar.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [chin-1-square.png](Orc.mesh-female/chin-1-square.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [chin-2-rounded.png](Orc.mesh-female/chin-2-rounded.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [neck-2-torque.png](Orc.mesh-female/neck-2-torque.png) | The complete accessory loop floats in front of the collar, beard or neck instead of wrapping behind it; perspective/occlusion needs correction. |

### Dwarf1.mesh

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](Dwarf1.mesh/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](Dwarf1.mesh/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [eyes-1-narrow.png](Dwarf1.mesh/eyes-1-narrow.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-2-round.png](Dwarf1.mesh/eyes-2-round.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-3-eyepatch.png](Dwarf1.mesh/eyes-3-eyepatch.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [nose-1-crooked.png](Dwarf1.mesh/nose-1-crooked.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [nose-2-broad.png](Dwarf1.mesh/nose-2-broad.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [mouth-1-scowl.png](Dwarf1.mesh/mouth-1-scowl.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-2-grin.png](Dwarf1.mesh/mouth-2-grin.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-3-cigar.png](Dwarf1.mesh/mouth-3-cigar.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [chin-1-forked.png](Dwarf1.mesh/chin-1-forked.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [chin-2-braided.png](Dwarf1.mesh/chin-2-braided.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |

### Dwarf1.mesh-female

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](Dwarf1.mesh-female/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](Dwarf1.mesh-female/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [hair-1-swept.png](Dwarf1.mesh-female/hair-1-swept.png) | The complete isolated hair shape conflicts with the original hair/headwear silhouette or covers facial features; shared scaling and placement do not resolve the occlusion. |
| [hair-2-waves.png](Dwarf1.mesh-female/hair-2-waves.png) | The complete isolated hair shape conflicts with the original hair/headwear silhouette or covers facial features; shared scaling and placement do not resolve the occlusion. |
| [eyes-1-narrow.png](Dwarf1.mesh-female/eyes-1-narrow.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-2-round.png](Dwarf1.mesh-female/eyes-2-round.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-3-eyepatch.png](Dwarf1.mesh-female/eyes-3-eyepatch.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [nose-1-crooked.png](Dwarf1.mesh-female/nose-1-crooked.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [nose-2-broad.png](Dwarf1.mesh-female/nose-2-broad.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [mouth-1-teeth.png](Dwarf1.mesh-female/mouth-1-teeth.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-2-grin.png](Dwarf1.mesh-female/mouth-2-grin.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-3-cigar.png](Dwarf1.mesh-female/mouth-3-cigar.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [chin-1-square.png](Dwarf1.mesh-female/chin-1-square.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [chin-2-rounded.png](Dwarf1.mesh-female/chin-2-rounded.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |

### Dwarf2.mesh

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](Dwarf2.mesh/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](Dwarf2.mesh/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [hair-1-swept.png](Dwarf2.mesh/hair-1-swept.png) | The complete isolated hair shape conflicts with the original hair/headwear silhouette or covers facial features; shared scaling and placement do not resolve the occlusion. |
| [hair-2-waves.png](Dwarf2.mesh/hair-2-waves.png) | The complete isolated hair shape conflicts with the original hair/headwear silhouette or covers facial features; shared scaling and placement do not resolve the occlusion. |
| [eyes-1-narrow.png](Dwarf2.mesh/eyes-1-narrow.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-2-round.png](Dwarf2.mesh/eyes-2-round.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-3-eyepatch.png](Dwarf2.mesh/eyes-3-eyepatch.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [nose-1-crooked.png](Dwarf2.mesh/nose-1-crooked.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [nose-2-broad.png](Dwarf2.mesh/nose-2-broad.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [mouth-1-teeth.png](Dwarf2.mesh/mouth-1-teeth.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-2-grin.png](Dwarf2.mesh/mouth-2-grin.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-3-cigar.png](Dwarf2.mesh/mouth-3-cigar.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [chin-1-forked.png](Dwarf2.mesh/chin-1-forked.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [chin-2-braided.png](Dwarf2.mesh/chin-2-braided.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [neck-1-teeth.png](Dwarf2.mesh/neck-1-teeth.png) | The complete accessory loop floats in front of the collar, beard or neck instead of wrapping behind it; perspective/occlusion needs correction. |
| [neck-2-torque.png](Dwarf2.mesh/neck-2-torque.png) | The complete accessory loop floats in front of the collar, beard or neck instead of wrapping behind it; perspective/occlusion needs correction. |

### Dwarf2.mesh-female

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](Dwarf2.mesh-female/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](Dwarf2.mesh-female/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [hair-1-swept.png](Dwarf2.mesh-female/hair-1-swept.png) | The complete isolated hair shape conflicts with the original hair/headwear silhouette or covers facial features; shared scaling and placement do not resolve the occlusion. |
| [hair-2-waves.png](Dwarf2.mesh-female/hair-2-waves.png) | The complete isolated hair shape conflicts with the original hair/headwear silhouette or covers facial features; shared scaling and placement do not resolve the occlusion. |
| [eyes-1-narrow.png](Dwarf2.mesh-female/eyes-1-narrow.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-2-round.png](Dwarf2.mesh-female/eyes-2-round.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-3-eyepatch.png](Dwarf2.mesh-female/eyes-3-eyepatch.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [nose-1-crooked.png](Dwarf2.mesh-female/nose-1-crooked.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [nose-2-broad.png](Dwarf2.mesh-female/nose-2-broad.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [mouth-1-teeth.png](Dwarf2.mesh-female/mouth-1-teeth.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-2-grin.png](Dwarf2.mesh-female/mouth-2-grin.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-3-cigar.png](Dwarf2.mesh-female/mouth-3-cigar.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [chin-1-square.png](Dwarf2.mesh-female/chin-1-square.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [chin-2-rounded.png](Dwarf2.mesh-female/chin-2-rounded.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [neck-1-teeth.png](Dwarf2.mesh-female/neck-1-teeth.png) | The complete accessory loop floats in front of the collar, beard or neck instead of wrapping behind it; perspective/occlusion needs correction. |
| [neck-2-torque.png](Dwarf2.mesh-female/neck-2-torque.png) | The complete accessory loop floats in front of the collar, beard or neck instead of wrapping behind it; perspective/occlusion needs correction. |

### RunelordDwarf.mesh

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](RunelordDwarf.mesh/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](RunelordDwarf.mesh/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [eyes-1-narrow.png](RunelordDwarf.mesh/eyes-1-narrow.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-2-round.png](RunelordDwarf.mesh/eyes-2-round.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-3-eyepatch.png](RunelordDwarf.mesh/eyes-3-eyepatch.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [nose-1-crooked.png](RunelordDwarf.mesh/nose-1-crooked.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [nose-2-broad.png](RunelordDwarf.mesh/nose-2-broad.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [mouth-1-teeth.png](RunelordDwarf.mesh/mouth-1-teeth.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-2-grin.png](RunelordDwarf.mesh/mouth-2-grin.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-3-cigar.png](RunelordDwarf.mesh/mouth-3-cigar.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [chin-1-forked.png](RunelordDwarf.mesh/chin-1-forked.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [chin-2-braided.png](RunelordDwarf.mesh/chin-2-braided.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [neck-1-teeth.png](RunelordDwarf.mesh/neck-1-teeth.png) | The complete accessory loop floats in front of the collar, beard or neck instead of wrapping behind it; perspective/occlusion needs correction. |
| [neck-2-torque.png](RunelordDwarf.mesh/neck-2-torque.png) | The complete accessory loop floats in front of the collar, beard or neck instead of wrapping behind it; perspective/occlusion needs correction. |

### RunelordDwarf.mesh-female

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](RunelordDwarf.mesh-female/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](RunelordDwarf.mesh-female/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [eyes-1-narrow.png](RunelordDwarf.mesh-female/eyes-1-narrow.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-2-round.png](RunelordDwarf.mesh-female/eyes-2-round.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-3-eyepatch.png](RunelordDwarf.mesh-female/eyes-3-eyepatch.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [nose-1-crooked.png](RunelordDwarf.mesh-female/nose-1-crooked.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [nose-2-broad.png](RunelordDwarf.mesh-female/nose-2-broad.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [mouth-1-teeth.png](RunelordDwarf.mesh-female/mouth-1-teeth.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-2-grin.png](RunelordDwarf.mesh-female/mouth-2-grin.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-3-cigar.png](RunelordDwarf.mesh-female/mouth-3-cigar.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [chin-1-square.png](RunelordDwarf.mesh-female/chin-1-square.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [chin-2-rounded.png](RunelordDwarf.mesh-female/chin-2-rounded.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |

### Gnome.mesh

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](Gnome.mesh/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](Gnome.mesh/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [eyes-1-narrow.png](Gnome.mesh/eyes-1-narrow.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-2-round.png](Gnome.mesh/eyes-2-round.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-3-eyepatch.png](Gnome.mesh/eyes-3-eyepatch.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [nose-1-crooked.png](Gnome.mesh/nose-1-crooked.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [nose-2-broad.png](Gnome.mesh/nose-2-broad.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [mouth-1-teeth.png](Gnome.mesh/mouth-1-teeth.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-2-grin.png](Gnome.mesh/mouth-2-grin.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-3-cigar.png](Gnome.mesh/mouth-3-cigar.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [chin-1-square.png](Gnome.mesh/chin-1-square.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [chin-2-rounded.png](Gnome.mesh/chin-2-rounded.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |

### Gnome.mesh-female

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](Gnome.mesh-female/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](Gnome.mesh-female/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [eyes-1-narrow.png](Gnome.mesh-female/eyes-1-narrow.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-2-round.png](Gnome.mesh-female/eyes-2-round.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-3-eyepatch.png](Gnome.mesh-female/eyes-3-eyepatch.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [nose-1-crooked.png](Gnome.mesh-female/nose-1-crooked.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [nose-2-broad.png](Gnome.mesh-female/nose-2-broad.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [mouth-1-teeth.png](Gnome.mesh-female/mouth-1-teeth.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-2-grin.png](Gnome.mesh-female/mouth-2-grin.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-3-cigar.png](Gnome.mesh-female/mouth-3-cigar.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [chin-1-square.png](Gnome.mesh-female/chin-1-square.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [chin-2-rounded.png](Gnome.mesh-female/chin-2-rounded.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |

### Adventurer.mesh

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](Adventurer.mesh/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](Adventurer.mesh/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [eyes-1-narrow.png](Adventurer.mesh/eyes-1-narrow.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-2-round.png](Adventurer.mesh/eyes-2-round.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-3-eyepatch.png](Adventurer.mesh/eyes-3-eyepatch.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [nose-1-crooked.png](Adventurer.mesh/nose-1-crooked.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [nose-2-broad.png](Adventurer.mesh/nose-2-broad.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [mouth-1-teeth.png](Adventurer.mesh/mouth-1-teeth.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-2-grin.png](Adventurer.mesh/mouth-2-grin.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-3-cigar.png](Adventurer.mesh/mouth-3-cigar.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [chin-1-square.png](Adventurer.mesh/chin-1-square.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [chin-2-rounded.png](Adventurer.mesh/chin-2-rounded.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |

### Adventurer.mesh-female

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](Adventurer.mesh-female/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](Adventurer.mesh-female/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [eyes-1-narrow.png](Adventurer.mesh-female/eyes-1-narrow.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-2-round.png](Adventurer.mesh-female/eyes-2-round.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-3-eyepatch.png](Adventurer.mesh-female/eyes-3-eyepatch.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [nose-1-crooked.png](Adventurer.mesh-female/nose-1-crooked.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [nose-2-broad.png](Adventurer.mesh-female/nose-2-broad.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [mouth-1-teeth.png](Adventurer.mesh-female/mouth-1-teeth.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-2-grin.png](Adventurer.mesh-female/mouth-2-grin.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-3-cigar.png](Adventurer.mesh-female/mouth-3-cigar.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [chin-1-square.png](Adventurer.mesh-female/chin-1-square.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [chin-2-rounded.png](Adventurer.mesh-female/chin-2-rounded.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |

### Monk.mesh

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](Monk.mesh/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](Monk.mesh/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [eyes-1-narrow.png](Monk.mesh/eyes-1-narrow.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-2-round.png](Monk.mesh/eyes-2-round.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-3-eyepatch.png](Monk.mesh/eyes-3-eyepatch.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [nose-1-crooked.png](Monk.mesh/nose-1-crooked.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [nose-2-broad.png](Monk.mesh/nose-2-broad.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [mouth-1-teeth.png](Monk.mesh/mouth-1-teeth.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-2-grin.png](Monk.mesh/mouth-2-grin.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-3-cigar.png](Monk.mesh/mouth-3-cigar.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [chin-1-square.png](Monk.mesh/chin-1-square.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [chin-2-rounded.png](Monk.mesh/chin-2-rounded.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |

### Monk.mesh-female

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](Monk.mesh-female/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](Monk.mesh-female/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [eyes-1-narrow.png](Monk.mesh-female/eyes-1-narrow.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-2-round.png](Monk.mesh-female/eyes-2-round.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-3-eyepatch.png](Monk.mesh-female/eyes-3-eyepatch.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [nose-1-crooked.png](Monk.mesh-female/nose-1-crooked.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [nose-2-broad.png](Monk.mesh-female/nose-2-broad.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [mouth-1-teeth.png](Monk.mesh-female/mouth-1-teeth.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-2-grin.png](Monk.mesh-female/mouth-2-grin.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-3-cigar.png](Monk.mesh-female/mouth-3-cigar.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [chin-1-square.png](Monk.mesh-female/chin-1-square.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [chin-2-rounded.png](Monk.mesh-female/chin-2-rounded.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |

### Defender.mesh

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](Defender.mesh/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](Defender.mesh/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [hair-1-swept.png](Defender.mesh/hair-1-swept.png) | The complete isolated hair shape conflicts with the original hair/headwear silhouette or covers facial features; shared scaling and placement do not resolve the occlusion. |
| [hair-2-waves.png](Defender.mesh/hair-2-waves.png) | The complete isolated hair shape conflicts with the original hair/headwear silhouette or covers facial features; shared scaling and placement do not resolve the occlusion. |
| [eyes-1-narrow.png](Defender.mesh/eyes-1-narrow.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-2-round.png](Defender.mesh/eyes-2-round.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-3-eyepatch.png](Defender.mesh/eyes-3-eyepatch.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [nose-1-crooked.png](Defender.mesh/nose-1-crooked.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [nose-2-broad.png](Defender.mesh/nose-2-broad.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [mouth-1-teeth.png](Defender.mesh/mouth-1-teeth.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-2-grin.png](Defender.mesh/mouth-2-grin.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-3-cigar.png](Defender.mesh/mouth-3-cigar.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [chin-1-forked.png](Defender.mesh/chin-1-forked.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [chin-2-braided.png](Defender.mesh/chin-2-braided.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |

### Defender.mesh-female

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](Defender.mesh-female/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](Defender.mesh-female/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [hair-1-swept.png](Defender.mesh-female/hair-1-swept.png) | The complete isolated hair shape conflicts with the original hair/headwear silhouette or covers facial features; shared scaling and placement do not resolve the occlusion. |
| [hair-2-waves.png](Defender.mesh-female/hair-2-waves.png) | The complete isolated hair shape conflicts with the original hair/headwear silhouette or covers facial features; shared scaling and placement do not resolve the occlusion. |
| [eyes-1-narrow.png](Defender.mesh-female/eyes-1-narrow.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-2-round.png](Defender.mesh-female/eyes-2-round.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-3-eyepatch.png](Defender.mesh-female/eyes-3-eyepatch.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [nose-1-crooked.png](Defender.mesh-female/nose-1-crooked.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [nose-2-broad.png](Defender.mesh-female/nose-2-broad.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [mouth-1-teeth.png](Defender.mesh-female/mouth-1-teeth.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-2-grin.png](Defender.mesh-female/mouth-2-grin.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-3-cigar.png](Defender.mesh-female/mouth-3-cigar.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [chin-1-square.png](Defender.mesh-female/chin-1-square.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [chin-2-rounded.png](Defender.mesh-female/chin-2-rounded.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |

### Wizard.mesh

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](Wizard.mesh/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](Wizard.mesh/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [eyes-1-narrow.png](Wizard.mesh/eyes-1-narrow.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-2-round.png](Wizard.mesh/eyes-2-round.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-3-eyepatch.png](Wizard.mesh/eyes-3-eyepatch.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [nose-1-crooked.png](Wizard.mesh/nose-1-crooked.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [nose-2-broad.png](Wizard.mesh/nose-2-broad.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [mouth-1-teeth.png](Wizard.mesh/mouth-1-teeth.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-2-grin.png](Wizard.mesh/mouth-2-grin.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-3-cigar.png](Wizard.mesh/mouth-3-cigar.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [chin-1-forked.png](Wizard.mesh/chin-1-forked.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [chin-2-braided.png](Wizard.mesh/chin-2-braided.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |

### Wizard.mesh-female

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](Wizard.mesh-female/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](Wizard.mesh-female/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [eyes-1-narrow.png](Wizard.mesh-female/eyes-1-narrow.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-2-round.png](Wizard.mesh-female/eyes-2-round.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-3-eyepatch.png](Wizard.mesh-female/eyes-3-eyepatch.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [nose-1-crooked.png](Wizard.mesh-female/nose-1-crooked.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [nose-2-broad.png](Wizard.mesh-female/nose-2-broad.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [mouth-1-teeth.png](Wizard.mesh-female/mouth-1-teeth.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-2-grin.png](Wizard.mesh-female/mouth-2-grin.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-3-cigar.png](Wizard.mesh-female/mouth-3-cigar.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [chin-1-square.png](Wizard.mesh-female/chin-1-square.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [chin-2-rounded.png](Wizard.mesh-female/chin-2-rounded.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |

### Elf.mesh-male

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](Elf.mesh-male/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](Elf.mesh-male/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [ears-1-notched.png](Elf.mesh-male/ears-1-notched.png) | The new ear does not replace the original ear silhouette; both tips/contours remain visible or the feature sits in front of the original attachment. |
| [ears-2-rounder.png](Elf.mesh-male/ears-2-rounder.png) | The new ear does not replace the original ear silhouette; both tips/contours remain visible or the feature sits in front of the original attachment. |
| [eyes-1-narrow.png](Elf.mesh-male/eyes-1-narrow.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-2-round.png](Elf.mesh-male/eyes-2-round.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-3-eyepatch.png](Elf.mesh-male/eyes-3-eyepatch.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [mouth-1-teeth.png](Elf.mesh-male/mouth-1-teeth.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-2-grin.png](Elf.mesh-male/mouth-2-grin.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-3-cigar.png](Elf.mesh-male/mouth-3-cigar.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [chin-1-square.png](Elf.mesh-male/chin-1-square.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [chin-2-rounded.png](Elf.mesh-male/chin-2-rounded.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [neck-2-torque.png](Elf.mesh-male/neck-2-torque.png) | The complete accessory loop floats in front of the collar, beard or neck instead of wrapping behind it; perspective/occlusion needs correction. |

### Elf.mesh

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](Elf.mesh/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](Elf.mesh/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [ears-1-notched.png](Elf.mesh/ears-1-notched.png) | The new ear does not replace the original ear silhouette; both tips/contours remain visible or the feature sits in front of the original attachment. |
| [ears-2-rounder.png](Elf.mesh/ears-2-rounder.png) | The new ear does not replace the original ear silhouette; both tips/contours remain visible or the feature sits in front of the original attachment. |
| [eyes-1-narrow.png](Elf.mesh/eyes-1-narrow.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-2-round.png](Elf.mesh/eyes-2-round.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-3-eyepatch.png](Elf.mesh/eyes-3-eyepatch.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [mouth-1-teeth.png](Elf.mesh/mouth-1-teeth.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-2-grin.png](Elf.mesh/mouth-2-grin.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-3-cigar.png](Elf.mesh/mouth-3-cigar.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [chin-1-square.png](Elf.mesh/chin-1-square.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [chin-2-rounded.png](Elf.mesh/chin-2-rounded.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [neck-2-torque.png](Elf.mesh/neck-2-torque.png) | The complete accessory loop floats in front of the collar, beard or neck instead of wrapping behind it; perspective/occlusion needs correction. |

### DarkElf.mesh-male

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](DarkElf.mesh-male/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](DarkElf.mesh-male/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [ears-1-notched.png](DarkElf.mesh-male/ears-1-notched.png) | The new ear does not replace the original ear silhouette; both tips/contours remain visible or the feature sits in front of the original attachment. |
| [ears-2-rounder.png](DarkElf.mesh-male/ears-2-rounder.png) | The new ear does not replace the original ear silhouette; both tips/contours remain visible or the feature sits in front of the original attachment. |
| [eyes-1-narrow.png](DarkElf.mesh-male/eyes-1-narrow.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-2-round.png](DarkElf.mesh-male/eyes-2-round.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-3-eyepatch.png](DarkElf.mesh-male/eyes-3-eyepatch.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [mouth-1-teeth.png](DarkElf.mesh-male/mouth-1-teeth.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-2-grin.png](DarkElf.mesh-male/mouth-2-grin.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-3-cigar.png](DarkElf.mesh-male/mouth-3-cigar.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [chin-1-square.png](DarkElf.mesh-male/chin-1-square.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [chin-2-rounded.png](DarkElf.mesh-male/chin-2-rounded.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [neck-2-torque.png](DarkElf.mesh-male/neck-2-torque.png) | The complete accessory loop floats in front of the collar, beard or neck instead of wrapping behind it; perspective/occlusion needs correction. |

### DarkElf.mesh

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](DarkElf.mesh/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](DarkElf.mesh/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [ears-1-notched.png](DarkElf.mesh/ears-1-notched.png) | The new ear does not replace the original ear silhouette; both tips/contours remain visible or the feature sits in front of the original attachment. |
| [ears-2-rounder.png](DarkElf.mesh/ears-2-rounder.png) | The new ear does not replace the original ear silhouette; both tips/contours remain visible or the feature sits in front of the original attachment. |
| [eyes-1-narrow.png](DarkElf.mesh/eyes-1-narrow.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-2-round.png](DarkElf.mesh/eyes-2-round.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-3-eyepatch.png](DarkElf.mesh/eyes-3-eyepatch.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [mouth-1-teeth.png](DarkElf.mesh/mouth-1-teeth.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-2-grin.png](DarkElf.mesh/mouth-2-grin.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-3-cigar.png](DarkElf.mesh/mouth-3-cigar.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [chin-1-square.png](DarkElf.mesh/chin-1-square.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [chin-2-rounded.png](DarkElf.mesh/chin-2-rounded.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [neck-2-torque.png](DarkElf.mesh/neck-2-torque.png) | The complete accessory loop floats in front of the collar, beard or neck instead of wrapping behind it; perspective/occlusion needs correction. |

### Troll.mesh

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](Troll.mesh/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](Troll.mesh/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [ears-1-notched.png](Troll.mesh/ears-1-notched.png) | The new ear does not replace the original ear silhouette; both tips/contours remain visible or the feature sits in front of the original attachment. |
| [ears-2-rounder.png](Troll.mesh/ears-2-rounder.png) | The new ear does not replace the original ear silhouette; both tips/contours remain visible or the feature sits in front of the original attachment. |
| [eyes-1-narrow.png](Troll.mesh/eyes-1-narrow.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-2-round.png](Troll.mesh/eyes-2-round.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-3-eyepatch.png](Troll.mesh/eyes-3-eyepatch.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [nose-1-crooked.png](Troll.mesh/nose-1-crooked.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [nose-2-broad.png](Troll.mesh/nose-2-broad.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [mouth-1-teeth.png](Troll.mesh/mouth-1-teeth.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-2-grin.png](Troll.mesh/mouth-2-grin.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-3-cigar.png](Troll.mesh/mouth-3-cigar.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [chin-1-square.png](Troll.mesh/chin-1-square.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [chin-2-rounded.png](Troll.mesh/chin-2-rounded.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [neck-2-torque.png](Troll.mesh/neck-2-torque.png) | The complete accessory loop floats in front of the collar, beard or neck instead of wrapping behind it; perspective/occlusion needs correction. |

### Troll.mesh-female

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](Troll.mesh-female/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](Troll.mesh-female/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [hair-1-mossstrands.png](Troll.mesh-female/hair-1-mossstrands.png) | The complete isolated hair shape conflicts with the original hair/headwear silhouette or covers facial features; shared scaling and placement do not resolve the occlusion. |
| [hair-2-mossbraids.png](Troll.mesh-female/hair-2-mossbraids.png) | The complete isolated hair shape conflicts with the original hair/headwear silhouette or covers facial features; shared scaling and placement do not resolve the occlusion. |
| [eyes-1-narrow.png](Troll.mesh-female/eyes-1-narrow.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-2-round.png](Troll.mesh-female/eyes-2-round.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-3-eyepatch.png](Troll.mesh-female/eyes-3-eyepatch.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [nose-1-crooked.png](Troll.mesh-female/nose-1-crooked.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [nose-2-broad.png](Troll.mesh-female/nose-2-broad.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [mouth-1-teeth.png](Troll.mesh-female/mouth-1-teeth.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-2-grin.png](Troll.mesh-female/mouth-2-grin.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-3-cigar.png](Troll.mesh-female/mouth-3-cigar.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [chin-1-square.png](Troll.mesh-female/chin-1-square.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [chin-2-rounded.png](Troll.mesh-female/chin-2-rounded.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [neck-2-torque.png](Troll.mesh-female/neck-2-torque.png) | The complete accessory loop floats in front of the collar, beard or neck instead of wrapping behind it; perspective/occlusion needs correction. |

### Lizardman.mesh

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](Lizardman.mesh/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](Lizardman.mesh/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [eyes-1-narrow.png](Lizardman.mesh/eyes-1-narrow.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-2-round.png](Lizardman.mesh/eyes-2-round.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-3-eyepatch.png](Lizardman.mesh/eyes-3-eyepatch.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [nose-1-crooked.png](Lizardman.mesh/nose-1-crooked.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [nose-2-broad.png](Lizardman.mesh/nose-2-broad.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [mouth-1-teeth.png](Lizardman.mesh/mouth-1-teeth.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-2-grin.png](Lizardman.mesh/mouth-2-grin.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-3-cigar.png](Lizardman.mesh/mouth-3-cigar.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [chin-1-square.png](Lizardman.mesh/chin-1-square.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [chin-2-rounded.png](Lizardman.mesh/chin-2-rounded.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [neck-2-torque.png](Lizardman.mesh/neck-2-torque.png) | The complete accessory loop floats in front of the collar, beard or neck instead of wrapping behind it; perspective/occlusion needs correction. |

### Lizardman.mesh-female

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](Lizardman.mesh-female/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](Lizardman.mesh-female/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [hair-1-shortfrill.png](Lizardman.mesh-female/hair-1-shortfrill.png) | The complete isolated hair shape conflicts with the original hair/headwear silhouette or covers facial features; shared scaling and placement do not resolve the occlusion. |
| [hair-2-wavefrill.png](Lizardman.mesh-female/hair-2-wavefrill.png) | The complete isolated hair shape conflicts with the original hair/headwear silhouette or covers facial features; shared scaling and placement do not resolve the occlusion. |
| [eyes-1-narrow.png](Lizardman.mesh-female/eyes-1-narrow.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-2-round.png](Lizardman.mesh-female/eyes-2-round.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [eyes-3-eyepatch.png](Lizardman.mesh-female/eyes-3-eyepatch.png) | The isolated eye/brow region does not follow the original pair perspective and spacing, or overlaps headwear; old and new contours remain visible. |
| [nose-1-crooked.png](Lizardman.mesh-female/nose-1-crooked.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [nose-2-broad.png](Lizardman.mesh-female/nose-2-broad.png) | The nose reads as a separate skin/scale piece with a different perspective or light boundary instead of a continuous extension of the original face. |
| [mouth-1-teeth.png](Lizardman.mesh-female/mouth-1-teeth.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-2-grin.png](Lizardman.mesh-female/mouth-2-grin.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [mouth-3-cigar.png](Lizardman.mesh-female/mouth-3-cigar.png) | The original lip, tusk or snout outline remains behind the new mouth; corners and perspective do not connect cleanly. |
| [chin-1-square.png](Lizardman.mesh-female/chin-1-square.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [chin-2-rounded.png](Lizardman.mesh-female/chin-2-rounded.png) | The isolated jaw/beard does not replace the original outer silhouette and shows a separate skin/stone boundary or remaining beard point. |
| [neck-2-torque.png](Lizardman.mesh-female/neck-2-torque.png) | The complete accessory loop floats in front of the collar, beard or neck instead of wrapping behind it; perspective/occlusion needs correction. |

### Knight.mesh

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](Knight.mesh/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](Knight.mesh/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |

### Knight.mesh-female

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](Knight.mesh-female/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](Knight.mesh-female/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [hair-1-swept.png](Knight.mesh-female/hair-1-swept.png) | The complete isolated hair shape conflicts with the original hair/headwear silhouette or covers facial features; shared scaling and placement do not resolve the occlusion. |
| [hair-2-waves.png](Knight.mesh-female/hair-2-waves.png) | The complete isolated hair shape conflicts with the original hair/headwear silhouette or covers facial features; shared scaling and placement do not resolve the occlusion. |

### Cultist.mesh

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](Cultist.mesh/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](Cultist.mesh/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [neck-1-teeth.png](Cultist.mesh/neck-1-teeth.png) | The complete accessory loop floats in front of the collar, beard or neck instead of wrapping behind it; perspective/occlusion needs correction. |
| [neck-2-torque.png](Cultist.mesh/neck-2-torque.png) | The complete accessory loop floats in front of the collar, beard or neck instead of wrapping behind it; perspective/occlusion needs correction. |

### Cultist.mesh-female

| Existing option | Observed fit problem |
|---|---|
| [build-1-slim.png](Cultist.mesh-female/build-1-slim.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [build-2-sturdy.png](Cultist.mesh-female/build-2-sturdy.png) | The isolated torso leaves the original shoulders/outline visible and introduces a second neck or collar; the head/body join and garment continuity do not match. |
| [neck-1-teeth.png](Cultist.mesh-female/neck-1-teeth.png) | The complete accessory loop floats in front of the collar, beard or neck instead of wrapping behind it; perspective/occlusion needs correction. |
| [neck-2-torque.png](Cultist.mesh-female/neck-2-torque.png) | The complete accessory loop floats in front of the collar, beard or neck instead of wrapping behind it; perspective/occlusion needs correction. |
