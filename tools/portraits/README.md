# Creature portrait export

This maintained tool exports the game's existing creature portraits to PNG files
for asset work and visual comparison. It reuses
[`CreaturePortrait.cpp`](../../source/render/CreaturePortrait.cpp), including its
framing, clipping correction, cache and cleanup, instead of copying that renderer.
It discovers unique mesh names from `config/creatures.cfg`.

These are model-rendered reference images, not newly illustrated artwork.
The tool does not change the game's portrait assets or runtime behavior.

## Windows prerequisites

Use the Windows build prerequisites: MSVC x64 and the project's installed OGRE/CEGUI libraries, render plugins, codecs
and media. No extra packages or game installation are required. The wrapper
loads the repository environment helper when MSVC is not already available.
On another workstation, load your own x64 compiler environment first and pass
your dependency installation prefix explicitly.

## Build and export

From the repository root:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/win32/export-creature-portraits.ps1
```

The default output is `build/portrait-export/`. To select the installed libraries
and an output folder:

```powershell
./scripts/win32/export-creature-portraits.ps1 -DependencyPrefix 'C:/dev/od-deps/install' -OutputDirectory 'build/portrait previews'
```

The C++ entry point and build wrapper are tracked source; generated executables,
object files, logs and PNGs are build outputs. Repeating the command regenerates
files with the same names in the selected output directory. Existing local
preview scripts are not needed. The export uses a hidden rendering window and
does not launch the game or access saved games or user settings.

Each configured mesh produces `portrait-<mesh-name>.png`; the existing Kobold and
Orc GUI composition checks additionally produce `portrait-gui-<mesh-name>.png`.
Compilation output, renderer messages and results use the `portrait-export-`
prefix. Successful runs report `PORTRAITS=<count>` and return zero.

## Verification

The exporter checks nonempty rendered pixels, cache reuse, preservation of world
shadow parameters, and cleanup of temporary scenes, materials, viewports and
textures. A missing configuration, empty mesh set or rendering failure returns
an error. The wrapper reports compiler/export failures and preserves their logs.

The Windows build and hidden export passed for all 33 configured meshes on
September 7, 2026, including an output path containing spaces. These checks
validate the existing export path; artistic acceptance of future illustrated
portraits remains separate. Linux and other render plugins are not validated
by this Windows wrapper.


## Existing isolated feature placement

`prepare_existing_features.py` reads the existing isolated feature PNGs and the tracked feature catalog/layouts; it writes derived RGBA patches and manifests under `materials/portraits/variants/`, and local review composites under `work/existing-feature-review/`.
It uses the same already available Pillow dependency as the grid helper and does not call an image-generation service or modify its input PNGs.
Run `python tools/portraits/validate_existing_features.py` to check the delivery contract, source/base hashes and correction statuses.
See the [delivery report](../../materials/portraits/variants/README.md) for known fit failures and the limits of technical validation.
The individual audit records each existing file, its visible observation, cause or uncertainty, required action and local comparison evidence.
`reviewed-local-only` means no obvious defect in that single-option comparison; it does not mean final combined acceptance.
Correction flags concern current composites, including unresolved appearance differences; they do not count required regenerations.

## Neutral-base registration and frozen manifest contract

`prepare_neutral_bases.py` writes 34 separate 887x1774 delivery copies and provenance;
only the last row is repeated for the two short canvases, preserving every source pixel.
`prepare_neutral_features.py --portrait <catalog-id>` registers existing isolated sources
using `neutral-feature-layouts.json` and writes candidate patches and evidence under
`materials/portraits/neutral-variants/`; repeat the option to process a creature pair.
`validate_neutral_features.py` checks original hashes, preserved base pixels and candidate
dimensions, without granting visual acceptance.
`python scripts/check_portrait_manifests.py` checks the final completion contract under
`materials/portraits/variants/` using only the Python standard library; optional repeated
`--portrait` arguments limit it to pilot images. It requires neutral Base references,
a full-canvas outfit slot with at least one option, the current draw order, and the
four existing helmets for Knight/Cultist. Base, Slot and Option rows retain their delivered meanings and source dimensions.
Optional `Rule<TAB>slot<TAB>n<TAB>x<TAB>y<TAB>w<TAB>h` sets only that option's destination rectangle;
`Mask<TAB>slot<TAB>n<TAB>earlier-slot` protects the selected earlier part's alpha.
`SourcePolygonMask<TAB>slot<TAB>n<TAB>feather<TAB>x1<TAB>y1<TAB>...` excludes a measured source contour;
feather is the outside transition width in source pixels. No source colours are changed.
Absent rules preserve the original pixels and slot placement. Scaling uses nearest
source pixel centers without reflection or rotation. Option numbers and draw order
stay unchanged. Derived reviews can be isolated with `render_manifest_reviews.py --out`;
the frozen source and existing review rasters are never overwritten during these checks.
helmet damage is clipped in the delivered scar alpha, not by an extra manifest row.
Every source option must appear in the native and 50x100 composite checks.
`prepare_outfit_variants.py` derives registration copies without modifying generator files;
`deliver_neutral_variants.py` copies final options into their catalog folders;
`render_manifest_reviews.py` composes exactly the delivered manifest order;
`verify_portrait_sources.py` checks the independent source hashes.
These asset/tool changes do not change runtime behavior or require a runtime version bump.

`render_native_check_sheets.py --portrait <catalog-id>` records six literal-manifest combinations covering every option, with unresampled face/body panels and twelve 50x100 previews. `visual-acceptance.json` records acceptance only after visual inspection and ties it to the delivered file hashes; the neutral validator rejects stale acceptance records.

`measure_base_tints.py` measures species-colour pixels in each actual neutral base,
writes normalized skin bounds to `config/dungeonbook-base-tints.cfg`, and records
pixel bounds and source hashes in `base-tint-measurements.json`. All inspected
bases have blank faces and no hair or beard; absent regions are recorded without
inventing boxes from feature placement. Tint shifts remain neutral.

Completion checks read the delivered manifests in `materials/portraits/variants/`
and their canonical references in `materials/portraits/neutral-bases/`, not local
preparation copies. They retain source SHA-256 checks against the saved inventory,
reconstruct all twelve composites, compare the committed 50x100 sheet pixel for
pixel, and validate native evidence dimensions and complete option coverage.
The two additional tint blocks are checked against the tracked test bases;
all 34 measured production rectangles remain mandatory. These checks run with
tracked repository files alone; generator-cache paths are provenance only.
