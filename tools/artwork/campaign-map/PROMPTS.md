# Hollowmark campaign artwork

Date: 2026-10-03. Tool: built-in image_gen for the original and revision prompts below; Python, Pillow and NumPy for mask rasterisation, image resampling, crops and state effects. No additional packages were installed. All artwork is original and licensed GPLv3+.

## Approved colour requirement

Province mask colours are unique, at least 20 apart from each other in Euclidean RGB distance, never pure black; black is reserved for pixels outside the provinces.

## Generation 1: discarded initial map

This initial output was rejected because the province layout and cartographic treatment did not meet the delivery requirements; it is not included in the delivered files.

```text
Create a production game asset: world_base.png, exactly 2560x1600 RGBA. Original dark fantasy top-down cartographic parchment map of kingdom Hollowmark on a dark stone table. Flat top-down 2D ink-and-watercolour illustration, no perspective landscape. Central land spans approximately x180..2380 y125..1475. South and east coast, northern mountain chain, southwest green village start, northeast largest fortified capital. Exactly 26 organically shaped contiguous provinces with crisp thin dark ink borders, varied sizes smaller southwest and larger northeast; no labels or text inside provinces. Detailed little drawn hills, forest clusters, settlements, rivers, roads. Southwest green hills/village, eastward ford, northward springs/well town, moor/mines, south cliff fort, ash valley, west central great river, fen convent, south lagoons/port adjacent inland dunes/barrows, central bridges/lava rift, northern ravine forest, central illuminated hill, northern volcano, east thorns/abbey, north mountain cairns, east harbour/sea caves, east marsh towers/golden spire, northeast black forest/crossroads fortress, far north silver mines/lakes, northeast haunted forest/royal grounds/citadel. Muted parchment beige/brown/green, hand-drawn readable cartography, slightly ominous. Worn slightly curled parchment edges, original compass ornament, blank title cartouche top left and blank progress cartouche bottom left outside land. No readable text, no labels, no watermark. Neutral provinces only. All design original.
```

## Generation 2: initial delivered map (superseded by the native-tile revision)

The input was an original procedural layout guide, 2560x1600, with 26 coloured organic province regions and temporary province IDs; the guide was only an intermediate input. The generated output was 1586x992 RGB and was resampled with Lanczos to the required 2560x1600 RGBA. Final province outlines were traced into shared pixel polygons, inked onto the base, and used for both the ID map and state-layer masks; temporary guide lettering is absent from the final map. The northern lake region and the hunting grounds are separate provinces. The citadel province is the largest.

```text
Use case: stylized-concept. Asset: 2560x1600 flat hand-inked world map on worn parchment over dark stone, original dark fantasy. The supplied coloured image is ONLY an exact province layout guide: preserve coast footprint and all 26 province boundaries precisely, remove ALL its IDs/text and colours. Render as flat medieval cartography, tiny symbolic mountains/tree clusters/building plan icons rather than realistic aerial landscape or tall 3D buildings. Parchment beige and brown ink, green-brown peaceful neutral lands. Top-down 2D, no perspective. Main land within x180..2400 y150..1435, coast south and east with blue-grey ink hatching. Northern edge mountains.
Each guide province receives its specific geography: T01 small green hills with one village, T02 river ford, T03 springs and well town, T04 moorland and mine heads, T05 southern cliff fort, T06 grey ash valley, T07 west-centre large winding river, T08 fen convent, T09A southern lagoons and port, T09B inland dunes and ancient barrows, T10 central town bridging lava rift, T11 northern forest ravines, T12 centre illuminated hill, T13 centre-north volcanic hollow, T14 thorn hedges, T15 abbey town, T16 northern mountain cairns, T17 east-coast harbour with sea caves, T18A east marsh watchtowers, T18B gold hills and treasury spire, T19 northeast black forest, T20 northeast crossroads fortress, T21 far north silver mines and lakes, T22 northeast misty forest, T23 royal hunting grounds near capital, T24 largest far northeast walled capital. Very fine roads connect adjacent lands. Distinctive original compass rose bottom right outside land; blank title cartouche at x210 y35 w560 h85, blank progress cartouche x220 y1460 w680 h80. Worn curled parchment margins. Absolutely no readable lettering, no province labels, no extra province boundaries, no modern graphics, no watermark. Exact layout and symbolic flat cartographic art are essential.
```

## Generation 3: delivered bonus cave-marker atlas

