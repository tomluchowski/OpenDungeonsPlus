# Illustrated creature portraits

The population panel uses original illustrated portraits stored as
`materials/textures/portrait-<MeshName>.png`. The creature mesh filename is the
identity key; custom creatures without an illustration retain the model preview.
The small portraits next to the hand reuse an upper-centered square crop of the same
illustration, one per held creature; their ordering and pickup/drop behavior
are unchanged. Meshes without artwork keep the existing model-based square crop.

All 33 mesh identities in the bundled creature configuration have illustrations.
The square crop starts one quarter of the unused image height from the top,
keeping faces visible; model previews retain their original centered crop.

Artwork is generated with the built-in image generation tool, using this
project's model exports as identity references. Recreate those references with
the [maintained exporter](../../tools/portraits/README.md); they are build outputs.
The exact generation prompts are recorded below. Generated raster files are
tracked game assets and distributed under the project's GPL-3.0-or-later terms.
All portraits are AI-generated for this project; they do not depict characters from other games.

Additional per-creature prompts are in the adjacent Markdown files. The Goblin
image is the consistency exemplar for this artwork set. Keep the mesh identity,
original model colors and
equipment when revising an illustration. Do not put count labels into the art:
the population panel supplies the live count overlay.

## Gender variants

Seventeen humanoid portraits have a second image for the other gender:
`portrait-<Mesh>.mesh-female.png` (or `-male.png` for the two elves), each with
a prompt record `<Mesh>-female.md` / `<Mesh>-male.md`. The game shows such an image
in the profile card and the Dungeonbook when the creature's profile gender matches
it and the file exists; otherwise, and for creatures without a gender, it uses the
base `portrait-<Mesh>.mesh.png`. The population panel and the hand icons always use
the base image. The colour regions of `config/portrait-tints.cfg` are tuned per
image, so every gender image has an own entry named like the file (for example
`Orc.mesh-female`).

## Composed Dungeonbook pictures

When the folder `materials/portraits/variants/<catalog id>/` holds a `manifest.cfg`, the Dungeonbook and the
creature card show a picture composed from the neutral base named there and one chosen part per slot, instead of
the preview portrait. The server picks the parts once when a creature spawns and stores them with the creature
(optional last token of the creature line in the save file, sent to the clients with the creature data). The
catalog id is the mesh name plus the lower case gender (`Orc.mesh-male`), or the plain mesh name when the
folder without gender exists. Without a valid manifest the Dungeonbook shows the tinted preview portrait as
before; the creature bar never uses these folders.

Settings are in `config/dungeonbook-appearance.cfg` (asset folder, cache limits), the colour regions of the
neutral bases in `config/dungeonbook-base-tints.cfg`, the hair and beard colours in
`config/dungeonbook-part-tints.cfg` (the bases have no hair, eyes or beards, so the colour is applied to the chosen
parts) and the profile remarks that match the parts in
`config/dungeonbook-quirks.cfg` (one line per slot and option name, as written in the manifests).

## Goblin.mesh

Identity reference: `build/portrait-export/portrait-Goblin.mesh.png`.
Output: `materials/textures/portrait-Goblin.mesh.png`.

Create a finished 2D painted game portrait from this original project's goblin model reference, preserving its identity rather than rendering the model. One narrow vertical portrait, exactly 1:2 width-to-height composition. Subject: bald gray-olive green goblin, huge pointed ears, bright pale green eyes, wide nose, small pointed teeth, lean bare upper torso; all these identity features must match the supplied reference. Reinterpret as an expressive, mischievous hand-painted late-1990s dark-fantasy strategy-game character card: prominent face with a sly crooked grin and slightly raised eyebrow, bold painterly contours, deliberately illustrated shadows, readable shapes at 50x100 pixels, humorous sinister personality. Head fully visible including ears, upper body down to mid-chest, slight three-quarter pose, head occupies upper half, shoulders lower half. Flat very dark desaturated blue backdrop, opaque image. No 3D-rendered surfaces, no photorealism, no UI frame, no text, numbers or symbols, no added equipment, no characters from other games. Output a single portrait image ready to use as game art.


## Historical placement audit against costume previews

This superseded audit evaluated 632 derived patches against the costume previews; the current neutral-base delivery is described below.
All source features and base portraits are unchanged. Technical checks passed, visual acceptance remains open; the former 422 correction flags were unsupported and have been withdrawn.
The [individual findings](variants/README.md#individual-findings) now cover all 632 options and 34 creature/gender combinations, with exact filenames, observations, actions and local evidence.
Of these, 404 show placement problems, 128 show overlap problems, 21 have appearance differences with unresolved cause, and 79 have no obvious local defect; none of these counts establishes required regeneration.
No new artwork was generated for this preparation; game composition is a separate task.


## Neutral Dungeonbook assets

[The inventory](generation-inventory.json) combines all saved source-generation records,
exact prompts, source hashes, clothing attempts and current per-file findings.
Original preview illustrations and the 632 original feature parts are preserved.
The two short neutral canvases repeat their final row without rescaling;
[height-fix provenance](neutral-bases/canvas-height-fixes.json) records original and fixed hashes.
Final manifests use only Base, Slot and Option rows, with neutral bases and at least
one full-canvas clothing option per portrait; game composition remains separate.
The [current findings](neutral-variants/feature-review.md) record exact file observations
and fit resolutions; [acceptance records](neutral-variants/visual-acceptance.json) bind
reviewed combinations to their delivered file hashes. Existing source regeneration
has not been required; only the thirty missing clothing sources were generated.
