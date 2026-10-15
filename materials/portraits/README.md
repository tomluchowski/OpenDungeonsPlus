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
No original reference-game artwork is included.

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

## Goblin.mesh

Identity reference: `build/portrait-export/portrait-Goblin.mesh.png`.
Output: `materials/textures/portrait-Goblin.mesh.png`.

Create a finished 2D painted game portrait from this original project's goblin model reference, preserving its identity rather than rendering the model. One narrow vertical portrait, exactly 1:2 width-to-height composition. Subject: bald gray-olive green goblin, huge pointed ears, bright pale green eyes, wide nose, small pointed teeth, lean bare upper torso; all these identity features must match the supplied reference. Reinterpret as an expressive, mischievous hand-painted late-1990s dark-fantasy strategy-game character card: prominent face with a sly crooked grin and slightly raised eyebrow, bold painterly contours, deliberately illustrated shadows, readable shapes at 50x100 pixels, humorous sinister personality. Head fully visible including ears, upper body down to mid-chest, slight three-quarter pose, head occupies upper half, shoulders lower half. Flat very dark desaturated blue backdrop, opaque image. No 3D-rendered surfaces, no photorealism, no UI frame, no text, numbers or symbols, no added equipment, no reference-game characters. Output a single portrait image ready to use as game art.


## Existing feature placement candidates

The [feature delivery](variants/README.md) contains 632 derived patches and 34 manifests made from the existing isolated assets.
All source features and base portraits are unchanged. Technical checks passed, but 422 option composites have documented fit problems and visual acceptance remains open.
No new artwork was generated for this preparation; game composition is a separate task.