Generated output: 1920x819 RGBA with genuine transparency. Each of five 384px-wide cells was trimmed to its alpha bounds, fitted within 80x80, and centred on a transparent 96x96 canvas. Alpha was preserved. Hidden variants are desaturated and dimmed; completed variants have a gold completion tick.

```text
Use case: stylized-concept. Create ONE transparent sprite atlas, five equally sized square cells in a single horizontal row, 480x96 preferred, no text or cell dividers. Each cell contains an original dark-fantasy hand-painted top-down map marker: small stone cave entrance with a muted gold rim, deep charcoal opening, ember-orange lantern and parchment-brown stones, crisp dark ink outline, readable at 96x96, transparent margins. Left to right: cave with round boulder; cave with tiny arrow-target crest; cave with curled labyrinth-path crest; cave with three skittle pins; cave with a small swarm of bat/insect silhouettes. Consistent lighting upper-left, no backplate outside cave, no logos, no watermark, original designs. The five markers are centered and fully separated by transparent gutters.
```

## Pixel contract and technical processing

All coordinates in the JSON are in the 2560x1600 base coordinate system. Province bounding boxes are tight integer bounds of the final mask; the right and bottom edges are exclusive. All three regular state layers have exactly 16px padding on every side, size [bbox width + 32, bbox height + 32], and origin [bbox x - 16, bbox y - 16]. Their province interiors use precisely the same alpha mask. Available-state glow can extend outside the mask into the padding: an 8px dilation with a soft blur, ember orange. The underlying province colour is unchanged.

Current locked layers reduce saturation to 72% and brightness to 45%, then add visible smooth silver-grey fog. Conquered layers use charcoal earth, fine 1–2px branching ink cracks, subtle ember veins and an original hand-drawn dark-red, gold-edged horned-heart keeper banner with a wooden pole centred at the banner anchor. The final internal-border pass darkens and widens the shared province outlines in all states, as detailed below.

Lift layers scale the tight province cutout to [ceil(width * 1.06), ceil(height * 1.06)] with Lanczos and add 32px padding on each side. The lift origin is [bbox x - floor((scaled width - width) / 2) - 32, bbox y - floor((scaled height - height) / 2) - 32]. A lower bevel, upper-left light rim and blurred lower-right shadow create elevation. The normal crop and lift crop therefore intentionally differ.

All PNGs are 8-bit, explicitly tagged sRGB; the ID map is RGB, never resampled or antialiased. Its only colours are the 26 JSON mask colours and black. State-layer files stay below 2,000,000 bytes.

The review preview composites T01 through T12 (including both T09 branches) conquered, T13 available with its lift overlay, all remaining provinces locked, and B01 through B03 found. Site icons are centred on their JSON position.

## Validation

Run from the repository root:

```sh
python tools/artwork/campaign-map/validate_world.py
```

Requires the already available Pillow library. An optional argument selects another campaign asset directory. The validator fails on missing files, wrong modes/sizes/bit depth/sRGB, extra ID-map colours, disconnected masks, invalid IDs/names/hosts, palette distances below 20, incorrect crop origins, misaligned alpha, out-of-province anchors/sites, oversized layers or an incorrect preview composite. It also verifies that the finale province is the largest and that the two branch pairs share a border.

Only artwork, the fixed artwork JSON, this prompt record, the validator and CREDITS are delivered; no game code, levels or configuration changed, so no game-version or README change is required. This is an asset delivery; game-screen integration is a separate task.

Initial delivery validation completed on 2026-10-03: the full asset validator exited 0; all 125 new files have CREDITS entries; the largest regular/lift state layer was 606,627 bytes; the 122 PNGs totalled approximately 47 MB. Five isolated negative cases were rejected correctly: palette distance below 20, a missing state layer, an unexpected ID-map colour, a disconnected province pixel and an incorrect preview pixel. The review image was inspected for cartographic style, blank title/progress areas and visible conquered/locked/raised states; no game runtime or gameplay acceptance was performed.

## Revision: native-resolution artwork and stronger state treatment (2026-10-03)

Tool for each of the following five image-generation/editing calls: built-in image_gen. Python/Pillow/NumPy performed the explicitly authorised technical crops, effects, masks and validation; no packages were installed.

