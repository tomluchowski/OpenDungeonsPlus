# Neutral Dungeonbook registration

All 34 canonical bases are 887x1774. The two originally short canvases repeat
only the final row; original files and pixel-preservation evidence are retained.
The 34 manifests reuse 632 existing features and 16 existing helmets, plus 40
selected clothing overlays: ten already existed and thirty were added for bases
without clothing. This gives 688 manifest options. Technical validation and
visual acceptance remain separate; pending findings are not source defects.

Manifests contain only TAB-separated Base, Slot and Option rows. Draw order:
build, outfit, hair, ears, eyes, nose, mouth, chin, helmet, scar, neck.
Existing option numbers remain unchanged. Derived fitted copies from the earlier
contract remain local evidence and are not referenced by the current manifests.
Helmet damage is baked into the scar alpha common to all four available helmets.

See [exact findings](feature-review.md) and [machine-readable findings](feature-review.json).
Every option appears in the twelve recorded combinations for its base, rendered
at native size and 50x100. Review images are evidence, not automatic acceptance.
No existing source has been proved to require regeneration. Original previews,
source parts and helmets are checked with SHA-256 separately from derived fits.

Asset and tool changes do not alter game code or require a runtime version bump.

All 34 deliveries have native and 50x100 acceptance tied to their hashes;
all concrete findings, including the six Lizardman findings, are closed.
`base-tint-measurements.json` records actual neutral-base colour-pixel bounds,
source hashes and normalized rectangles for the 34 skin regions in
`config/dungeonbook-base-tints.cfg`. Hair, eyes and beard are absent from these
blank neutral bases, so their regions are explicitly recorded as absent.
No feature placement rectangle is used for tint measurement.
