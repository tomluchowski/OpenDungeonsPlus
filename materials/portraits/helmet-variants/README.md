# Face-covering helmet variants

Owner-approved addition: four helmet shapes for Knight and Cultist, each male and female, totaling 16 directly generated isolated parts.

Original generator files are copied without cropping or pixel edits into [generated-helmets](../generated-helmets/); exact prompts, references and generator paths are in its catalog and per-file records.
These folders contain separately derived RGBA helmet patches, supplemental TAB-separated manifests, exact placement transforms, and diagnostic composites; they do not replace the existing 632 patches or their manifests.

Selection rules: choose exactly one of helmet options 1–4; option 0 is invalid on a featureless neutral head.
Draw order: build, hair, helmet, scar, neck; helmet scratches must be clipped to the selected helmet alpha.
The manifests reference the neutral bases via relative paths; Knight male's referenced base is intentionally absent and must not be substituted with the original clothed portrait.
Game-side loading, selection and masking remain the separate runtime integration task.

Validation: all 16 selected sources are unchanged copies, with RGBA alpha ranging from 0 to 255; all 16 derived patches fit the 887x950 slot with complete contours inside the 887x1774 portrait.
Twelve individual neutral-base composites were inspected in original size and at 50x100.
Knight male's four parts were inspected as isolated artwork only; neutral-base visual acceptance is deferred with its missing base.
Combined compatibility with existing clothing, hair, helmet damage and neck overlays remains open alongside their already documented registration findings; single-helmet inspection does not complete the whole portrait plan.

Ritter/Knight male: [four isolated source parts](../generated-helmets/Knight.mesh/), [placement and manifest](Knight.mesh/).
Ritter/Knight female: [four isolated source parts](../generated-helmets/Knight.mesh-female/), [overview](Knight.mesh-female/review/overview.jpg).
Cultist male: [four isolated source parts](../generated-helmets/Cultist.mesh/), [overview](Cultist.mesh/review/overview.jpg).
Cultist female: [four isolated source parts](../generated-helmets/Cultist.mesh-female/), [overview](Cultist.mesh-female/review/overview.jpg).

Two first candidates were rejected: Knight female pointed visor faced the wrong direction, and Cultist female ridged helm contained a background haze.
The direction-corrected and clean newly generated replacements are selected in the catalog; rejected candidate records remain under local work/helmet-rejected and the original generator files remain preserved.
No existing feature or neutral base was regenerated.