The map was redrawn as four overlapping 1536x1024 native tiles. Each tile was reduced to 1440x960, placed at [0,0], [1120,0], [0,640] or [1120,640], and stitched into the exact 2560x1600 canvas; no tile was enlarged. Overlaps are 320px, joined along a low-error seam with only 8px feathering to retain sharp ink detail. Tile mean-colour corrections were bounded to 8 levels per channel to retain the approved palette. The supplied older crops were composition/style inputs, not the delivered map pixels. The parchment treatment and cartographic ink/watercolour style were retained.

### Tile prompts

Tile 1; actual output 1536x1024 RGB:

```text
Use case: precise-object-edit. This is one overlapping tile of an existing hand-inked and watercolour parchment campaign map. Re-render this exact tile in genuinely new sharp fine detail at native 1536x1024 resolution (3:2), not an enlargement or sharpening filter applied to its blurred pixels. Preserve its exact composition, all geographic subjects and positions, muted beige/brown/green/blue palette, hand-drawn ink-and-watercolour style, worn parchment material, the existing decorative elements and blank cartouches. Keep the top-down cartographic treatment. Redraw tiny tree foliage, rock hatching, roof tiles and fine coastline/water hatching crisply. Remove ALL province-border overlay lines crossing terrain, including long straight rectangular borders and duplicate dark territorial outlines; keep coastlines, rivers, roads and the finer linework of buildings and terrain. Province boundaries will be drawn later from exact technical masks. Keep every subject in place right to the four image edges so adjoining tiles match; do not add margins, frames, labels, words, new settlements, new terrain, banners or state effects. Avoid blurry strokes, 3D rendering, colour grading and altered parchment.
Tile 1 is the north-west portion; treat the attached image as the edit target and preserve its framing exactly.
```

Tile 2; actual output 1536x1024 RGB:

```text
Use case: precise-object-edit. This is one overlapping tile of an existing hand-inked and watercolour parchment campaign map. Re-render this exact tile in genuinely new sharp fine detail at native 1536x1024 resolution (3:2), not an enlargement or sharpening filter applied to its blurred pixels. Preserve its exact composition, all geographic subjects and positions, muted beige/brown/green/blue palette, hand-drawn ink-and-watercolour style, worn parchment material, the existing decorative elements and blank cartouches. Keep the top-down cartographic treatment. Redraw tiny tree foliage, rock hatching, roof tiles and fine coastline/water hatching crisply. Remove ALL province-border overlay lines crossing terrain, including long straight rectangular borders and duplicate dark territorial outlines; keep coastlines, rivers, roads and the finer linework of buildings and terrain. Province boundaries will be drawn later from exact technical masks. Keep every subject in place right to the four image edges so adjoining tiles match; do not add margins, frames, labels, words, new settlements, new terrain, banners or state effects. Avoid blurry strokes, 3D rendering, colour grading and altered parchment.
Tile 2 is the north-east portion; treat the attached image as the edit target and preserve its framing exactly.
```

Tile 3; actual output 1536x1024 RGB:

```text
Use case: precise-object-edit. This is one overlapping tile of an existing hand-inked and watercolour parchment campaign map. Re-render this exact tile in genuinely new sharp fine detail at native 1536x1024 resolution (3:2), not an enlargement or sharpening filter applied to its blurred pixels. Preserve its exact composition, all geographic subjects and positions, muted beige/brown/green/blue palette, hand-drawn ink-and-watercolour style, worn parchment material, the existing decorative elements and blank cartouches. Keep the top-down cartographic treatment. Redraw tiny tree foliage, rock hatching, roof tiles and fine coastline/water hatching crisply. Remove ALL province-border overlay lines crossing terrain, including long straight rectangular borders and duplicate dark territorial outlines; keep coastlines, rivers, roads and the finer linework of buildings and terrain. Province boundaries will be drawn later from exact technical masks. Keep every subject in place right to the four image edges so adjoining tiles match; do not add margins, frames, labels, words, new settlements, new terrain, banners or state effects. Avoid blurry strokes, 3D rendering, colour grading and altered parchment.
Tile 3 is the south-west portion; treat the attached image as the edit target and preserve its framing exactly.
```

Tile 4; actual output 1536x1024 RGB:

```text
Use case: precise-object-edit. This is one overlapping tile of an existing hand-inked and watercolour parchment campaign map. Re-render this exact tile in genuinely new sharp fine detail at native 1536x1024 resolution (3:2), not an enlargement or sharpening filter applied to its blurred pixels. Preserve its exact composition, all geographic subjects and positions, muted beige/brown/green/blue palette, hand-drawn ink-and-watercolour style, worn parchment material, the existing decorative elements and blank cartouches. Keep the top-down cartographic treatment. Redraw tiny tree foliage, rock hatching, roof tiles and fine coastline/water hatching crisply. Remove ALL province-border overlay lines crossing terrain, including long straight rectangular borders and duplicate dark territorial outlines; keep coastlines, rivers, roads and the finer linework of buildings and terrain. Province boundaries will be drawn later from exact technical masks. Keep every subject in place right to the four image edges so adjoining tiles match; do not add margins, frames, labels, words, new settlements, new terrain, banners or state effects. Avoid blurry strokes, 3D rendering, colour grading and altered parchment.
Tile 4 is the south-east portion; treat the attached image as the edit target and preserve its framing exactly.
```

### Hand-drawn banner prompt

Actual output: 1024x1536 RGBA. The generated alpha was retained, the artwork fitted inside 84x114 and centred on the existing 88x120 banner canvas; no standalone banner file was added to the deliverables.

```text
Use case: stylized-concept. Generate a single original keeper banner sprite on a genuinely transparent background, native 512x768, with clear transparent margin. Match an antique parchment fantasy map drawn in thin brown-black pen ink and muted watercolour. A slender irregular WOODEN pole with bark grain, small tied rope knots and a pointed wooden finial supports a small deep-red hanging cloth pennant with a muted gold stitched hem and an original muted-gold horned-heart emblem: recognisable heart with two curved outward horns. The cloth has soft watercolour folds, darker red fabric shadows, a slightly uneven weathered lower edge, fine crosshatched ink shading; the pole is brown wood with no metallic gold shaft. Upper-left illumination; a small realistic cast shade on cloth, slight flutter, readable at 88x120 game-map size. It must look hand-drawn on the same medieval map, not a flat vector icon, not plastic, not photoreal, not a glossy 3D UI badge. No text, no logos, no backdrop, no border or additional symbols. Keep the full pole and entire cloth inside the image.
```

### Revised technical treatment

The neutral terrain retains the native tile detail. Smooth, mixed-wavelength displacement bends the technical province edges; the ID map uses nearest-neighbour sampling to preserve its exact palette. Only the narrow coast artwork is remapped with its coast mask, so interior buildings and terrain are not distorted. The deformation Jacobian was checked to remain positive. The longest measured horizontal/vertical boundary run is 36px; the longest diagonal run is 35 samples (49.50px), below the requested 80px limit. Tight bboxes, layer origins and anchors were recalculated without changing JSON fields, IDs, names, mask colours or asset paths. All bonus-marker PNGs remain byte-identical to the original delivery.

Locked provinces use 72% colour saturation and 45% brightness before a visible silver-grey cloud layer. The measured pre-fog luminance ratios across all provinces are approximately 44.4% to 44.5%, including 8-bit rounding. Conquered cracks are sparse, branched brown-black ink strokes, strictly 1–2px wide, with subtle low-opacity ember accents. The flat procedural banner was replaced by the generated watercolour/ink cloth and wooden-pole sprite. Available glow parameters and the 106% lift, bevel, rim and shadow parameters stayed unchanged during this revision.

## Final approved-map border pass (2026-10-03)

After approval of the revised neutral map, only internal province borders were strengthened: a 7px dilation of the exact shared-province edge mask, RGB [18,13,9], applied consistently to the base and regular state layers; lift layers were recut using the same approved lift algorithm. The outside coast outline was not globally thickened. The existing site markers and approved geography were retained, and the same review-state composite was rebuilt.

Pixel comparison against the approved snapshot passed for all 104 state layers: no decoded pixels changed outside the border bands (including the necessary lift-resampling margin), and every layer's alpha stayed identical. The JSON, ID-map and all 15 bonus-marker files are byte-identical to the approved snapshot. Filenames and data structure are unchanged.

Final validation exited 0 for all 26 connected provinces, 104 layers, five sites, all required PNG formats/dimensions/sRGB tags, palette distances, alignment and the exact preview composite. The largest state layer is 685,203 bytes, below 2 MB; the 122 PNG files total 51,787,843 bytes. Existing CREDITS rows cover every delivered file. This update changes only artwork and its prompt/provenance record, so no game-version, README or gameplay documentation update is required.
