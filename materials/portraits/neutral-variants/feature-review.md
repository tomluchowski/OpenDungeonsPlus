# Concrete portrait findings

Per-file findings; placement corrections do not prove source regeneration is required.

## Kobold.mesh - build-1-slim.png

Status: closed-native-and-50x100

The source ends at the upper arms and upper torso; registered shoulder shading and contour transitions remain visible at native size.

Cause: The existing source covers only the upper torso, whereas the neutral base contains the complete visible body.

Required action: Check the derived body transition beneath the available clothing and both body options.

Evidence: Kobold.mesh/review/combined-1.png

Resolution: Continuous sleeve registration covers both body options; preserved background gaps remain unchanged. Scar shifted onto the cheek. Native panels and twelve 50x100 composites checked.

Final evidence: ../../variants/Kobold.mesh/review/native-body-check.jpg

## Kobold.mesh - build-2-sturdy.png

Status: closed-native-and-50x100

The source ends at the upper arms and upper torso; registered shoulder shading and contour transitions remain visible at native size.

Cause: The existing source covers only the upper torso, whereas the neutral base contains the complete visible body.

Required action: Check the derived body transition beneath the available clothing and both body options.

Evidence: Kobold.mesh/review/combined-2.png

Resolution: Continuous sleeve registration covers both body options; preserved background gaps remain unchanged. Scar shifted onto the cheek. Native panels and twelve 50x100 composites checked.

Final evidence: ../../variants/Kobold.mesh/review/native-body-check.jpg

## Kobold.mesh-female - build-1-slim.png

Status: closed-native-and-50x100

The source ends at the upper arms and upper torso; registered shoulder shading and contour transitions remain visible at native size.

Cause: The existing source covers only the upper torso, whereas the neutral base contains the complete visible body.

Required action: Check the derived body transition beneath the available clothing and both body options.

Evidence: Kobold.mesh-female/review/combined-1.png

Resolution: Continuous sleeve registration covers both body options; preserved background gaps remain unchanged. Scar shifted onto the cheek. Native panels and twelve 50x100 composites checked.

Final evidence: ../../variants/Kobold.mesh-female/review/native-body-check.jpg

## Kobold.mesh-female - build-2-sturdy.png

Status: closed-native-and-50x100

The source ends at the upper arms and upper torso; registered shoulder shading and contour transitions remain visible at native size.

Cause: The existing source covers only the upper torso, whereas the neutral base contains the complete visible body.

Required action: Check the derived body transition beneath the available clothing and both body options.

Evidence: Kobold.mesh-female/review/combined-2.png

Resolution: Continuous sleeve registration covers both body options; preserved background gaps remain unchanged. Scar shifted onto the cheek. Native panels and twelve 50x100 composites checked.

Final evidence: ../../variants/Kobold.mesh-female/review/native-body-check.jpg

## Dwarf1.mesh - hair-1-braid.png

Status: closed-native-and-50x100

The hair source includes a complete moustache and beard plus the side braid.

Cause: The generated hair asset contains more than the intended hair-slot content.

Required action: Retain the side braid using the recorded derived alpha polygon; inspect the temple attachment.

Evidence: ../generated-features/Dwarf1.mesh/hair-1-braid.png

Resolution: Both hair roots feathered on derived copies; separate brooch and necklace anchors retained, necklace cord fitted behind the beard union. All delivered options checked in native panels and twelve 50x100 composites.

Final evidence: ../../variants/Dwarf1.mesh/review/native-body-check.jpg

## Dwarf1.mesh - neck-2-tooth.png

Status: closed-native-and-50x100

The necklace shared the right-chest brooch anchor.

Cause: Two accessories with different attachment locations used one centred transform.

Required action: Use the separate neck-centred transform already recorded for this option and inspect beneath the beard.

Evidence: Dwarf1.mesh/review/combined-2.png

Resolution: Both hair roots feathered on derived copies; separate brooch and necklace anchors retained, necklace cord fitted behind the beard union. All delivered options checked in native panels and twelve 50x100 composites.

Final evidence: ../../variants/Dwarf1.mesh/review/native-body-check.jpg

## Dwarf1.mesh-female - chin-1-square.png

Status: closed-native-and-50x100

Surrounding skin boundaries remain visible below the mouth in the individual overview.

Cause: Different source skin shading meets the neutral face at the patch contour.

Required action: Inspect and adjust the derived contour feathering with all mouth variants.

Evidence: Dwarf1.mesh-female/review/overview.jpg

Resolution: Transparent padded alpha filtering removes the rectangular patch boundaries; all delivered options were inspected in native face/body panels and all twelve 50x100 composites.

Final evidence: ../../variants/Dwarf1.mesh-female/review/native-face-check.jpg

## Dwarf1.mesh-female - chin-2-rounded.png

Status: closed-native-and-50x100

Surrounding skin boundaries remain visible below the mouth in the individual overview.

Cause: Different source skin shading meets the neutral face at the patch contour.

Required action: Inspect and adjust the derived contour feathering with all mouth variants.

Evidence: Dwarf1.mesh-female/review/overview.jpg

Resolution: Transparent padded alpha filtering removes the rectangular patch boundaries; all delivered options were inspected in native face/body panels and all twelve 50x100 composites.

Final evidence: ../../variants/Dwarf1.mesh-female/review/native-face-check.jpg

## Orc.mesh-female - scar-1-diagonal.png

Status: closed-native-and-50x100

In the literal manifest composite, the upper scar crossed the ear root.

Cause: Scar is drawn after ears and its current alpha extended into the ear area.

Required action: Move the scar into the cheek and bake alpha avoidance against all ear and hair options; inspect the literal manifest composite.

Evidence: ../variants/Orc.mesh-female/review/combined-1.png

Resolution: Derived scar placement avoids the ear root; padded alpha feathering removes the mouth boundary. Every delivered option reviewed at native scale and in twelve 50x100 composites.

Final evidence: ../../variants/Orc.mesh-female/review/native-face-check.jpg

## Orc.mesh-female - mouth-1-tusks.png

Status: closed-native-and-50x100

A horizontal surrounding-skin edge was visible below the right mouth corner.

Cause: The mouth source includes skin that ends at the lower contour.

Required action: Apply the derived lower-edge feather already implemented and recheck every chin combination.

Evidence: ../variants/Orc.mesh-female/review/combined-1.png

Resolution: Derived scar placement avoids the ear root; padded alpha feathering removes the mouth boundary. Every delivered option reviewed at native scale and in twelve 50x100 composites.

Final evidence: ../../variants/Orc.mesh-female/review/native-face-check.jpg

## Knight.mesh - outfit-1-plate-build0.png

Status: closed-native-and-50x100

The new outfit has its bent sleeves and belt roughly 200 pixels above the neutral body anchors; the raw composite exposes a second pair of arms.

Cause: New clothing geometry is vertically compressed relative to the saved neutral body.

Required action: Correct this newly generated outfit within the three permitted attempts; preserve the existing 632 sources and the neutral base.

Evidence: ../../../work/knight-outfit-2-raw-composite.png

Resolution: Existing corrected clothing sources fitted to both body options; helmets cover the neutral head. Every delivered option reviewed at native size and twelve 50x100 composites.

Final evidence: ../../variants/Knight.mesh/review/native-body-check.jpg

## Knight.mesh - outfit-1-plate-build0.png

Status: closed-native-and-50x100

The new outfit has its bent sleeves and belt roughly 200 pixels above the neutral body anchors; the raw composite exposes a second pair of arms.

Cause: New clothing geometry is vertically compressed relative to the saved neutral body.

Required action: Correct this newly generated outfit within the three permitted attempts; preserve the existing 632 sources and the neutral base.

Evidence: ../../../work/knight-outfit-3-raw-composite.png

Resolution: Existing corrected clothing sources fitted to both body options; helmets cover the neutral head. Every delivered option reviewed at native size and twelve 50x100 composites.

Final evidence: ../../variants/Knight.mesh/review/native-body-check.jpg

## Knight.mesh-female - outfit-1-plate-build0.png

Status: closed-native-and-50x100

The new armor leaves neutral forearm wedges visible at both sides between rows 1250 and 1550.

Cause: The generated sleeve openings are larger than the neutral forearm outline.

Required action: Correct the new clothing source to cover the existing neutral arms; preserve the base and all existing feature sources.

Evidence: Knight.mesh-female/review/outfits/outfit-1-plate-build0.png

Resolution: Existing corrected clothing sources fitted to both body options; helmets cover the neutral head. Every delivered option reviewed at native size and twelve 50x100 composites.

Final evidence: ../../variants/Knight.mesh-female/review/native-body-check.jpg

## Goblin.mesh - chin-1-forked.png

Status: closed-native-and-50x100

The native composite has a hard skin transition at the upper jaw patch.

Cause: Source contour and neutral geometry differ at this attachment.

Required action: Feather the derived jaw alpha and check both chin options with all mouth options.

Evidence: Goblin.mesh/review/combined-1.png

Resolution: Padded face feathering, crown anchoring and wider continuous garment registration remove the documented boundaries and exposed arm strips; all options reviewed at native size and 50x100.

Final evidence: ../../variants/Goblin.mesh/review/native-body-check.jpg

## Goblin.mesh - outfit-1-patched-build0.png

Status: closed-native-and-50x100

Visible neutral forearm pixels remain at the lower sleeve edge.

Cause: Source contour and neutral geometry differ at this attachment.

Required action: Register sleeve width to cover the neutral and both existing body options, without changing the source.

Evidence: Goblin.mesh/review/combined-1.png

Resolution: Padded face feathering, crown anchoring and wider continuous garment registration remove the documented boundaries and exposed arm strips; all options reviewed at native size and 50x100.

Final evidence: ../../variants/Goblin.mesh/review/native-body-check.jpg

## Goblin.mesh-female - chin-1-square.png

Status: closed-native-and-50x100

The native composite has a hard skin transition at the upper jaw patch.

Cause: Source contour and neutral geometry differ at this attachment.

Required action: Feather the derived jaw alpha and check both chin options with all mouth options.

Evidence: Goblin.mesh-female/review/combined-1.png

Resolution: Padded face feathering, crown anchoring and wider continuous garment registration remove the documented boundaries and exposed arm strips; all options reviewed at native size and 50x100.

Final evidence: ../../variants/Goblin.mesh-female/review/native-body-check.jpg

## Goblin.mesh-female - outfit-1-patched-build0.png

Status: closed-native-and-50x100

Visible neutral forearm pixels remain at the lower sleeve edge.

Cause: Source contour and neutral geometry differ at this attachment.

Required action: Register sleeve width to cover the neutral and both existing body options, without changing the source.

Evidence: Goblin.mesh-female/review/combined-1.png

Resolution: Padded face feathering, crown anchoring and wider continuous garment registration remove the documented boundaries and exposed arm strips; all options reviewed at native size and 50x100.

Final evidence: ../../variants/Goblin.mesh-female/review/native-body-check.jpg

## Cultist.mesh-female - helmet-1-crest.png

Status: closed-native-and-50x100

The literal combined review shows a dark gap between the helmet lower edge and the robe collar; inspect all four helmet options against both builds.

Cause: Existing helmet registration and the new robe collar do not share the neutral head and neck anchors.

Required action: Adjust existing helmet or robe registration locally and inspect every helmet at native size and 50x100; preserve all raw sources.

Evidence: ../../variants/Cultist.mesh-female/review/combined-native-sheet.jpg

Resolution: Old costume neckline masked on both derived build options; all four helmets and every delivered option checked at native size and 50x100. The earlier dark gap observation was neutral-neck shading, not a missing background region; the collar boundary is now continuous.

Final evidence: ../../variants/Cultist.mesh-female/review/native-face-check.jpg

## Dwarf1.mesh-female - eyes-1-narrow.png

Status: closed-native-and-50x100

Native combined review shows a hard skin edge at the rectangular option boundary.

Cause: Previous alpha filtering replicated opaque edge pixels instead of treating pixels outside the slot as transparent.

Required action: Use transparent padded alpha filtering, then inspect every facial option in native and 50x100 composites.

Evidence: ../../variants/Dwarf1.mesh-female/review/combined-1.png

Resolution: Transparent padded alpha filtering removes the rectangular patch boundaries; all delivered options were inspected in native face/body panels and all twelve 50x100 composites.

Final evidence: ../../variants/Dwarf1.mesh-female/review/native-face-check.jpg

## Monk.mesh-female - eyes-1-narrow.png

Status: closed-native-and-50x100

Native combined review shows a hard skin edge at the rectangular option boundary.

Cause: Previous alpha filtering replicated opaque edge pixels instead of treating pixels outside the slot as transparent.

Required action: Use transparent padded alpha filtering, then inspect every facial option in native and 50x100 composites.

Evidence: ../../variants/Monk.mesh-female/review/combined-1.png

Resolution: Hair crown anchored at the top, ears fitted to the neutral head, and padded alpha feathering removes rectangular facial boundaries. Every option reviewed at native scale and twelve 50x100 composites.

Final evidence: ../../variants/Monk.mesh-female/review/native-face-check.jpg

## Monk.mesh-female - nose-1-crooked.png

Status: closed-native-and-50x100

Native combined review shows a hard skin edge at the rectangular option boundary.

Cause: Previous alpha filtering replicated opaque edge pixels instead of treating pixels outside the slot as transparent.

Required action: Use transparent padded alpha filtering, then inspect every facial option in native and 50x100 composites.

Evidence: ../../variants/Monk.mesh-female/review/combined-1.png

Resolution: Hair crown anchored at the top, ears fitted to the neutral head, and padded alpha feathering removes rectangular facial boundaries. Every option reviewed at native scale and twelve 50x100 composites.

Final evidence: ../../variants/Monk.mesh-female/review/native-face-check.jpg

## Monk.mesh-female - mouth-1-teeth.png

Status: closed-native-and-50x100

Native combined review shows a hard skin edge at the rectangular option boundary.

Cause: Previous alpha filtering replicated opaque edge pixels instead of treating pixels outside the slot as transparent.

Required action: Use transparent padded alpha filtering, then inspect every facial option in native and 50x100 composites.

Evidence: ../../variants/Monk.mesh-female/review/combined-1.png

Resolution: Hair crown anchored at the top, ears fitted to the neutral head, and padded alpha feathering removes rectangular facial boundaries. Every option reviewed at native scale and twelve 50x100 composites.

Final evidence: ../../variants/Monk.mesh-female/review/native-face-check.jpg

## Monk.mesh-female - chin-1-square.png

Status: closed-native-and-50x100

Native combined review shows a hard skin edge at the rectangular option boundary.

Cause: Previous alpha filtering replicated opaque edge pixels instead of treating pixels outside the slot as transparent.

Required action: Use transparent padded alpha filtering, then inspect every facial option in native and 50x100 composites.

Evidence: ../../variants/Monk.mesh-female/review/combined-1.png

Resolution: Hair crown anchored at the top, ears fitted to the neutral head, and padded alpha feathering removes rectangular facial boundaries. Every option reviewed at native scale and twelve 50x100 composites.

Final evidence: ../../variants/Monk.mesh-female/review/native-face-check.jpg

## Cultist.mesh-female - build-1-slim.png

Status: closed-native-and-50x100

Native review shows bright red old collar fragments beside the new robe neckline.

Cause: The preserved build source contains the original costume, which extends above the new robe.

Required action: Mask the old neckline on derived build copies, then recheck both body choices with every helmet.

Evidence: ../../variants/Cultist.mesh-female/review/native-face-check.jpg

Resolution: Old costume neckline masked on both derived build options; all four helmets and every delivered option checked at native size and 50x100. The earlier dark gap observation was neutral-neck shading, not a missing background region; the collar boundary is now continuous.

Final evidence: ../../variants/Cultist.mesh-female/review/native-face-check.jpg

## Cultist.mesh-female - build-2-sturdy.png

Status: closed-native-and-50x100

Native review shows bright red old collar fragments beside the new robe neckline.

Cause: The preserved build source contains the original costume, which extends above the new robe.

Required action: Mask the old neckline on derived build copies, then recheck both body choices with every helmet.

Evidence: ../../variants/Cultist.mesh-female/review/native-face-check.jpg

Resolution: Old costume neckline masked on both derived build options; all four helmets and every delivered option checked at native size and 50x100. The earlier dark gap observation was neutral-neck shading, not a missing background region; the collar boundary is now continuous.

Final evidence: ../../variants/Cultist.mesh-female/review/native-face-check.jpg

## Monk.mesh - hair-2-waves.png

Status: closed-native-and-50x100

The second hairstyle was vertically centred inside its tall slot and left an unintended exposed scalp above the fringe.

Cause: Whole-image bounds were centred even when source aspect ratio left unused slot height.

Required action: Anchor the existing hair source at the measured crown rather than the slot centre and recheck both hairstyles.

Evidence: ../../variants/Monk.mesh/review/native-face-check.jpg

Resolution: Hair crown anchored at the top, ears fitted to the neutral head, and padded alpha feathering removes rectangular facial boundaries. Every option reviewed at native scale and twelve 50x100 composites.

Final evidence: ../../variants/Monk.mesh/review/native-face-check.jpg

## Monk.mesh-female - hair-2-waves.png

Status: closed-native-and-50x100

The second hairstyle was vertically centred inside its tall slot and left an unintended exposed scalp above the fringe.

Cause: Whole-image bounds were centred even when source aspect ratio left unused slot height.

Required action: Anchor the existing hair source at the measured crown rather than the slot centre and recheck both hairstyles.

Evidence: ../../variants/Monk.mesh-female/review/native-face-check.jpg

Resolution: Hair crown anchored at the top, ears fitted to the neutral head, and padded alpha feathering removes rectangular facial boundaries. Every option reviewed at native scale and twelve 50x100 composites.

Final evidence: ../../variants/Monk.mesh-female/review/native-face-check.jpg

## Kobold.mesh - outfit-1-patched-build0.png

Status: closed-native-and-50x100

Horizontal texture streaks and abrupt sleeve/torso jumps visible in native body panels.

Cause: Earlier row-span extension changes sampling width abruptly between rows.

Required action: Refit the existing clothing source using continuous sleeve extensions and inspect native and 50x100 composites.

Evidence: ../../variants/Kobold.mesh/review/native-body-check.jpg

Resolution: Continuous sleeve registration covers both body options; preserved background gaps remain unchanged. Scar shifted onto the cheek. Native panels and twelve 50x100 composites checked.

Final evidence: ../../variants/Kobold.mesh/review/native-body-check.jpg

## Kobold.mesh-female - outfit-1-patched-build0.png

Status: closed-native-and-50x100

Horizontal texture streaks and abrupt sleeve/torso jumps visible in native body panels.

Cause: Earlier row-span extension changes sampling width abruptly between rows.

Required action: Refit the existing clothing source using continuous sleeve extensions and inspect native and 50x100 composites.

Evidence: ../../variants/Kobold.mesh-female/review/native-body-check.jpg

Resolution: Continuous sleeve registration covers both body options; preserved background gaps remain unchanged. Scar shifted onto the cheek. Native panels and twelve 50x100 composites checked.

Final evidence: ../../variants/Kobold.mesh-female/review/native-body-check.jpg

## Kobold.mesh - scar-1-diagonal.png

Status: closed-native-and-50x100

Lower scar tip extends outside the left cheek in the native composite.

Cause: Scar slot too far left on the neutral head.

Required action: Move scar slot 35 pixels right; inspect all scar options.

Evidence: ../../variants/Kobold.mesh/review/native-face-check.jpg

Resolution: Continuous sleeve registration covers both body options; preserved background gaps remain unchanged. Scar shifted onto the cheek. Native panels and twelve 50x100 composites checked.

Final evidence: ../../variants/Kobold.mesh/review/native-body-check.jpg

## Goblin.mesh-female - hair-2-waves.png

Status: closed-native-and-50x100

Hair leaves a bare crown strip when centered vertically.

Cause: Alpha bounds of the shorter hairstyle centered in the tall hair slot.

Required action: Anchor existing hair at the crown and review both hairstyles.

Evidence: ../../variants/Goblin.mesh-female/review/native-face-check.jpg

Resolution: Padded face feathering, crown anchoring and wider continuous garment registration remove the documented boundaries and exposed arm strips; all options reviewed at native size and 50x100.

Final evidence: ../../variants/Goblin.mesh-female/review/native-body-check.jpg

## Dwarf2.mesh - eyes-2-round.png

Status: closed-native-and-50x100

Eye-source skin extends outside the neutral head right contour.

Cause: Slot position and scale do not match the neutral head.

Required action: Reduce eye-slot width to 385 pixels and height to 165 pixels.

Evidence: ../../variants/Dwarf2.mesh/review/native-face-check.jpg

Resolution: Eye width fitted to head contour; side-hair root and crown aligned; neck accessory restored beneath the beard occlusion mask. All options viewed at native scale and 50x100.

Final evidence: ../../variants/Dwarf2.mesh/review/native-face-check.jpg

## Dwarf2.mesh - hair-2-waves.png

Status: closed-native-and-50x100

Hair root does not meet the neutral crown/temple.

Cause: Slot position and scale do not match the neutral head.

Required action: Anchor female hair at the crown; move male side hair to the temple.

Evidence: ../../variants/Dwarf2.mesh/review/native-face-check.jpg

Resolution: Eye width fitted to head contour; side-hair root and crown aligned; neck accessory restored beneath the beard occlusion mask. All options viewed at native scale and 50x100.

Final evidence: ../../variants/Dwarf2.mesh/review/native-face-check.jpg

## Dwarf2.mesh-female - eyes-2-round.png

Status: closed-native-and-50x100

Eye-source skin extends outside the neutral head right contour.

Cause: Slot position and scale do not match the neutral head.

Required action: Reduce eye-slot width to 385 pixels and height to 165 pixels.

Evidence: ../../variants/Dwarf2.mesh-female/review/native-face-check.jpg

Resolution: Eye width fitted to head contour; side-hair root and crown aligned; neck accessory restored beneath the beard occlusion mask. All options viewed at native scale and 50x100.

Final evidence: ../../variants/Dwarf2.mesh-female/review/native-face-check.jpg

## Dwarf2.mesh-female - hair-2-waves.png

Status: closed-native-and-50x100

Hair root does not meet the neutral crown/temple.

Cause: Slot position and scale do not match the neutral head.

Required action: Anchor female hair at the crown; move male side hair to the temple.

Evidence: ../../variants/Dwarf2.mesh-female/review/native-face-check.jpg

Resolution: Eye width fitted to head contour; side-hair root and crown aligned; neck accessory restored beneath the beard occlusion mask. All options viewed at native scale and 50x100.

Final evidence: ../../variants/Dwarf2.mesh-female/review/native-face-check.jpg

## Dwarf2.mesh - neck-2-torque.png

Status: closed-native-and-50x100

Neck ornament lies across the beard.

Cause: Neck slot too high and missing beard occlusion.

Required action: Move neck slot lower and bake inverse beard alpha union into necklaces.

Evidence: ../../variants/Dwarf2.mesh/review/native-body-check.jpg

Resolution: Eye width fitted to head contour; side-hair root and crown aligned; neck accessory restored beneath the beard occlusion mask. All options viewed at native scale and 50x100.

Final evidence: ../../variants/Dwarf2.mesh/review/native-face-check.jpg

## RunelordDwarf.mesh - hair-1-swept.png

Status: closed-native-and-50x100

Hair roots centred below the temples, leaving an unintended bald crown.

Cause: Vertical centring of an asymmetric hair source.

Required action: Anchor the hair silhouette at the top of its head slot.

Evidence: ../../variants/RunelordDwarf.mesh/review/native-face-check.jpg

Resolution: Side-hair repositioned and masked behind the neutral head silhouette; neck overlays masked behind beard options. All options checked at native scale and 50x100.

Final evidence: ../../variants/RunelordDwarf.mesh/review/native-face-check.jpg

## RunelordDwarf.mesh - hair-2-waves.png

Status: closed-native-and-50x100

Hair roots centred below the temples, leaving an unintended bald crown.

Cause: Vertical centring of an asymmetric hair source.

Required action: Anchor the hair silhouette at the top of its head slot.

Evidence: ../../variants/RunelordDwarf.mesh/review/native-face-check.jpg

Resolution: Side-hair repositioned and masked behind the neutral head silhouette; neck overlays masked behind beard options. All options checked at native scale and 50x100.

Final evidence: ../../variants/RunelordDwarf.mesh/review/native-face-check.jpg

## RunelordDwarf.mesh-female - hair-1-swept.png

Status: closed-native-and-50x100

Hair roots centred below the temples, leaving an unintended bald crown.

Cause: Vertical centring of an asymmetric hair source.

Required action: Anchor the hair silhouette at the top of its head slot.

Evidence: ../../variants/RunelordDwarf.mesh-female/review/native-face-check.jpg

Resolution: Side-hair repositioned and masked behind the neutral head silhouette; neck overlays masked behind beard options. All options checked at native scale and 50x100.

Final evidence: ../../variants/RunelordDwarf.mesh-female/review/native-face-check.jpg

## RunelordDwarf.mesh-female - hair-2-waves.png

Status: closed-native-and-50x100

Hair roots centred below the temples, leaving an unintended bald crown.

Cause: Vertical centring of an asymmetric hair source.

Required action: Anchor the hair silhouette at the top of its head slot.

Evidence: ../../variants/RunelordDwarf.mesh-female/review/native-face-check.jpg

Resolution: Side-hair repositioned and masked behind the neutral head silhouette; neck overlays masked behind beard options. All options checked at native scale and 50x100.

Final evidence: ../../variants/RunelordDwarf.mesh-female/review/native-face-check.jpg

## RunelordDwarf.mesh - neck-2-torque.png

Status: closed-native-and-50x100

Neck accessory drawn over the beard.

Cause: Neck draw order without beard occlusion.

Required action: Mask the neck overlay behind the union of the existing beard options.

Evidence: ../../variants/RunelordDwarf.mesh/review/native-body-check.jpg

Resolution: Side-hair repositioned and masked behind the neutral head silhouette; neck overlays masked behind beard options. All options checked at native scale and 50x100.

Final evidence: ../../variants/RunelordDwarf.mesh/review/native-face-check.jpg

## Gnome.mesh - chin-1-square.png

Status: closed-native-and-50x100

Chin patch extends below neutral jaw.

Cause: Source centred in a generic slot.

Required action: Fit the chin into the 300x100 slot at y=750, below the mouth.

Evidence: ../../variants/Gnome.mesh/review/native-face-check.jpg

Resolution: Chin fitted within jaw while preserving mouth; necklace raised to neckline; side-hair masked behind neutral head. All options checked at native scale and 50x100.

Final evidence: ../../variants/Gnome.mesh/review/native-face-check.jpg

## Gnome.mesh - chin-2-rounded.png

Status: closed-native-and-50x100

Chin patch extends below neutral jaw.

Cause: Source centred in a generic slot.

Required action: Fit the chin into the 300x100 slot at y=750, below the mouth.

Evidence: ../../variants/Gnome.mesh/review/native-face-check.jpg

Resolution: Chin fitted within jaw while preserving mouth; necklace raised to neckline; side-hair masked behind neutral head. All options checked at native scale and 50x100.

Final evidence: ../../variants/Gnome.mesh/review/native-face-check.jpg

## Gnome.mesh - neck-1-teeth.png

Status: closed-native-and-50x100

Neck ornament floats in the middle of the chest.

Cause: Source centred in a generic slot.

Required action: Move the neck slot up 120 pixels.

Evidence: ../../variants/Gnome.mesh/review/native-face-check.jpg

Resolution: Chin fitted within jaw while preserving mouth; necklace raised to neckline; side-hair masked behind neutral head. All options checked at native scale and 50x100.

Final evidence: ../../variants/Gnome.mesh/review/native-face-check.jpg

## Gnome.mesh - neck-2-torque.png

Status: closed-native-and-50x100

Neck ornament floats in the middle of the chest.

Cause: Source centred in a generic slot.

Required action: Move the neck slot up 120 pixels.

Evidence: ../../variants/Gnome.mesh/review/native-face-check.jpg

Resolution: Chin fitted within jaw while preserving mouth; necklace raised to neckline; side-hair masked behind neutral head. All options checked at native scale and 50x100.

Final evidence: ../../variants/Gnome.mesh/review/native-face-check.jpg

## Gnome.mesh-female - hair-1-swept.png

Status: closed-native-and-50x100

Side-hair roots overlap the bald forehead.

Cause: Source centred in a generic slot.

Required action: Mask the existing side hair behind the neutral head contour.

Evidence: ../../variants/Gnome.mesh-female/review/native-face-check.jpg

Resolution: Chin fitted within jaw while preserving mouth; necklace raised to neckline; side-hair masked behind neutral head. All options checked at native scale and 50x100.

Final evidence: ../../variants/Gnome.mesh-female/review/native-face-check.jpg

## Gnome.mesh-female - hair-2-waves.png

Status: closed-native-and-50x100

Side-hair roots overlap the bald forehead.

Cause: Source centred in a generic slot.

Required action: Mask the existing side hair behind the neutral head contour.

Evidence: ../../variants/Gnome.mesh-female/review/native-face-check.jpg

Resolution: Chin fitted within jaw while preserving mouth; necklace raised to neckline; side-hair masked behind neutral head. All options checked at native scale and 50x100.

Final evidence: ../../variants/Gnome.mesh-female/review/native-face-check.jpg

## Adventurer.mesh - hair-1-swept.png

Status: closed-native-and-50x100

Hair centred across the face with bald crown.

Cause: Generic centred hair registration or unmasked source costume.

Required action: Anchor hair at slot top; clip body sources to neutral skin silhouette.

Evidence: ../../variants/Adventurer.mesh/review/native-face-check.jpg

Resolution: Hair anchored at crown; female face occlusion baked into hair alpha; old costume clipped to neutral body silhouette. All options checked at native scale and 50x100.

Final evidence: ../../variants/Adventurer.mesh/review/native-face-check.jpg

## Adventurer.mesh - hair-2-waves.png

Status: closed-native-and-50x100

Hair centred across the face with bald crown.

Cause: Generic centred hair registration or unmasked source costume.

Required action: Anchor hair at slot top; clip body sources to neutral skin silhouette.

Evidence: ../../variants/Adventurer.mesh/review/native-face-check.jpg

Resolution: Hair anchored at crown; female face occlusion baked into hair alpha; old costume clipped to neutral body silhouette. All options checked at native scale and 50x100.

Final evidence: ../../variants/Adventurer.mesh/review/native-face-check.jpg

## Adventurer.mesh - build-1-slim.png

Status: closed-native-and-50x100

Old costume contours protrude into neutral arm gaps.

Cause: Generic centred hair registration or unmasked source costume.

Required action: Anchor hair at slot top; clip body sources to neutral skin silhouette.

Evidence: ../../variants/Adventurer.mesh/review/native-face-check.jpg

Resolution: Hair anchored at crown; female face occlusion baked into hair alpha; old costume clipped to neutral body silhouette. All options checked at native scale and 50x100.

Final evidence: ../../variants/Adventurer.mesh/review/native-face-check.jpg

## Adventurer.mesh - build-2-sturdy.png

Status: closed-native-and-50x100

Old costume contours protrude into neutral arm gaps.

Cause: Generic centred hair registration or unmasked source costume.

Required action: Anchor hair at slot top; clip body sources to neutral skin silhouette.

Evidence: ../../variants/Adventurer.mesh/review/native-face-check.jpg

Resolution: Hair anchored at crown; female face occlusion baked into hair alpha; old costume clipped to neutral body silhouette. All options checked at native scale and 50x100.

Final evidence: ../../variants/Adventurer.mesh/review/native-face-check.jpg

## Adventurer.mesh-female - hair-1-swept.png

Status: closed-native-and-50x100

Hair centred across the face with bald crown.

Cause: Generic centred hair registration or unmasked source costume.

Required action: Anchor hair at slot top; clip body sources to neutral skin silhouette.

Evidence: ../../variants/Adventurer.mesh-female/review/native-face-check.jpg

Resolution: Hair anchored at crown; female face occlusion baked into hair alpha; old costume clipped to neutral body silhouette. All options checked at native scale and 50x100.

Final evidence: ../../variants/Adventurer.mesh-female/review/native-face-check.jpg

## Adventurer.mesh-female - hair-2-waves.png

Status: closed-native-and-50x100

Hair centred across the face with bald crown.

Cause: Generic centred hair registration or unmasked source costume.

Required action: Anchor hair at slot top; clip body sources to neutral skin silhouette.

Evidence: ../../variants/Adventurer.mesh-female/review/native-face-check.jpg

Resolution: Hair anchored at crown; female face occlusion baked into hair alpha; old costume clipped to neutral body silhouette. All options checked at native scale and 50x100.

Final evidence: ../../variants/Adventurer.mesh-female/review/native-face-check.jpg

## Adventurer.mesh-female - build-1-slim.png

Status: closed-native-and-50x100

Old costume contours protrude into neutral arm gaps.

Cause: Generic centred hair registration or unmasked source costume.

Required action: Anchor hair at slot top; clip body sources to neutral skin silhouette.

Evidence: ../../variants/Adventurer.mesh-female/review/native-face-check.jpg

Resolution: Hair anchored at crown; female face occlusion baked into hair alpha; old costume clipped to neutral body silhouette. All options checked at native scale and 50x100.

Final evidence: ../../variants/Adventurer.mesh-female/review/native-face-check.jpg

## Adventurer.mesh-female - build-2-sturdy.png

Status: closed-native-and-50x100

Old costume contours protrude into neutral arm gaps.

Cause: Generic centred hair registration or unmasked source costume.

Required action: Anchor hair at slot top; clip body sources to neutral skin silhouette.

Evidence: ../../variants/Adventurer.mesh-female/review/native-face-check.jpg

Resolution: Hair anchored at crown; female face occlusion baked into hair alpha; old costume clipped to neutral body silhouette. All options checked at native scale and 50x100.

Final evidence: ../../variants/Adventurer.mesh-female/review/native-face-check.jpg

## Defender.mesh - hair-1-swept.png

Status: closed-native-and-50x100

Hair roots lie below the crown.

Cause: Centred hair registration or body-source costume outside the new silhouette.

Required action: Anchor hair at slot top; clip old costume to neutral silhouette and fit clothing to uncovered arms.

Evidence: ../../variants/Defender.mesh/review/native-face-check.jpg

Resolution: Head silhouette mask and top hair anchor; body clipped to neutral skin silhouette; sleeves extended over uncovered forearms; female chin moved, feathered, and masked behind all mouth options.

Final evidence: ../../variants/Defender.mesh/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Defender.mesh - hair-2-waves.png

Status: closed-native-and-50x100

Hair roots lie below the crown.

Cause: Centred hair registration or body-source costume outside the new silhouette.

Required action: Anchor hair at slot top; clip old costume to neutral silhouette and fit clothing to uncovered arms.

Evidence: ../../variants/Defender.mesh/review/native-face-check.jpg

Resolution: Head silhouette mask and top hair anchor; body clipped to neutral skin silhouette; sleeves extended over uncovered forearms; female chin moved, feathered, and masked behind all mouth options.

Final evidence: ../../variants/Defender.mesh/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Defender.mesh - build-1-slim.png

Status: closed-native-and-50x100

Old body/costume contours appear inside arm gaps.

Cause: Centred hair registration or body-source costume outside the new silhouette.

Required action: Anchor hair at slot top; clip old costume to neutral silhouette and fit clothing to uncovered arms.

Evidence: ../../variants/Defender.mesh/review/native-face-check.jpg

Resolution: Head silhouette mask and top hair anchor; body clipped to neutral skin silhouette; sleeves extended over uncovered forearms; female chin moved, feathered, and masked behind all mouth options.

Final evidence: ../../variants/Defender.mesh/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Defender.mesh - build-2-sturdy.png

Status: closed-native-and-50x100

Old body/costume contours appear inside arm gaps.

Cause: Centred hair registration or body-source costume outside the new silhouette.

Required action: Anchor hair at slot top; clip old costume to neutral silhouette and fit clothing to uncovered arms.

Evidence: ../../variants/Defender.mesh/review/native-face-check.jpg

Resolution: Head silhouette mask and top hair anchor; body clipped to neutral skin silhouette; sleeves extended over uncovered forearms; female chin moved, feathered, and masked behind all mouth options.

Final evidence: ../../variants/Defender.mesh/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Defender.mesh-female - hair-1-swept.png

Status: closed-native-and-50x100

Hair roots lie below the crown.

Cause: Centred hair registration or body-source costume outside the new silhouette.

Required action: Anchor hair at slot top; clip old costume to neutral silhouette and fit clothing to uncovered arms.

Evidence: ../../variants/Defender.mesh-female/review/native-face-check.jpg

Resolution: Head silhouette mask and top hair anchor; body clipped to neutral skin silhouette; sleeves extended over uncovered forearms; female chin moved, feathered, and masked behind all mouth options.

Final evidence: ../../variants/Defender.mesh-female/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Defender.mesh-female - hair-2-waves.png

Status: closed-native-and-50x100

Hair roots lie below the crown.

Cause: Centred hair registration or body-source costume outside the new silhouette.

Required action: Anchor hair at slot top; clip old costume to neutral silhouette and fit clothing to uncovered arms.

Evidence: ../../variants/Defender.mesh-female/review/native-face-check.jpg

Resolution: Head silhouette mask and top hair anchor; body clipped to neutral skin silhouette; sleeves extended over uncovered forearms; female chin moved, feathered, and masked behind all mouth options.

Final evidence: ../../variants/Defender.mesh-female/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Defender.mesh-female - build-1-slim.png

Status: closed-native-and-50x100

Old body/costume contours appear inside arm gaps.

Cause: Centred hair registration or body-source costume outside the new silhouette.

Required action: Anchor hair at slot top; clip old costume to neutral silhouette and fit clothing to uncovered arms.

Evidence: ../../variants/Defender.mesh-female/review/native-face-check.jpg

Resolution: Head silhouette mask and top hair anchor; body clipped to neutral skin silhouette; sleeves extended over uncovered forearms; female chin moved, feathered, and masked behind all mouth options.

Final evidence: ../../variants/Defender.mesh-female/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Defender.mesh-female - build-2-sturdy.png

Status: closed-native-and-50x100

Old body/costume contours appear inside arm gaps.

Cause: Centred hair registration or body-source costume outside the new silhouette.

Required action: Anchor hair at slot top; clip old costume to neutral silhouette and fit clothing to uncovered arms.

Evidence: ../../variants/Defender.mesh-female/review/native-face-check.jpg

Resolution: Head silhouette mask and top hair anchor; body clipped to neutral skin silhouette; sleeves extended over uncovered forearms; female chin moved, feathered, and masked behind all mouth options.

Final evidence: ../../variants/Defender.mesh-female/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Defender.mesh-female - chin-1-square.png

Status: closed-native-and-50x100

Rectangular chin boundary visible on the left jaw.

Cause: Source surrounding-skin rectangle.

Required action: Feather the derived chin alpha boundary.

Evidence: ../../variants/Defender.mesh-female/review/native-face-check.jpg

Resolution: Head silhouette mask and top hair anchor; body clipped to neutral skin silhouette; sleeves extended over uncovered forearms; female chin moved, feathered, and masked behind all mouth options.

Final evidence: ../../variants/Defender.mesh-female/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Wizard.mesh - hair-1-swept.png

Status: closed-native-and-50x100

Side hair overlaps forehead and cheek.

Cause: Source overlaps neutral head or extends outside neutral body.

Required action: Mask hair behind head and clip build to neutral body.

Evidence: ../../variants/Wizard.mesh/review/native-face-check.jpg

Resolution: Hair masked behind neutral head and body sources clipped to neutral silhouette; all options reviewed at native scale and 50x100.

Final evidence: ../../variants/Wizard.mesh/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Wizard.mesh - hair-2-waves.png

Status: closed-native-and-50x100

Side hair overlaps forehead and cheek.

Cause: Source overlaps neutral head or extends outside neutral body.

Required action: Mask hair behind head and clip build to neutral body.

Evidence: ../../variants/Wizard.mesh/review/native-face-check.jpg

Resolution: Hair masked behind neutral head and body sources clipped to neutral silhouette; all options reviewed at native scale and 50x100.

Final evidence: ../../variants/Wizard.mesh/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Wizard.mesh - build-1-slim.png

Status: closed-native-and-50x100

Old red and blue garment visible outside robe.

Cause: Source overlaps neutral head or extends outside neutral body.

Required action: Mask hair behind head and clip build to neutral body.

Evidence: ../../variants/Wizard.mesh/review/native-face-check.jpg

Resolution: Hair masked behind neutral head and body sources clipped to neutral silhouette; all options reviewed at native scale and 50x100.

Final evidence: ../../variants/Wizard.mesh/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Wizard.mesh - build-2-sturdy.png

Status: closed-native-and-50x100

Old red and blue garment visible outside robe.

Cause: Source overlaps neutral head or extends outside neutral body.

Required action: Mask hair behind head and clip build to neutral body.

Evidence: ../../variants/Wizard.mesh/review/native-face-check.jpg

Resolution: Hair masked behind neutral head and body sources clipped to neutral silhouette; all options reviewed at native scale and 50x100.

Final evidence: ../../variants/Wizard.mesh/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Wizard.mesh-female - hair-1-swept.png

Status: closed-native-and-50x100

Side hair overlaps forehead and cheek.

Cause: Source overlaps neutral head or extends outside neutral body.

Required action: Mask hair behind head and clip build to neutral body.

Evidence: ../../variants/Wizard.mesh-female/review/native-face-check.jpg

Resolution: Hair masked behind neutral head and body sources clipped to neutral silhouette; all options reviewed at native scale and 50x100.

Final evidence: ../../variants/Wizard.mesh-female/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Wizard.mesh-female - hair-2-waves.png

Status: closed-native-and-50x100

Side hair overlaps forehead and cheek.

Cause: Source overlaps neutral head or extends outside neutral body.

Required action: Mask hair behind head and clip build to neutral body.

Evidence: ../../variants/Wizard.mesh-female/review/native-face-check.jpg

Resolution: Hair masked behind neutral head and body sources clipped to neutral silhouette; all options reviewed at native scale and 50x100.

Final evidence: ../../variants/Wizard.mesh-female/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Wizard.mesh-female - build-1-slim.png

Status: closed-native-and-50x100

Old red and blue garment visible outside robe.

Cause: Source overlaps neutral head or extends outside neutral body.

Required action: Mask hair behind head and clip build to neutral body.

Evidence: ../../variants/Wizard.mesh-female/review/native-face-check.jpg

Resolution: Hair masked behind neutral head and body sources clipped to neutral silhouette; all options reviewed at native scale and 50x100.

Final evidence: ../../variants/Wizard.mesh-female/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Wizard.mesh-female - build-2-sturdy.png

Status: closed-native-and-50x100

Old red and blue garment visible outside robe.

Cause: Source overlaps neutral head or extends outside neutral body.

Required action: Mask hair behind head and clip build to neutral body.

Evidence: ../../variants/Wizard.mesh-female/review/native-face-check.jpg

Resolution: Hair masked behind neutral head and body sources clipped to neutral silhouette; all options reviewed at native scale and 50x100.

Final evidence: ../../variants/Wizard.mesh-female/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Elf.mesh-male - hair-1-swept.png

Status: closed-native-and-50x100

Hair crown starts below neutral head crown.

Cause: Centred hair registration and low chin slot; unmasked body silhouette.

Required action: Anchor crown, reposition chin with mouth occlusion, clip body.

Evidence: ../../variants/Elf.mesh-male/review/native-face-check.jpg

Resolution: Hair crown anchored at slot top; female fringe masked outside face; chin repositioned and mouth mask preserved; body clipped to neutral silhouette. Native and 50x100 checks completed.

Final evidence: ../../variants/Elf.mesh-male/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Elf.mesh-male - hair-2-waves.png

Status: closed-native-and-50x100

Hair crown starts below neutral head crown.

Cause: Centred hair registration and low chin slot; unmasked body silhouette.

Required action: Anchor crown, reposition chin with mouth occlusion, clip body.

Evidence: ../../variants/Elf.mesh-male/review/native-face-check.jpg

Resolution: Hair crown anchored at slot top; female fringe masked outside face; chin repositioned and mouth mask preserved; body clipped to neutral silhouette. Native and 50x100 checks completed.

Final evidence: ../../variants/Elf.mesh-male/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Elf.mesh-male - chin-1-square.png

Status: closed-native-and-50x100

Chin patch floats below mouth across clothing.

Cause: Centred hair registration and low chin slot; unmasked body silhouette.

Required action: Anchor crown, reposition chin with mouth occlusion, clip body.

Evidence: ../../variants/Elf.mesh-male/review/native-face-check.jpg

Resolution: Hair crown anchored at slot top; female fringe masked outside face; chin repositioned and mouth mask preserved; body clipped to neutral silhouette. Native and 50x100 checks completed.

Final evidence: ../../variants/Elf.mesh-male/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Elf.mesh-male - chin-2-rounded.png

Status: closed-native-and-50x100

Chin patch floats below mouth across clothing.

Cause: Centred hair registration and low chin slot; unmasked body silhouette.

Required action: Anchor crown, reposition chin with mouth occlusion, clip body.

Evidence: ../../variants/Elf.mesh-male/review/native-face-check.jpg

Resolution: Hair crown anchored at slot top; female fringe masked outside face; chin repositioned and mouth mask preserved; body clipped to neutral silhouette. Native and 50x100 checks completed.

Final evidence: ../../variants/Elf.mesh-male/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Elf.mesh-male - build-1-slim.png

Status: closed-native-and-50x100

Old body silhouette visible inside arm gap.

Cause: Centred hair registration and low chin slot; unmasked body silhouette.

Required action: Anchor crown, reposition chin with mouth occlusion, clip body.

Evidence: ../../variants/Elf.mesh-male/review/native-face-check.jpg

Resolution: Hair crown anchored at slot top; female fringe masked outside face; chin repositioned and mouth mask preserved; body clipped to neutral silhouette. Native and 50x100 checks completed.

Final evidence: ../../variants/Elf.mesh-male/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Elf.mesh-male - build-2-sturdy.png

Status: closed-native-and-50x100

Old body silhouette visible inside arm gap.

Cause: Centred hair registration and low chin slot; unmasked body silhouette.

Required action: Anchor crown, reposition chin with mouth occlusion, clip body.

Evidence: ../../variants/Elf.mesh-male/review/native-face-check.jpg

Resolution: Hair crown anchored at slot top; female fringe masked outside face; chin repositioned and mouth mask preserved; body clipped to neutral silhouette. Native and 50x100 checks completed.

Final evidence: ../../variants/Elf.mesh-male/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Elf.mesh - hair-1-swept.png

Status: closed-native-and-50x100

Hair crown starts below neutral head crown.

Cause: Centred hair registration and low chin slot; unmasked body silhouette.

Required action: Anchor crown, reposition chin with mouth occlusion, clip body.

Evidence: ../../variants/Elf.mesh/review/native-face-check.jpg

Resolution: Hair crown anchored at slot top; female fringe masked outside face; chin repositioned and mouth mask preserved; body clipped to neutral silhouette. Native and 50x100 checks completed.

Final evidence: ../../variants/Elf.mesh/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Elf.mesh - hair-2-waves.png

Status: closed-native-and-50x100

Hair crown starts below neutral head crown.

Cause: Centred hair registration and low chin slot; unmasked body silhouette.

Required action: Anchor crown, reposition chin with mouth occlusion, clip body.

Evidence: ../../variants/Elf.mesh/review/native-face-check.jpg

Resolution: Hair crown anchored at slot top; female fringe masked outside face; chin repositioned and mouth mask preserved; body clipped to neutral silhouette. Native and 50x100 checks completed.

Final evidence: ../../variants/Elf.mesh/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Elf.mesh - chin-1-square.png

Status: closed-native-and-50x100

Chin patch floats below mouth across clothing.

Cause: Centred hair registration and low chin slot; unmasked body silhouette.

Required action: Anchor crown, reposition chin with mouth occlusion, clip body.

Evidence: ../../variants/Elf.mesh/review/native-face-check.jpg

Resolution: Hair crown anchored at slot top; female fringe masked outside face; chin repositioned and mouth mask preserved; body clipped to neutral silhouette. Native and 50x100 checks completed.

Final evidence: ../../variants/Elf.mesh/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Elf.mesh - chin-2-rounded.png

Status: closed-native-and-50x100

Chin patch floats below mouth across clothing.

Cause: Centred hair registration and low chin slot; unmasked body silhouette.

Required action: Anchor crown, reposition chin with mouth occlusion, clip body.

Evidence: ../../variants/Elf.mesh/review/native-face-check.jpg

Resolution: Hair crown anchored at slot top; female fringe masked outside face; chin repositioned and mouth mask preserved; body clipped to neutral silhouette. Native and 50x100 checks completed.

Final evidence: ../../variants/Elf.mesh/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Elf.mesh - build-1-slim.png

Status: closed-native-and-50x100

Old body silhouette visible inside arm gap.

Cause: Centred hair registration and low chin slot; unmasked body silhouette.

Required action: Anchor crown, reposition chin with mouth occlusion, clip body.

Evidence: ../../variants/Elf.mesh/review/native-face-check.jpg

Resolution: Hair crown anchored at slot top; female fringe masked outside face; chin repositioned and mouth mask preserved; body clipped to neutral silhouette. Native and 50x100 checks completed.

Final evidence: ../../variants/Elf.mesh/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Elf.mesh - build-2-sturdy.png

Status: closed-native-and-50x100

Old body silhouette visible inside arm gap.

Cause: Centred hair registration and low chin slot; unmasked body silhouette.

Required action: Anchor crown, reposition chin with mouth occlusion, clip body.

Evidence: ../../variants/Elf.mesh/review/native-face-check.jpg

Resolution: Hair crown anchored at slot top; female fringe masked outside face; chin repositioned and mouth mask preserved; body clipped to neutral silhouette. Native and 50x100 checks completed.

Final evidence: ../../variants/Elf.mesh/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## DarkElf.mesh-male - hair-1-swept.png

Status: closed-native-and-50x100

Crown below head; strands cross facial features.

Cause: Low centred registration and narrow sleeve coverage.

Required action: Anchor crown, mask face, reposition chin and extend existing sleeve texture.

Evidence: ../../variants/DarkElf.mesh-male/review/native-face-check.jpg

Resolution: Crown anchored, face-contour alpha mask applied, chin repositioned with mouth occlusion and continuous sleeve extensions fitted; body clipped to neutral blue skin silhouette. Native and 50x100 checks completed.

Final evidence: ../../variants/DarkElf.mesh-male/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## DarkElf.mesh-male - hair-2-waves.png

Status: closed-native-and-50x100

Crown below head; strands cross facial features.

Cause: Low centred registration and narrow sleeve coverage.

Required action: Anchor crown, mask face, reposition chin and extend existing sleeve texture.

Evidence: ../../variants/DarkElf.mesh-male/review/native-face-check.jpg

Resolution: Crown anchored, face-contour alpha mask applied, chin repositioned with mouth occlusion and continuous sleeve extensions fitted; body clipped to neutral blue skin silhouette. Native and 50x100 checks completed.

Final evidence: ../../variants/DarkElf.mesh-male/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## DarkElf.mesh-male - chin-1-square.png

Status: closed-native-and-50x100

Chin extends over garment.

Cause: Low centred registration and narrow sleeve coverage.

Required action: Anchor crown, mask face, reposition chin and extend existing sleeve texture.

Evidence: ../../variants/DarkElf.mesh-male/review/native-face-check.jpg

Resolution: Crown anchored, face-contour alpha mask applied, chin repositioned with mouth occlusion and continuous sleeve extensions fitted; body clipped to neutral blue skin silhouette. Native and 50x100 checks completed.

Final evidence: ../../variants/DarkElf.mesh-male/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## DarkElf.mesh-male - chin-2-rounded.png

Status: closed-native-and-50x100

Chin extends over garment.

Cause: Low centred registration and narrow sleeve coverage.

Required action: Anchor crown, mask face, reposition chin and extend existing sleeve texture.

Evidence: ../../variants/DarkElf.mesh-male/review/native-face-check.jpg

Resolution: Crown anchored, face-contour alpha mask applied, chin repositioned with mouth occlusion and continuous sleeve extensions fitted; body clipped to neutral blue skin silhouette. Native and 50x100 checks completed.

Final evidence: ../../variants/DarkElf.mesh-male/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## DarkElf.mesh-male - outfit-1-tunic-build0.png

Status: closed-native-and-50x100

Blue forearm contours visible inside sleeve gaps.

Cause: Low centred registration and narrow sleeve coverage.

Required action: Anchor crown, mask face, reposition chin and extend existing sleeve texture.

Evidence: ../../variants/DarkElf.mesh-male/review/native-face-check.jpg

Resolution: Crown anchored, face-contour alpha mask applied, chin repositioned with mouth occlusion and continuous sleeve extensions fitted; body clipped to neutral blue skin silhouette. Native and 50x100 checks completed.

Final evidence: ../../variants/DarkElf.mesh-male/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## DarkElf.mesh - hair-1-swept.png

Status: closed-native-and-50x100

Crown below head; strands cross facial features.

Cause: Low centred registration and narrow sleeve coverage.

Required action: Anchor crown, mask face, reposition chin and extend existing sleeve texture.

Evidence: ../../variants/DarkElf.mesh/review/native-face-check.jpg

Resolution: Crown anchored, face-contour alpha mask applied, chin repositioned with mouth occlusion and continuous sleeve extensions fitted; body clipped to neutral blue skin silhouette. Native and 50x100 checks completed.

Final evidence: ../../variants/DarkElf.mesh/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## DarkElf.mesh - hair-2-waves.png

Status: closed-native-and-50x100

Crown below head; strands cross facial features.

Cause: Low centred registration and narrow sleeve coverage.

Required action: Anchor crown, mask face, reposition chin and extend existing sleeve texture.

Evidence: ../../variants/DarkElf.mesh/review/native-face-check.jpg

Resolution: Crown anchored, face-contour alpha mask applied, chin repositioned with mouth occlusion and continuous sleeve extensions fitted; body clipped to neutral blue skin silhouette. Native and 50x100 checks completed.

Final evidence: ../../variants/DarkElf.mesh/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## DarkElf.mesh - chin-1-square.png

Status: closed-native-and-50x100

Chin extends over garment.

Cause: Low centred registration and narrow sleeve coverage.

Required action: Anchor crown, mask face, reposition chin and extend existing sleeve texture.

Evidence: ../../variants/DarkElf.mesh/review/native-face-check.jpg

Resolution: Crown anchored, face-contour alpha mask applied, chin repositioned with mouth occlusion and continuous sleeve extensions fitted; body clipped to neutral blue skin silhouette. Native and 50x100 checks completed.

Final evidence: ../../variants/DarkElf.mesh/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## DarkElf.mesh - chin-2-rounded.png

Status: closed-native-and-50x100

Chin extends over garment.

Cause: Low centred registration and narrow sleeve coverage.

Required action: Anchor crown, mask face, reposition chin and extend existing sleeve texture.

Evidence: ../../variants/DarkElf.mesh/review/native-face-check.jpg

Resolution: Crown anchored, face-contour alpha mask applied, chin repositioned with mouth occlusion and continuous sleeve extensions fitted; body clipped to neutral blue skin silhouette. Native and 50x100 checks completed.

Final evidence: ../../variants/DarkElf.mesh/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## DarkElf.mesh - outfit-1-tunic-build0.png

Status: closed-native-and-50x100

Blue forearm contours visible inside sleeve gaps.

Cause: Low centred registration and narrow sleeve coverage.

Required action: Anchor crown, mask face, reposition chin and extend existing sleeve texture.

Evidence: ../../variants/DarkElf.mesh/review/native-face-check.jpg

Resolution: Crown anchored, face-contour alpha mask applied, chin repositioned with mouth occlusion and continuous sleeve extensions fitted; body clipped to neutral blue skin silhouette. Native and 50x100 checks completed.

Final evidence: ../../variants/DarkElf.mesh/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Troll.mesh - outfit-1-leather-build0.png

Status: closed-native-and-50x100

Raised collar covers right cheek.

Cause: Head colour mask misses stone shading; centred hair placement.

Required action: Use measured head polygon and anchor/mask existing hair.

Evidence: ../../variants/Troll.mesh/review/native-face-check.jpg

Resolution: Collars masked behind measured stone-head silhouettes; female hair crown anchored and face strands masked. Native and 50x100 checks completed.

Final evidence: ../../variants/Troll.mesh/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Troll.mesh-female - hair-1-swept.png

Status: closed-native-and-50x100

Crown sits below head top and strands cross face.

Cause: Head colour mask misses stone shading; centred hair placement.

Required action: Use measured head polygon and anchor/mask existing hair.

Evidence: ../../variants/Troll.mesh-female/review/native-face-check.jpg

Resolution: Collars masked behind measured stone-head silhouettes; female hair crown anchored and face strands masked. Native and 50x100 checks completed.

Final evidence: ../../variants/Troll.mesh-female/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Troll.mesh-female - hair-2-waves.png

Status: closed-native-and-50x100

Strands cross facial features.

Cause: Head colour mask misses stone shading; centred hair placement.

Required action: Use measured head polygon and anchor/mask existing hair.

Evidence: ../../variants/Troll.mesh-female/review/native-face-check.jpg

Resolution: Collars masked behind measured stone-head silhouettes; female hair crown anchored and face strands masked. Native and 50x100 checks completed.

Final evidence: ../../variants/Troll.mesh-female/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Troll.mesh-female - outfit-1-leather-build0.png

Status: closed-native-and-50x100

Collar overlaps right cheek after hair registration reveals it.

Cause: Stone colour threshold misses dark head shading.

Required action: Mask collar behind measured head silhouette.

Evidence: ../../variants/Troll.mesh-female/review/native-face-check.jpg

Resolution: Collars masked behind measured stone-head silhouettes; female hair crown anchored and face strands masked. Native and 50x100 checks completed.

Final evidence: ../../variants/Troll.mesh-female/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Lizardman.mesh-female - hair-1-swept.png

Status: closed-native-and-50x100

Crest starts below head crown.

Cause: Centred crest and low jaw registration; sleeve contour too narrow.

Required action: Anchor crest, reposition jaw and extend existing sleeve alpha/texture.

Evidence: ../../variants/Lizardman.mesh-female/review/native-face-check.jpg

Resolution: Existing crest anchored at crown; female jaw moved up and masked against mouth; preserved sleeve texture translated into uncovered body gaps. Native face/body panels and twelve 50x100 combinations inspected; no source regenerated.

Final evidence: ../../variants/Lizardman.mesh-female/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Lizardman.mesh-female - hair-2-waves.png

Status: closed-native-and-50x100

Crest starts below head crown.

Cause: Centred crest and low jaw registration; sleeve contour too narrow.

Required action: Anchor crest, reposition jaw and extend existing sleeve alpha/texture.

Evidence: ../../variants/Lizardman.mesh-female/review/native-face-check.jpg

Resolution: Existing crest anchored at crown; female jaw moved up and masked against mouth; preserved sleeve texture translated into uncovered body gaps. Native face/body panels and twelve 50x100 combinations inspected; no source regenerated.

Final evidence: ../../variants/Lizardman.mesh-female/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Lizardman.mesh-female - chin-1-square.png

Status: closed-native-and-50x100

Jaw patch is separated from lower lip by clothing.

Cause: Centred crest and low jaw registration; sleeve contour too narrow.

Required action: Anchor crest, reposition jaw and extend existing sleeve alpha/texture.

Evidence: ../../variants/Lizardman.mesh-female/review/native-face-check.jpg

Resolution: Existing crest anchored at crown; female jaw moved up and masked against mouth; preserved sleeve texture translated into uncovered body gaps. Native face/body panels and twelve 50x100 combinations inspected; no source regenerated.

Final evidence: ../../variants/Lizardman.mesh-female/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Lizardman.mesh-female - chin-2-rounded.png

Status: closed-native-and-50x100

Jaw patch is separated from lower lip by clothing.

Cause: Centred crest and low jaw registration; sleeve contour too narrow.

Required action: Anchor crest, reposition jaw and extend existing sleeve alpha/texture.

Evidence: ../../variants/Lizardman.mesh-female/review/native-face-check.jpg

Resolution: Existing crest anchored at crown; female jaw moved up and masked against mouth; preserved sleeve texture translated into uncovered body gaps. Native face/body panels and twelve 50x100 combinations inspected; no source regenerated.

Final evidence: ../../variants/Lizardman.mesh-female/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Lizardman.mesh-female - outfit-1-leather-build0.png

Status: closed-native-and-50x100

Uncovered yellow body sliver inside left lower sleeve.

Cause: Centred crest and low jaw registration; sleeve contour too narrow.

Required action: Anchor crest, reposition jaw and extend existing sleeve alpha/texture.

Evidence: ../../variants/Lizardman.mesh-female/review/native-face-check.jpg

Resolution: Existing crest anchored at crown; female jaw moved up and masked against mouth; preserved sleeve texture translated into uncovered body gaps. Native face/body panels and twelve 50x100 combinations inspected; no source regenerated.

Final evidence: ../../variants/Lizardman.mesh-female/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Lizardman.mesh - outfit-1-leather-build0.png

Status: closed-native-and-50x100

Yellow body sliver remains inside lower sleeve gap.

Cause: Existing sleeve gap wider than first derived extension.

Required action: Extend preserved garment texture over measured uncovered body.

Evidence: ../../variants/Lizardman.mesh/review/native-body-check.jpg

Resolution: Existing crest anchored at crown; female jaw moved up and masked against mouth; preserved sleeve texture translated into uncovered body gaps. Native face/body panels and twelve 50x100 combinations inspected; no source regenerated.

Final evidence: ../../variants/Lizardman.mesh/review/native-face-check.jpg; native-body-check.jpg; combined-small-sheet.png

## Option geometry review

All 34 catalogs were inspected at original size and 50x100. Every one of the 688 registered options occurs in the review combinations and retains visible pixels. Coverage does not imply exhaustive Cartesian acceptance. Existing source images, base images, helmets and saved previews remain unchanged. Prior findings and provenance above are retained.

| Catalog | File | Slot / option | Native combinations | Visible pixels | Status | Observation |
| --- | --- | --- | --- | --- | --- | --- |
| Kobold.mesh | build-1-slim.png | build / 1 | 4,5,6 | 250814 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh | build-2-sturdy.png | build / 2 | 7,8,9 | 278939 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh | ears-1-notched.png | ears / 1 | 1,3,5,7,9,11 | 19284 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh | ears-2-rounder.png | ears / 2 | 2,4,6,8,10,12 | 24437 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh | eyes-1-narrow.png | eyes / 1 | 1,4,7,10 | 25480 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh | eyes-2-round.png | eyes / 2 | 2,5,8,11 | 21612 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh | eyes-3-eyepatch.png | eyes / 3 | 3,6,9,12 | 24084 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh | nose-1-crooked.png | nose / 1 | 1,3,5,7,9,11 | 7712 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh | nose-2-broad.png | nose / 2 | 2,4,6,8,10,12 | 7989 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh | mouth-1-teeth.png | mouth / 1 | 1,4,7,10 | 10438 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh | mouth-2-grin.png | mouth / 2 | 2,5,8,11 | 10140 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh | mouth-3-cigar.png | mouth / 3 | 3,6,9,12 | 11861 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh | chin-1-forked.png | chin / 1 | 1,3,5,7,9,11 | 10750 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh | chin-2-braided.png | chin / 2 | 2,4,6,8,10,12 | 14219 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 2098 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 4316 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 5756 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11 | 14419 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12 | 18543 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh | outfit-1-patched-build0.png | outfit / 1 | 1,2,3,4,5,6,7,8,9,10,11,12 | 806933 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh-female | build-1-slim.png | build / 1 | 4,5,6 | 192789 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh-female | build-2-sturdy.png | build / 2 | 7,8,9 | 174908 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh-female | hair-1-swept.png | hair / 1 | 1,3,5,7,9,11 | 220582 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh-female | hair-2-waves.png | hair / 2 | 2,4,6,8,10,12 | 155466 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh-female | ears-1-notched.png | ears / 1 | 1,3,5,7,9,11 | 20522 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh-female | ears-2-rounder.png | ears / 2 | 2,4,6,8,10,12 | 19501 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh-female | eyes-1-narrow.png | eyes / 1 | 1,4,7,10 | 20371 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh-female | eyes-2-round.png | eyes / 2 | 2,5,8,11 | 18751 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh-female | eyes-3-eyepatch.png | eyes / 3 | 3,6,9,12 | 26060 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh-female | nose-1-crooked.png | nose / 1 | 1,3,5,7,9,11 | 9072 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh-female | nose-2-broad.png | nose / 2 | 2,4,6,8,10,12 | 7538 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh-female | mouth-1-teeth.png | mouth / 1 | 1,4,7,10 | 9820 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh-female | mouth-2-grin.png | mouth / 2 | 2,5,8,11 | 8584 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh-female | mouth-3-cigar.png | mouth / 3 | 3,6,9,12 | 7182 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh-female | chin-1-square.png | chin / 1 | 1,3,5,7,9,11 | 6667 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh-female | chin-2-rounded.png | chin / 2 | 2,4,6,8,10,12 | 6034 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh-female | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 1605 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh-female | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 2957 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh-female | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 6369 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh-female | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11 | 12758 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh-female | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12 | 18726 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Kobold.mesh-female | outfit-1-patched-build0.png | outfit / 1 | 1,2,3,4,5,6,7,8,9,10,11,12 | 824814 | inspected-native-and-50x100 | Ear and nose options use measured destinations; single ear in three-quarter view accepted. |
| Goblin.mesh | build-1-slim.png | build / 1 | 4,5,6 | 539502 | inspected-native-and-50x100 | Ear and nose options use measured destinations without reflection; selected race and gender catalog verified. |
| Goblin.mesh | build-2-sturdy.png | build / 2 | 7,8,9 | 552163 | inspected-native-and-50x100 | Ear and nose options use measured destinations without reflection; selected race and gender catalog verified. |
| Goblin.mesh | ears-1-notched.png | ears / 1 | 1,3,5,7,9,11 | 10096 | inspected-native-and-50x100 | Ear and nose options use measured destinations without reflection; selected race and gender catalog verified. |
| Goblin.mesh | ears-2-rounder.png | ears / 2 | 2,4,6,8,10,12 | 13820 | inspected-native-and-50x100 | Ear and nose options use measured destinations without reflection; selected race and gender catalog verified. |
| Goblin.mesh | eyes-1-narrow.png | eyes / 1 | 1,4,7,10 | 30271 | inspected-native-and-50x100 | Ear and nose options use measured destinations without reflection; selected race and gender catalog verified. |
| Goblin.mesh | eyes-2-round.png | eyes / 2 | 2,5,8,11 | 25606 | inspected-native-and-50x100 | Ear and nose options use measured destinations without reflection; selected race and gender catalog verified. |
| Goblin.mesh | eyes-3-eyepatch.png | eyes / 3 | 3,6,9,12 | 24398 | inspected-native-and-50x100 | Ear and nose options use measured destinations without reflection; selected race and gender catalog verified. |
| Goblin.mesh | nose-1-crooked.png | nose / 1 | 1,3,5,7,9,11 | 10146 | inspected-native-and-50x100 | Ear and nose options use measured destinations without reflection; selected race and gender catalog verified. |
| Goblin.mesh | nose-2-broad.png | nose / 2 | 2,4,6,8,10,12 | 8368 | inspected-native-and-50x100 | Ear and nose options use measured destinations without reflection; selected race and gender catalog verified. |
| Goblin.mesh | mouth-1-teeth.png | mouth / 1 | 1,4,7,10 | 11162 | inspected-native-and-50x100 | Ear and nose options use measured destinations without reflection; selected race and gender catalog verified. |
| Goblin.mesh | mouth-2-grin.png | mouth / 2 | 2,5,8,11 | 8535 | inspected-native-and-50x100 | Ear and nose options use measured destinations without reflection; selected race and gender catalog verified. |
| Goblin.mesh | mouth-3-cigar.png | mouth / 3 | 3,6,9,12 | 11551 | inspected-native-and-50x100 | Ear and nose options use measured destinations without reflection; selected race and gender catalog verified. |
| Goblin.mesh | chin-1-square.png | chin / 1 | 1,3,5,7,9,11 | 14850 | inspected-native-and-50x100 | Ear and nose options use measured destinations without reflection; selected race and gender catalog verified. |
| Goblin.mesh | chin-2-rounded.png | chin / 2 | 2,4,6,8,10,12 | 15410 | inspected-native-and-50x100 | Ear and nose options use measured destinations without reflection; selected race and gender catalog verified. |
| Goblin.mesh | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 1306 | inspected-native-and-50x100 | Ear and nose options use measured destinations without reflection; selected race and gender catalog verified. |
| Goblin.mesh | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 2822 | inspected-native-and-50x100 | Ear and nose options use measured destinations without reflection; selected race and gender catalog verified. |
| Goblin.mesh | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 3976 | inspected-native-and-50x100 | Ear and nose options use measured destinations without reflection; selected race and gender catalog verified. |
| Goblin.mesh | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11 | 10254 | inspected-native-and-50x100 | Ear and nose options use measured destinations without reflection; selected race and gender catalog verified. |
| Goblin.mesh | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12 | 21847 | inspected-native-and-50x100 | Ear and nose options use measured destinations without reflection; selected race and gender catalog verified. |
| Goblin.mesh | outfit-1-patched-build0.png | outfit / 1 | 1,2,3,4,5,6,7,8,9,10,11,12 | 820756 | inspected-native-and-50x100 | Ear and nose options use measured destinations without reflection; selected race and gender catalog verified. |
| Goblin.mesh-female | build-1-slim.png | build / 1 | 4,5,6 | 516255 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Goblin.mesh-female | build-2-sturdy.png | build / 2 | 7,8,9 | 547864 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Goblin.mesh-female | hair-1-swept.png | hair / 1 | 1,3,5,7,9,11 | 251819 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Goblin.mesh-female | hair-2-waves.png | hair / 2 | 2,4,6,8,10,12 | 226864 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Goblin.mesh-female | ears-1-notched.png | ears / 1 | 1,3,5,7,9,11 | 20466 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Goblin.mesh-female | ears-2-rounder.png | ears / 2 | 2,4,6,8,10,12 | 25126 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Goblin.mesh-female | eyes-1-narrow.png | eyes / 1 | 1,4,7,10 | 24415 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Goblin.mesh-female | eyes-2-round.png | eyes / 2 | 2,5,8,11 | 30070 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Goblin.mesh-female | eyes-3-eyepatch.png | eyes / 3 | 3,6,9,12 | 29504 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Goblin.mesh-female | nose-1-crooked.png | nose / 1 | 1,3,5,7,9,11 | 11654 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Goblin.mesh-female | nose-2-broad.png | nose / 2 | 2,4,6,8,10,12 | 10170 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Goblin.mesh-female | mouth-1-teeth.png | mouth / 1 | 1,4,7,10 | 9887 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Goblin.mesh-female | mouth-2-grin.png | mouth / 2 | 2,5,8,11 | 6699 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Goblin.mesh-female | mouth-3-cigar.png | mouth / 3 | 3,6,9,12 | 12987 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Goblin.mesh-female | chin-1-square.png | chin / 1 | 1,3,5,7,9,11 | 14550 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Goblin.mesh-female | chin-2-rounded.png | chin / 2 | 2,4,6,8,10,12 | 12368 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Goblin.mesh-female | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 1129 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Goblin.mesh-female | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 2978 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Goblin.mesh-female | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 2066 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Goblin.mesh-female | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11 | 14714 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Goblin.mesh-female | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12 | 25797 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Goblin.mesh-female | outfit-1-patched-build0.png | outfit / 1 | 1,2,3,4,5,6,7,8,9,10,11,12 | 828995 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh | build-1-slim.png | build / 1 | 4,5,6 | 613150 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh | build-2-sturdy.png | build / 2 | 7,8,9 | 609501 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh | ears-1-notched.png | ears / 1 | 1,3,5,7,9,11 | 24342 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh | ears-2-rounder.png | ears / 2 | 2,4,6,8,10,12 | 31227 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh | eyes-1-narrow.png | eyes / 1 | 1,4,7,10 | 31148 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh | eyes-2-round.png | eyes / 2 | 2,5,8,11 | 35282 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh | eyes-3-eyepatch.png | eyes / 3 | 3,6,9,12 | 35745 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh | nose-1-crooked.png | nose / 1 | 1,3,5,7,9,11 | 18533 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh | nose-2-broad.png | nose / 2 | 2,4,6,8,10,12 | 17448 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh | mouth-1-teeth.png | mouth / 1 | 1,4,7,10 | 17322 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh | mouth-2-grin.png | mouth / 2 | 2,5,8,11 | 18491 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh | mouth-3-cigar.png | mouth / 3 | 3,6,9,12 | 16942 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh | chin-1-square.png | chin / 1 | 1,3,5,7,9,11 | 10204 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh | chin-2-rounded.png | chin / 2 | 2,4,6,8,10,12 | 11615 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 913 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 2435 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 3209 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11 | 11000 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12 | 19617 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh | outfit-1-leather-build0.png | outfit / 1 | 1,4,7,10 | 857407 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh | outfit-2-chain-build0.png | outfit / 2 | 2,5,8,11 | 903056 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh | outfit-3-padded-build0.png | outfit / 3 | 3,6,9,12 | 852642 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh-female | build-1-slim.png | build / 1 | 4,5,6 | 577366 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh-female | build-2-sturdy.png | build / 2 | 7,8,9 | 607143 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh-female | hair-1-swept.png | hair / 1 | 1,3,5,7,9,11 | 222291 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh-female | hair-2-waves.png | hair / 2 | 2,4,6,8,10,12 | 236512 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh-female | ears-1-notched.png | ears / 1 | 1,3,5,7,9,11 | 29421 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh-female | ears-2-rounder.png | ears / 2 | 2,4,6,8,10,12 | 34447 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh-female | eyes-1-narrow.png | eyes / 1 | 1,4,7,10 | 27122 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh-female | eyes-2-round.png | eyes / 2 | 2,5,8,11 | 17444 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh-female | eyes-3-eyepatch.png | eyes / 3 | 3,6,9,12 | 27954 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh-female | nose-1-crooked.png | nose / 1 | 1,3,5,7,9,11 | 15539 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh-female | nose-2-broad.png | nose / 2 | 2,4,6,8,10,12 | 13503 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh-female | mouth-1-tusks.png | mouth / 1 | 1,4,7,10 | 27059 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh-female | mouth-2-grin.png | mouth / 2 | 2,5,8,11 | 20315 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh-female | mouth-3-cigar.png | mouth / 3 | 3,6,9,12 | 22035 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh-female | chin-1-square.png | chin / 1 | 1,3,5,7,9,11 | 16502 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh-female | chin-2-rounded.png | chin / 2 | 2,4,6,8,10,12 | 17116 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh-female | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 994 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh-female | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 2940 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh-female | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 5399 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh-female | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11 | 10709 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh-female | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12 | 24763 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh-female | outfit-1-leather-build0.png | outfit / 1 | 1,4,7,10 | 894640 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh-female | outfit-2-chain-build0.png | outfit / 2 | 2,5,8,11 | 890544 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Orc.mesh-female | outfit-3-padded-build0.png | outfit / 3 | 3,6,9,12 | 917066 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Dwarf1.mesh | build-1-slim.png | build / 1 | 4,5,6 | 472951 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Dwarf1.mesh | build-2-sturdy.png | build / 2 | 7,8,9 | 534936 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Dwarf1.mesh | hair-1-braid.png | hair / 1 | 1,3,5,7,9,11 | 22831 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Dwarf1.mesh | hair-2-loose.png | hair / 2 | 2,4,6,8,10,12 | 55300 | inspected-native-and-50x100 | Only hair option 2 translated to the existing outer temple below the headband; abrupt root on bare upper forehead removed without covering eyes, nose or mouth. |
| Dwarf1.mesh | eyes-1-narrow.png | eyes / 1 | 1,4,7,10 | 52725 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Dwarf1.mesh | eyes-2-round.png | eyes / 2 | 2,5,8,11 | 59251 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Dwarf1.mesh | eyes-3-eyepatch.png | eyes / 3 | 3,6,9,12 | 55003 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Dwarf1.mesh | nose-1-crooked.png | nose / 1 | 1,3,5,7,9,11 | 15519 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Dwarf1.mesh | nose-2-broad.png | nose / 2 | 2,4,6,8,10,12 | 16704 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Dwarf1.mesh | mouth-1-scowl.png | mouth / 1 | 1,4,7,10 | 22375 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Dwarf1.mesh | mouth-2-grin.png | mouth / 2 | 2,5,8,11 | 18660 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Dwarf1.mesh | mouth-3-cigar.png | mouth / 3 | 3,6,9,12 | 20268 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Dwarf1.mesh | chin-1-forked.png | chin / 1 | 1,3,5,7,9,11 | 108749 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Dwarf1.mesh | chin-2-braided.png | chin / 2 | 2,4,6,8,10,12 | 117416 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Dwarf1.mesh | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 2441 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Dwarf1.mesh | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 4915 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Dwarf1.mesh | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 8023 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Dwarf1.mesh | neck-1-pin.png | neck / 1 | 1,3,5,7,9,11 | 19924 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Dwarf1.mesh | neck-2-tooth.png | neck / 2 | 2,4,6,8,10,12 | 25915 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Dwarf1.mesh | outfit-1-apron-build0.png | outfit / 1 | 1,2,3,4,5,6,7,8,9,10,11,12 | 855949 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Dwarf1.mesh-female | build-1-slim.png | build / 1 | 4,5,6 | 548322 | inspected-native-and-50x100 | Hair option 2 destination moves only the braid away from the nose; option 1 and source pixels unchanged. |
| Dwarf1.mesh-female | build-2-sturdy.png | build / 2 | 7,8,9 | 550624 | inspected-native-and-50x100 | Hair option 2 destination moves only the braid away from the nose; option 1 and source pixels unchanged. |
| Dwarf1.mesh-female | hair-1-swept.png | hair / 1 | 1,3,5,7,9,11 | 254365 | inspected-native-and-50x100 | Hair option 2 destination moves only the braid away from the nose; option 1 and source pixels unchanged. |
| Dwarf1.mesh-female | hair-2-waves.png | hair / 2 | 2,4,6,8,10,12 | 430185 | inspected-native-and-50x100 | Hair option 2 destination moves only the braid away from the nose; option 1 and source pixels unchanged. |
| Dwarf1.mesh-female | eyes-1-narrow.png | eyes / 1 | 1,4,7,10 | 34784 | inspected-native-and-50x100 | Hair option 2 destination moves only the braid away from the nose; option 1 and source pixels unchanged. |
| Dwarf1.mesh-female | eyes-2-round.png | eyes / 2 | 2,5,8,11 | 34937 | inspected-native-and-50x100 | Hair option 2 destination moves only the braid away from the nose; option 1 and source pixels unchanged. |
| Dwarf1.mesh-female | eyes-3-eyepatch.png | eyes / 3 | 3,6,9,12 | 38707 | inspected-native-and-50x100 | Hair option 2 destination moves only the braid away from the nose; option 1 and source pixels unchanged. |
| Dwarf1.mesh-female | nose-1-crooked.png | nose / 1 | 1,3,5,7,9,11 | 12481 | inspected-native-and-50x100 | Hair option 2 destination moves only the braid away from the nose; option 1 and source pixels unchanged. |
| Dwarf1.mesh-female | nose-2-broad.png | nose / 2 | 2,4,6,8,10,12 | 17890 | inspected-native-and-50x100 | Hair option 2 destination moves only the braid away from the nose; option 1 and source pixels unchanged. |
| Dwarf1.mesh-female | mouth-1-teeth.png | mouth / 1 | 1,4,7,10 | 27373 | inspected-native-and-50x100 | Hair option 2 destination moves only the braid away from the nose; option 1 and source pixels unchanged. |
| Dwarf1.mesh-female | mouth-2-grin.png | mouth / 2 | 2,5,8,11 | 23351 | inspected-native-and-50x100 | Hair option 2 destination moves only the braid away from the nose; option 1 and source pixels unchanged. |
| Dwarf1.mesh-female | mouth-3-cigar.png | mouth / 3 | 3,6,9,12 | 20984 | inspected-native-and-50x100 | Hair option 2 destination moves only the braid away from the nose; option 1 and source pixels unchanged. |
| Dwarf1.mesh-female | chin-1-square.png | chin / 1 | 1,3,5,7,9,11 | 22204 | inspected-native-and-50x100 | Hair option 2 destination moves only the braid away from the nose; option 1 and source pixels unchanged. |
| Dwarf1.mesh-female | chin-2-rounded.png | chin / 2 | 2,4,6,8,10,12 | 19208 | inspected-native-and-50x100 | Hair option 2 destination moves only the braid away from the nose; option 1 and source pixels unchanged. |
| Dwarf1.mesh-female | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 1621 | inspected-native-and-50x100 | Hair option 2 destination moves only the braid away from the nose; option 1 and source pixels unchanged. |
| Dwarf1.mesh-female | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 2789 | inspected-native-and-50x100 | Hair option 2 destination moves only the braid away from the nose; option 1 and source pixels unchanged. |
| Dwarf1.mesh-female | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 5504 | inspected-native-and-50x100 | Hair option 2 destination moves only the braid away from the nose; option 1 and source pixels unchanged. |
| Dwarf1.mesh-female | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11 | 14151 | inspected-native-and-50x100 | Hair option 2 destination moves only the braid away from the nose; option 1 and source pixels unchanged. |
| Dwarf1.mesh-female | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12 | 15041 | inspected-native-and-50x100 | Hair option 2 destination moves only the braid away from the nose; option 1 and source pixels unchanged. |
| Dwarf1.mesh-female | outfit-1-apron-build0.png | outfit / 1 | 1,2,3,4,5,6,7,8,9,10,11,12 | 866934 | inspected-native-and-50x100 | Hair option 2 destination moves only the braid away from the nose; option 1 and source pixels unchanged. |
| Dwarf2.mesh | build-1-slim.png | build / 1 | 4,5,6 | 562183 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh | build-2-sturdy.png | build / 2 | 7,8,9 | 569665 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh | hair-1-swept.png | hair / 1 | 1,3,5,7,9,11 | 28880 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh | hair-2-waves.png | hair / 2 | 2,4,6,8,10,12 | 21979 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh | eyes-1-narrow.png | eyes / 1 | 1,4,7,10 | 29915 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh | eyes-2-round.png | eyes / 2 | 2,5,8,11 | 38282 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh | eyes-3-eyepatch.png | eyes / 3 | 3,6,9,12 | 34181 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh | nose-1-crooked.png | nose / 1 | 1,3,5,7,9,11 | 16425 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh | nose-2-broad.png | nose / 2 | 2,4,6,8,10,12 | 16794 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh | mouth-1-teeth.png | mouth / 1 | 1,4,7,10 | 24960 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh | mouth-2-grin.png | mouth / 2 | 2,5,8,11 | 26185 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh | mouth-3-cigar.png | mouth / 3 | 3,6,9,12 | 28142 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh | chin-1-forked.png | chin / 1 | 1,3,5,7,9,11 | 99607 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh | chin-2-braided.png | chin / 2 | 2,4,6,8,10,12 | 92973 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 1320 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 2813 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 5560 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11 | 16613 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12 | 40919 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh | outfit-1-padded-build0.png | outfit / 1 | 1,2,3,4,5,6,7,8,9,10,11,12 | 827397 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh-female | build-1-slim.png | build / 1 | 4,5,6 | 556456 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh-female | build-2-sturdy.png | build / 2 | 7,8,9 | 587612 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh-female | hair-1-swept.png | hair / 1 | 1,3,5,7,9,11 | 307288 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh-female | hair-2-waves.png | hair / 2 | 2,4,6,8,10,12 | 257804 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh-female | eyes-1-narrow.png | eyes / 1 | 1,4,7,10 | 29651 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh-female | eyes-2-round.png | eyes / 2 | 2,5,8,11 | 40033 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh-female | eyes-3-eyepatch.png | eyes / 3 | 3,6,9,12 | 40857 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh-female | nose-1-crooked.png | nose / 1 | 1,3,5,7,9,11 | 12652 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh-female | nose-2-broad.png | nose / 2 | 2,4,6,8,10,12 | 16985 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh-female | mouth-1-teeth.png | mouth / 1 | 1,4,7,10 | 23274 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh-female | mouth-2-grin.png | mouth / 2 | 2,5,8,11 | 25175 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh-female | mouth-3-cigar.png | mouth / 3 | 3,6,9,12 | 20927 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh-female | chin-1-square.png | chin / 1 | 1,3,5,7,9,11 | 18324 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh-female | chin-2-rounded.png | chin / 2 | 2,4,6,8,10,12 | 19075 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh-female | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 1251 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh-female | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 2907 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh-female | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 3500 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh-female | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11 | 14980 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh-female | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12 | 26879 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| Dwarf2.mesh-female | outfit-1-padded-build0.png | outfit / 1 | 1,2,3,4,5,6,7,8,9,10,11,12 | 806946 | inspected-native-and-50x100 | Both chin options protect the actually selected mouth alpha; all three mouth options checked. |
| RunelordDwarf.mesh | build-1-slim.png | build / 1 | 4,5,6 | 551910 | inspected-native-and-50x100 | Beard partly occludes neck accessories; each existing option retains visible pixels. |
| RunelordDwarf.mesh | build-2-sturdy.png | build / 2 | 7,8,9 | 570556 | inspected-native-and-50x100 | Beard partly occludes neck accessories; each existing option retains visible pixels. |
| RunelordDwarf.mesh | hair-1-swept.png | hair / 1 | 1,3,5,7,9,11 | 22938 | inspected-native-and-50x100 | Beard partly occludes neck accessories; each existing option retains visible pixels. |
| RunelordDwarf.mesh | hair-2-waves.png | hair / 2 | 2,4,6,8,10,12 | 31047 | inspected-native-and-50x100 | Beard partly occludes neck accessories; each existing option retains visible pixels. |
| RunelordDwarf.mesh | eyes-1-narrow.png | eyes / 1 | 1,4,7,10 | 18600 | inspected-native-and-50x100 | Beard partly occludes neck accessories; each existing option retains visible pixels. |
| RunelordDwarf.mesh | eyes-2-round.png | eyes / 2 | 2,5,8,11 | 17183 | inspected-native-and-50x100 | Beard partly occludes neck accessories; each existing option retains visible pixels. |
| RunelordDwarf.mesh | eyes-3-eyepatch.png | eyes / 3 | 3,6,9,12 | 19822 | inspected-native-and-50x100 | Beard partly occludes neck accessories; each existing option retains visible pixels. |
| RunelordDwarf.mesh | nose-1-crooked.png | nose / 1 | 1,3,5,7,9,11 | 7962 | inspected-native-and-50x100 | Beard partly occludes neck accessories; each existing option retains visible pixels. |
| RunelordDwarf.mesh | nose-2-broad.png | nose / 2 | 2,4,6,8,10,12 | 5947 | inspected-native-and-50x100 | Beard partly occludes neck accessories; each existing option retains visible pixels. |
| RunelordDwarf.mesh | mouth-1-teeth.png | mouth / 1 | 1,4,7,10 | 11574 | inspected-native-and-50x100 | Beard partly occludes neck accessories; each existing option retains visible pixels. |
| RunelordDwarf.mesh | mouth-2-grin.png | mouth / 2 | 2,5,8,11 | 11771 | inspected-native-and-50x100 | Beard partly occludes neck accessories; each existing option retains visible pixels. |
| RunelordDwarf.mesh | mouth-3-cigar.png | mouth / 3 | 3,6,9,12 | 9869 | inspected-native-and-50x100 | Beard partly occludes neck accessories; each existing option retains visible pixels. |
| RunelordDwarf.mesh | chin-1-forked.png | chin / 1 | 1,3,5,7,9,11 | 83523 | inspected-native-and-50x100 | Beard partly occludes neck accessories; each existing option retains visible pixels. |
| RunelordDwarf.mesh | chin-2-braided.png | chin / 2 | 2,4,6,8,10,12 | 92189 | inspected-native-and-50x100 | Beard partly occludes neck accessories; each existing option retains visible pixels. |
| RunelordDwarf.mesh | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 1488 | inspected-native-and-50x100 | Beard partly occludes neck accessories; each existing option retains visible pixels. |
| RunelordDwarf.mesh | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 3234 | inspected-native-and-50x100 | Beard partly occludes neck accessories; each existing option retains visible pixels. |
| RunelordDwarf.mesh | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 5476 | inspected-native-and-50x100 | Beard partly occludes neck accessories; each existing option retains visible pixels. |
| RunelordDwarf.mesh | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11 | 18417 | inspected-native-and-50x100 | Beard partly occludes neck accessories; each existing option retains visible pixels. |
| RunelordDwarf.mesh | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12 | 34819 | inspected-native-and-50x100 | Beard partly occludes neck accessories; each existing option retains visible pixels. |
| RunelordDwarf.mesh | outfit-1-padded-build0.png | outfit / 1 | 1,2,3,4,5,6,7,8,9,10,11,12 | 930093 | inspected-native-and-50x100 | Beard partly occludes neck accessories; each existing option retains visible pixels. |
| RunelordDwarf.mesh-female | build-1-slim.png | build / 1 | 4,5,6 | 603865 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| RunelordDwarf.mesh-female | build-2-sturdy.png | build / 2 | 7,8,9 | 540774 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| RunelordDwarf.mesh-female | hair-1-swept.png | hair / 1 | 1,3,5,7,9,11 | 54948 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| RunelordDwarf.mesh-female | hair-2-waves.png | hair / 2 | 2,4,6,8,10,12 | 66347 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| RunelordDwarf.mesh-female | eyes-1-narrow.png | eyes / 1 | 1,4,7,10 | 16711 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| RunelordDwarf.mesh-female | eyes-2-round.png | eyes / 2 | 2,5,8,11 | 16047 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| RunelordDwarf.mesh-female | eyes-3-eyepatch.png | eyes / 3 | 3,6,9,12 | 20322 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| RunelordDwarf.mesh-female | nose-1-crooked.png | nose / 1 | 1,3,5,7,9,11 | 8380 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| RunelordDwarf.mesh-female | nose-2-broad.png | nose / 2 | 2,4,6,8,10,12 | 7973 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| RunelordDwarf.mesh-female | mouth-1-teeth.png | mouth / 1 | 1,4,7,10 | 9041 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| RunelordDwarf.mesh-female | mouth-2-grin.png | mouth / 2 | 2,5,8,11 | 11000 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| RunelordDwarf.mesh-female | mouth-3-cigar.png | mouth / 3 | 3,6,9,12 | 8702 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| RunelordDwarf.mesh-female | chin-1-square.png | chin / 1 | 1,3,5,7,9,11 | 15191 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| RunelordDwarf.mesh-female | chin-2-rounded.png | chin / 2 | 2,4,6,8,10,12 | 12600 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| RunelordDwarf.mesh-female | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 1436 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| RunelordDwarf.mesh-female | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 3118 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| RunelordDwarf.mesh-female | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 3706 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| RunelordDwarf.mesh-female | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11 | 22006 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| RunelordDwarf.mesh-female | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12 | 26248 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| RunelordDwarf.mesh-female | outfit-1-padded-build0.png | outfit / 1 | 1,2,3,4,5,6,7,8,9,10,11,12 | 871212 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Gnome.mesh | build-1-slim.png | build / 1 | 4,5,6 | 590488 | inspected-native-and-50x100 | No hair slot exists; existing face and accessory options remain visible. |
| Gnome.mesh | build-2-sturdy.png | build / 2 | 7,8,9 | 593256 | inspected-native-and-50x100 | No hair slot exists; existing face and accessory options remain visible. |
| Gnome.mesh | eyes-1-narrow.png | eyes / 1 | 1,4,7,10 | 29029 | inspected-native-and-50x100 | No hair slot exists; existing face and accessory options remain visible. |
| Gnome.mesh | eyes-2-round.png | eyes / 2 | 2,5,8,11 | 32450 | inspected-native-and-50x100 | No hair slot exists; existing face and accessory options remain visible. |
| Gnome.mesh | eyes-3-eyepatch.png | eyes / 3 | 3,6,9,12 | 28039 | inspected-native-and-50x100 | No hair slot exists; existing face and accessory options remain visible. |
| Gnome.mesh | nose-1-crooked.png | nose / 1 | 1,3,5,7,9,11 | 11358 | inspected-native-and-50x100 | No hair slot exists; existing face and accessory options remain visible. |
| Gnome.mesh | nose-2-broad.png | nose / 2 | 2,4,6,8,10,12 | 15179 | inspected-native-and-50x100 | No hair slot exists; existing face and accessory options remain visible. |
| Gnome.mesh | mouth-1-teeth.png | mouth / 1 | 1,4,7,10 | 15199 | inspected-native-and-50x100 | No hair slot exists; existing face and accessory options remain visible. |
| Gnome.mesh | mouth-2-grin.png | mouth / 2 | 2,5,8,11 | 12831 | inspected-native-and-50x100 | No hair slot exists; existing face and accessory options remain visible. |
| Gnome.mesh | mouth-3-cigar.png | mouth / 3 | 3,6,9,12 | 13631 | inspected-native-and-50x100 | No hair slot exists; existing face and accessory options remain visible. |
| Gnome.mesh | chin-1-square.png | chin / 1 | 1,3,5,7,9,11 | 7046 | inspected-native-and-50x100 | No hair slot exists; existing face and accessory options remain visible. |
| Gnome.mesh | chin-2-rounded.png | chin / 2 | 2,4,6,8,10,12 | 7597 | inspected-native-and-50x100 | No hair slot exists; existing face and accessory options remain visible. |
| Gnome.mesh | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 2132 | inspected-native-and-50x100 | No hair slot exists; existing face and accessory options remain visible. |
| Gnome.mesh | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 3143 | inspected-native-and-50x100 | No hair slot exists; existing face and accessory options remain visible. |
| Gnome.mesh | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 3945 | inspected-native-and-50x100 | No hair slot exists; existing face and accessory options remain visible. |
| Gnome.mesh | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11 | 17148 | inspected-native-and-50x100 | No hair slot exists; existing face and accessory options remain visible. |
| Gnome.mesh | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12 | 26652 | inspected-native-and-50x100 | No hair slot exists; existing face and accessory options remain visible. |
| Gnome.mesh | outfit-1-tunic-build0.png | outfit / 1 | 1,2,3,4,5,6,7,8,9,10,11,12 | 917447 | inspected-native-and-50x100 | No hair slot exists; existing face and accessory options remain visible. |
| Gnome.mesh-female | build-1-slim.png | build / 1 | 4,5,6 | 526784 | inspected-native-and-50x100 | Both hair options contain side strands and a transparent crown; no missing loaded crown pixels. |
| Gnome.mesh-female | build-2-sturdy.png | build / 2 | 7,8,9 | 562462 | inspected-native-and-50x100 | Both hair options contain side strands and a transparent crown; no missing loaded crown pixels. |
| Gnome.mesh-female | hair-1-swept.png | hair / 1 | 1,3,5,7,9,11 | 72207 | inspected-native-and-50x100 | Both hair options contain side strands and a transparent crown; no missing loaded crown pixels. |
| Gnome.mesh-female | hair-2-waves.png | hair / 2 | 2,4,6,8,10,12 | 123616 | inspected-native-and-50x100 | Both hair options contain side strands and a transparent crown; no missing loaded crown pixels. |
| Gnome.mesh-female | eyes-1-narrow.png | eyes / 1 | 1,4,7,10 | 34095 | inspected-native-and-50x100 | Both hair options contain side strands and a transparent crown; no missing loaded crown pixels. |
| Gnome.mesh-female | eyes-2-round.png | eyes / 2 | 2,5,8,11 | 33760 | inspected-native-and-50x100 | Both hair options contain side strands and a transparent crown; no missing loaded crown pixels. |
| Gnome.mesh-female | eyes-3-eyepatch.png | eyes / 3 | 3,6,9,12 | 31047 | inspected-native-and-50x100 | Both hair options contain side strands and a transparent crown; no missing loaded crown pixels. |
| Gnome.mesh-female | nose-1-crooked.png | nose / 1 | 1,3,5,7,9,11 | 11056 | inspected-native-and-50x100 | Both hair options contain side strands and a transparent crown; no missing loaded crown pixels. |
| Gnome.mesh-female | nose-2-broad.png | nose / 2 | 2,4,6,8,10,12 | 14817 | inspected-native-and-50x100 | Both hair options contain side strands and a transparent crown; no missing loaded crown pixels. |
| Gnome.mesh-female | mouth-1-teeth.png | mouth / 1 | 1,4,7,10 | 13634 | inspected-native-and-50x100 | Both hair options contain side strands and a transparent crown; no missing loaded crown pixels. |
| Gnome.mesh-female | mouth-2-grin.png | mouth / 2 | 2,5,8,11 | 11209 | inspected-native-and-50x100 | Both hair options contain side strands and a transparent crown; no missing loaded crown pixels. |
| Gnome.mesh-female | mouth-3-cigar.png | mouth / 3 | 3,6,9,12 | 15934 | inspected-native-and-50x100 | Both hair options contain side strands and a transparent crown; no missing loaded crown pixels. |
| Gnome.mesh-female | chin-1-square.png | chin / 1 | 1,3,5,7,9,11 | 12636 | inspected-native-and-50x100 | Both hair options contain side strands and a transparent crown; no missing loaded crown pixels. |
| Gnome.mesh-female | chin-2-rounded.png | chin / 2 | 2,4,6,8,10,12 | 12872 | inspected-native-and-50x100 | Both hair options contain side strands and a transparent crown; no missing loaded crown pixels. |
| Gnome.mesh-female | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 3049 | inspected-native-and-50x100 | Both hair options contain side strands and a transparent crown; no missing loaded crown pixels. |
| Gnome.mesh-female | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 3397 | inspected-native-and-50x100 | Both hair options contain side strands and a transparent crown; no missing loaded crown pixels. |
| Gnome.mesh-female | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 2804 | inspected-native-and-50x100 | Both hair options contain side strands and a transparent crown; no missing loaded crown pixels. |
| Gnome.mesh-female | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11 | 18277 | inspected-native-and-50x100 | Both hair options contain side strands and a transparent crown; no missing loaded crown pixels. |
| Gnome.mesh-female | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12 | 31482 | inspected-native-and-50x100 | Both hair options contain side strands and a transparent crown; no missing loaded crown pixels. |
| Gnome.mesh-female | outfit-1-tunic-build0.png | outfit / 1 | 1,2,3,4,5,6,7,8,9,10,11,12 | 730479 | inspected-native-and-50x100 | Both hair options contain side strands and a transparent crown; no missing loaded crown pixels. |
| Adventurer.mesh | build-1-slim.png | build / 1 | 4,5,6 | 632672 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh | build-2-sturdy.png | build / 2 | 7,8,9 | 651977 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh | hair-1-swept.png | hair / 1 | 1,3,5,7,9,11 | 142091 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh | hair-2-waves.png | hair / 2 | 2,4,6,8,10,12 | 134830 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh | ears-1-notched.png | ears / 1 | 1,3,5,7,9,11 | 9435 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh | ears-2-rounder.png | ears / 2 | 2,4,6,8,10,12 | 9966 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh | eyes-1-narrow.png | eyes / 1 | 1,4,7,10 | 23455 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh | eyes-2-round.png | eyes / 2 | 2,5,8,11 | 26688 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh | eyes-3-eyepatch.png | eyes / 3 | 3,6,9,12 | 24353 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh | nose-1-crooked.png | nose / 1 | 1,3,5,7,9,11 | 11211 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh | nose-2-broad.png | nose / 2 | 2,4,6,8,10,12 | 12734 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh | mouth-1-teeth.png | mouth / 1 | 1,4,7,10 | 10397 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh | mouth-2-grin.png | mouth / 2 | 2,5,8,11 | 10641 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh | mouth-3-cigar.png | mouth / 3 | 3,6,9,12 | 13177 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh | chin-1-square.png | chin / 1 | 1,3,5,7,9,11 | 15360 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh | chin-2-rounded.png | chin / 2 | 2,4,6,8,10,12 | 11746 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 1468 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 3291 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 3674 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11 | 11955 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12 | 21959 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh | outfit-1-leather-build0.png | outfit / 1 | 1,2,3,4,5,6,7,8,9,10,11,12 | 891965 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh-female | build-1-slim.png | build / 1 | 4,5,6 | 606516 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh-female | build-2-sturdy.png | build / 2 | 7,8,9 | 662726 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh-female | hair-1-swept.png | hair / 1 | 1,3,5,7,9,11 | 159363 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh-female | hair-2-waves.png | hair / 2 | 2,4,6,8,10,12 | 184239 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh-female | ears-1-notched.png | ears / 1 | 1,3,5,7,9,11 | 9345 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh-female | ears-2-rounder.png | ears / 2 | 2,4,6,8,10,12 | 10826 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh-female | eyes-1-narrow.png | eyes / 1 | 1,4,7,10 | 22869 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh-female | eyes-2-round.png | eyes / 2 | 2,5,8,11 | 27615 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh-female | eyes-3-eyepatch.png | eyes / 3 | 3,6,9,12 | 26182 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh-female | nose-1-crooked.png | nose / 1 | 1,3,5,7,9,11 | 11578 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh-female | nose-2-broad.png | nose / 2 | 2,4,6,8,10,12 | 14263 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh-female | mouth-1-teeth.png | mouth / 1 | 1,4,7,10 | 10730 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh-female | mouth-2-grin.png | mouth / 2 | 2,5,8,11 | 8389 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh-female | mouth-3-cigar.png | mouth / 3 | 3,6,9,12 | 12291 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh-female | chin-1-square.png | chin / 1 | 1,3,5,7,9,11 | 10850 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh-female | chin-2-rounded.png | chin / 2 | 2,4,6,8,10,12 | 11236 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh-female | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 1617 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh-female | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 3933 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh-female | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 2819 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh-female | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11 | 16260 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh-female | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12 | 19599 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Adventurer.mesh-female | outfit-1-leather-build0.png | outfit / 1 | 1,2,3,4,5,6,7,8,9,10,11,12 | 882644 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Monk.mesh | build-1-slim.png | build / 1 | 4,5,6 | 699647 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Monk.mesh | build-2-sturdy.png | build / 2 | 7,8,9 | 669057 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Monk.mesh | hair-1-swept.png | hair / 1 | 1,3,5,7,9,11 | 114926 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Monk.mesh | hair-2-waves.png | hair / 2 | 2,4,6,8,10,12 | 130014 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Monk.mesh | ears-1-notched.png | ears / 1 | 1,3,5,7,9,11 | 7895 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Monk.mesh | ears-2-rounder.png | ears / 2 | 2,4,6,8,10,12 | 8703 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Monk.mesh | eyes-1-narrow.png | eyes / 1 | 1,4,7,10 | 19438 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Monk.mesh | eyes-2-round.png | eyes / 2 | 2,5,8,11 | 26505 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Monk.mesh | eyes-3-eyepatch.png | eyes / 3 | 3,6,9,12 | 26399 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Monk.mesh | nose-1-crooked.png | nose / 1 | 1,3,5,7,9,11 | 12247 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Monk.mesh | nose-2-broad.png | nose / 2 | 2,4,6,8,10,12 | 10373 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Monk.mesh | mouth-1-teeth.png | mouth / 1 | 1,4,7,10 | 11528 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Monk.mesh | mouth-2-grin.png | mouth / 2 | 2,5,8,11 | 12746 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Monk.mesh | mouth-3-cigar.png | mouth / 3 | 3,6,9,12 | 12796 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Monk.mesh | chin-1-square.png | chin / 1 | 1,3,5,7,9,11 | 16172 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Monk.mesh | chin-2-rounded.png | chin / 2 | 2,4,6,8,10,12 | 16937 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Monk.mesh | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 1863 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Monk.mesh | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 2888 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Monk.mesh | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 3830 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Monk.mesh | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11 | 17205 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Monk.mesh | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12 | 25448 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Monk.mesh | outfit-1-robe-build0.png | outfit / 1 | 1,2,3,4,5,6,7,8,9,10,11,12 | 946494 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Monk.mesh-female | build-1-slim.png | build / 1 | 4,5,6 | 631115 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured nose and upper-lip overlap; option 1 unchanged. |
| Monk.mesh-female | build-2-sturdy.png | build / 2 | 7,8,9 | 712071 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured nose and upper-lip overlap; option 1 unchanged. |
| Monk.mesh-female | hair-1-swept.png | hair / 1 | 1,3,5,7,9,11 | 223931 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured nose and upper-lip overlap; option 1 unchanged. |
| Monk.mesh-female | hair-2-waves.png | hair / 2 | 2,4,6,8,10,12 | 173510 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured nose and upper-lip overlap; option 1 unchanged. |
| Monk.mesh-female | ears-1-notched.png | ears / 1 | 1,3,5,7,9,11 | 7754 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured nose and upper-lip overlap; option 1 unchanged. |
| Monk.mesh-female | ears-2-rounder.png | ears / 2 | 2,4,6,8,10,12 | 7570 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured nose and upper-lip overlap; option 1 unchanged. |
| Monk.mesh-female | eyes-1-narrow.png | eyes / 1 | 1,4,7,10 | 20229 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured nose and upper-lip overlap; option 1 unchanged. |
| Monk.mesh-female | eyes-2-round.png | eyes / 2 | 2,5,8,11 | 26973 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured nose and upper-lip overlap; option 1 unchanged. |
| Monk.mesh-female | eyes-3-eyepatch.png | eyes / 3 | 3,6,9,12 | 24505 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured nose and upper-lip overlap; option 1 unchanged. |
| Monk.mesh-female | nose-1-crooked.png | nose / 1 | 1,3,5,7,9,11 | 10818 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured nose and upper-lip overlap; option 1 unchanged. |
| Monk.mesh-female | nose-2-broad.png | nose / 2 | 2,4,6,8,10,12 | 12439 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured nose and upper-lip overlap; option 1 unchanged. |
| Monk.mesh-female | mouth-1-teeth.png | mouth / 1 | 1,4,7,10 | 10761 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured nose and upper-lip overlap; option 1 unchanged. |
| Monk.mesh-female | mouth-2-grin.png | mouth / 2 | 2,5,8,11 | 10445 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured nose and upper-lip overlap; option 1 unchanged. |
| Monk.mesh-female | mouth-3-cigar.png | mouth / 3 | 3,6,9,12 | 12396 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured nose and upper-lip overlap; option 1 unchanged. |
| Monk.mesh-female | chin-1-square.png | chin / 1 | 1,3,5,7,9,11 | 17937 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured nose and upper-lip overlap; option 1 unchanged. |
| Monk.mesh-female | chin-2-rounded.png | chin / 2 | 2,4,6,8,10,12 | 16107 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured nose and upper-lip overlap; option 1 unchanged. |
| Monk.mesh-female | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 1476 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured nose and upper-lip overlap; option 1 unchanged. |
| Monk.mesh-female | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 4316 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured nose and upper-lip overlap; option 1 unchanged. |
| Monk.mesh-female | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 3295 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured nose and upper-lip overlap; option 1 unchanged. |
| Monk.mesh-female | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11 | 18440 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured nose and upper-lip overlap; option 1 unchanged. |
| Monk.mesh-female | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12 | 26127 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured nose and upper-lip overlap; option 1 unchanged. |
| Monk.mesh-female | outfit-1-robe-build0.png | outfit / 1 | 1,2,3,4,5,6,7,8,9,10,11,12 | 917822 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured nose and upper-lip overlap; option 1 unchanged. |
| Defender.mesh | build-1-slim.png | build / 1 | 4,5,6 | 594062 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh | build-2-sturdy.png | build / 2 | 7,8,9 | 622631 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh | hair-1-swept.png | hair / 1 | 1,3,5,7,9,11 | 29864 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh | hair-2-waves.png | hair / 2 | 2,4,6,8,10,12 | 33150 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh | ears-1-notched.png | ears / 1 | 1,3,5,7,9,11 | 9445 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh | ears-2-rounder.png | ears / 2 | 2,4,6,8,10,12 | 9760 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh | eyes-1-narrow.png | eyes / 1 | 1,4,7,10 | 23653 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh | eyes-2-round.png | eyes / 2 | 2,5,8,11 | 26779 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh | eyes-3-eyepatch.png | eyes / 3 | 3,6,9,12 | 24943 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh | nose-1-crooked.png | nose / 1 | 1,3,5,7,9,11 | 12527 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh | nose-2-broad.png | nose / 2 | 2,4,6,8,10,12 | 14818 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh | mouth-1-teeth.png | mouth / 1 | 1,4,7,10 | 12473 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh | mouth-2-grin.png | mouth / 2 | 2,5,8,11 | 11277 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh | mouth-3-cigar.png | mouth / 3 | 3,6,9,12 | 12249 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh | chin-1-forked.png | chin / 1 | 1,3,5,7,9,11 | 6888 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh | chin-2-braided.png | chin / 2 | 2,4,6,8,10,12 | 5150 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 1577 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 3285 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 2854 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11 | 17352 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12 | 28603 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh | outfit-1-padded-build0.png | outfit / 1 | 1,2,3,4,5,6,7,8,9,10,11,12 | 852514 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh-female | build-1-slim.png | build / 1 | 4,5,6 | 552968 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh-female | build-2-sturdy.png | build / 2 | 7,8,9 | 605591 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh-female | hair-1-swept.png | hair / 1 | 1,3,5,7,9,11 | 124722 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh-female | hair-2-waves.png | hair / 2 | 2,4,6,8,10,12 | 130787 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh-female | ears-1-notched.png | ears / 1 | 1,3,5,7,9,11 | 9943 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh-female | ears-2-rounder.png | ears / 2 | 2,4,6,8,10,12 | 9758 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh-female | eyes-1-narrow.png | eyes / 1 | 1,4,7,10 | 27440 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh-female | eyes-2-round.png | eyes / 2 | 2,5,8,11 | 29535 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh-female | eyes-3-eyepatch.png | eyes / 3 | 3,6,9,12 | 29941 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh-female | nose-1-crooked.png | nose / 1 | 1,3,5,7,9,11 | 10829 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh-female | nose-2-broad.png | nose / 2 | 2,4,6,8,10,12 | 14881 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh-female | mouth-1-teeth.png | mouth / 1 | 1,4,7,10 | 10169 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh-female | mouth-2-grin.png | mouth / 2 | 2,5,8,11 | 11278 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh-female | mouth-3-cigar.png | mouth / 3 | 3,6,9,12 | 11824 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh-female | chin-1-square.png | chin / 1 | 1,3,5,7,9,11 | 17614 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh-female | chin-2-rounded.png | chin / 2 | 2,4,6,8,10,12 | 13554 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh-female | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 1974 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh-female | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 3525 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh-female | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 3435 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh-female | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11 | 16042 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh-female | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12 | 22305 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Defender.mesh-female | outfit-1-padded-build0.png | outfit / 1 | 1,2,3,4,5,6,7,8,9,10,11,12 | 823439 | inspected-native-and-50x100 | Both ear roots moved from the outer eyebrow to the outer temple. |
| Wizard.mesh | build-1-slim.png | build / 1 | 4,5,6 | 497805 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh | build-2-sturdy.png | build / 2 | 7,8,9 | 494120 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh | hair-1-swept.png | hair / 1 | 1,3,5,7,9,11 | 22708 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh | hair-2-waves.png | hair / 2 | 2,4,6,8,10,12 | 24546 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh | ears-1-notched.png | ears / 1 | 1,3,5,7,9,11 | 6038 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh | ears-2-rounder.png | ears / 2 | 2,4,6,8,10,12 | 6304 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh | eyes-1-narrow.png | eyes / 1 | 1,4,7,10 | 12676 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh | eyes-2-round.png | eyes / 2 | 2,5,8,11 | 13410 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh | eyes-3-eyepatch.png | eyes / 3 | 3,6,9,12 | 14928 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh | nose-1-crooked.png | nose / 1 | 1,3,5,7,9,11 | 8063 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh | nose-2-broad.png | nose / 2 | 2,4,6,8,10,12 | 7218 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh | mouth-1-teeth.png | mouth / 1 | 1,4,7,10 | 8655 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh | mouth-2-grin.png | mouth / 2 | 2,5,8,11 | 9553 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh | mouth-3-cigar.png | mouth / 3 | 3,6,9,12 | 9972 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh | chin-1-forked.png | chin / 1 | 1,3,5,7,9,11 | 5336 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh | chin-2-braided.png | chin / 2 | 2,4,6,8,10,12 | 4963 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 1281 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 2212 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 3269 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11 | 13304 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12 | 19487 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh | outfit-1-robe-build0.png | outfit / 1 | 1,2,3,4,5,6,7,8,9,10,11,12 | 682226 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh-female | build-1-slim.png | build / 1 | 4,5,6 | 485172 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh-female | build-2-sturdy.png | build / 2 | 7,8,9 | 442231 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh-female | hair-1-swept.png | hair / 1 | 1,3,5,7,9,11 | 18217 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh-female | hair-2-waves.png | hair / 2 | 2,4,6,8,10,12 | 21948 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh-female | ears-1-notched.png | ears / 1 | 1,3,5,7,9,11 | 6363 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh-female | ears-2-rounder.png | ears / 2 | 2,4,6,8,10,12 | 6421 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh-female | eyes-1-narrow.png | eyes / 1 | 1,4,7,10 | 11383 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh-female | eyes-2-round.png | eyes / 2 | 2,5,8,11 | 12453 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh-female | eyes-3-eyepatch.png | eyes / 3 | 3,6,9,12 | 15379 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh-female | nose-1-crooked.png | nose / 1 | 1,3,5,7,9,11 | 8206 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh-female | nose-2-broad.png | nose / 2 | 2,4,6,8,10,12 | 8185 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh-female | mouth-1-teeth.png | mouth / 1 | 1,4,7,10 | 8222 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh-female | mouth-2-grin.png | mouth / 2 | 2,5,8,11 | 7946 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh-female | mouth-3-cigar.png | mouth / 3 | 3,6,9,12 | 9952 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh-female | chin-1-square.png | chin / 1 | 1,3,5,7,9,11 | 11569 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh-female | chin-2-rounded.png | chin / 2 | 2,4,6,8,10,12 | 15010 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh-female | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 1352 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh-female | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 2666 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh-female | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 2163 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh-female | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11 | 13432 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh-female | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12 | 21056 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Wizard.mesh-female | outfit-1-robe-build0.png | outfit / 1 | 1,2,3,4,5,6,7,8,9,10,11,12 | 685221 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Elf.mesh-male | build-1-slim.png | build / 1 | 4,5,6 | 536927 | inspected-native-and-50x100 | Only ear option 1 moved to the matching temple; option 2 unchanged. |
| Elf.mesh-male | build-2-sturdy.png | build / 2 | 7,8,9 | 565072 | inspected-native-and-50x100 | Only ear option 1 moved to the matching temple; option 2 unchanged. |
| Elf.mesh-male | hair-1-swept.png | hair / 1 | 1,3,5,7,9,11 | 193517 | inspected-native-and-50x100 | Only ear option 1 moved to the matching temple; option 2 unchanged. |
| Elf.mesh-male | hair-2-waves.png | hair / 2 | 2,4,6,8,10,12 | 198888 | inspected-native-and-50x100 | Only ear option 1 moved to the matching temple; option 2 unchanged. |
| Elf.mesh-male | ears-1-notched.png | ears / 1 | 1,3,5,7,9,11 | 12948 | inspected-native-and-50x100 | Only ear option 1 moved to the matching temple; option 2 unchanged. |
| Elf.mesh-male | ears-2-rounder.png | ears / 2 | 2,4,6,8,10,12 | 28204 | inspected-native-and-50x100 | Only ear option 1 moved to the matching temple; option 2 unchanged. |
| Elf.mesh-male | eyes-1-narrow.png | eyes / 1 | 1,4,7,10 | 20110 | inspected-native-and-50x100 | Only ear option 1 moved to the matching temple; option 2 unchanged. |
| Elf.mesh-male | eyes-2-round.png | eyes / 2 | 2,5,8,11 | 29758 | inspected-native-and-50x100 | Only ear option 1 moved to the matching temple; option 2 unchanged. |
| Elf.mesh-male | eyes-3-eyepatch.png | eyes / 3 | 3,6,9,12 | 18539 | inspected-native-and-50x100 | Only ear option 1 moved to the matching temple; option 2 unchanged. |
| Elf.mesh-male | nose-1-crooked.png | nose / 1 | 1,3,5,7,9,11 | 11712 | inspected-native-and-50x100 | Only ear option 1 moved to the matching temple; option 2 unchanged. |
| Elf.mesh-male | nose-2-broad.png | nose / 2 | 2,4,6,8,10,12 | 15552 | inspected-native-and-50x100 | Only ear option 1 moved to the matching temple; option 2 unchanged. |
| Elf.mesh-male | mouth-1-teeth.png | mouth / 1 | 1,4,7,10 | 16764 | inspected-native-and-50x100 | Only ear option 1 moved to the matching temple; option 2 unchanged. |
| Elf.mesh-male | mouth-2-grin.png | mouth / 2 | 2,5,8,11 | 14791 | inspected-native-and-50x100 | Only ear option 1 moved to the matching temple; option 2 unchanged. |
| Elf.mesh-male | mouth-3-cigar.png | mouth / 3 | 3,6,9,12 | 16413 | inspected-native-and-50x100 | Only ear option 1 moved to the matching temple; option 2 unchanged. |
| Elf.mesh-male | chin-1-square.png | chin / 1 | 1,3,5,7,9,11 | 13638 | inspected-native-and-50x100 | Only ear option 1 moved to the matching temple; option 2 unchanged. |
| Elf.mesh-male | chin-2-rounded.png | chin / 2 | 2,4,6,8,10,12 | 9743 | inspected-native-and-50x100 | Only ear option 1 moved to the matching temple; option 2 unchanged. |
| Elf.mesh-male | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 1166 | inspected-native-and-50x100 | Only ear option 1 moved to the matching temple; option 2 unchanged. |
| Elf.mesh-male | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 2993 | inspected-native-and-50x100 | Only ear option 1 moved to the matching temple; option 2 unchanged. |
| Elf.mesh-male | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 3181 | inspected-native-and-50x100 | Only ear option 1 moved to the matching temple; option 2 unchanged. |
| Elf.mesh-male | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11 | 12157 | inspected-native-and-50x100 | Only ear option 1 moved to the matching temple; option 2 unchanged. |
| Elf.mesh-male | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12 | 23065 | inspected-native-and-50x100 | Only ear option 1 moved to the matching temple; option 2 unchanged. |
| Elf.mesh-male | outfit-1-tunic-build0.png | outfit / 1 | 1,2,3,4,5,6,7,8,9,10,11,12 | 725849 | inspected-native-and-50x100 | Only ear option 1 moved to the matching temple; option 2 unchanged. |
| Elf.mesh | build-1-slim.png | build / 1 | 4,5,6 | 506673 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Elf.mesh | build-2-sturdy.png | build / 2 | 7,8,9 | 549508 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Elf.mesh | hair-1-swept.png | hair / 1 | 1,3,5,7,9,11 | 146374 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Elf.mesh | hair-2-waves.png | hair / 2 | 2,4,6,8,10,12 | 182272 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Elf.mesh | ears-1-notched.png | ears / 1 | 1,3,5,7,9,11 | 22242 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Elf.mesh | ears-2-rounder.png | ears / 2 | 2,4,6,8,10,12 | 28852 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Elf.mesh | eyes-1-narrow.png | eyes / 1 | 1,4,7,10 | 27677 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Elf.mesh | eyes-2-round.png | eyes / 2 | 2,5,8,11 | 34822 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Elf.mesh | eyes-3-eyepatch.png | eyes / 3 | 3,6,9,12 | 21170 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Elf.mesh | nose-1-crooked.png | nose / 1 | 1,3,5,7,9,11 | 12191 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Elf.mesh | nose-2-broad.png | nose / 2 | 2,4,6,8,10,12 | 13811 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Elf.mesh | mouth-1-teeth.png | mouth / 1 | 1,4,7,10 | 13689 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Elf.mesh | mouth-2-grin.png | mouth / 2 | 2,5,8,11 | 14683 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Elf.mesh | mouth-3-cigar.png | mouth / 3 | 3,6,9,12 | 18127 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Elf.mesh | chin-1-square.png | chin / 1 | 1,3,5,7,9,11 | 10178 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Elf.mesh | chin-2-rounded.png | chin / 2 | 2,4,6,8,10,12 | 8098 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Elf.mesh | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 1597 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Elf.mesh | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 2799 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Elf.mesh | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 3752 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Elf.mesh | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11 | 12863 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Elf.mesh | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12 | 28597 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Elf.mesh | outfit-1-tunic-build0.png | outfit / 1 | 1,2,3,4,5,6,7,8,9,10,11,12 | 772818 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh-male | build-1-slim.png | build / 1 | 4,5,6 | 555181 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh-male | build-2-sturdy.png | build / 2 | 7,8,9 | 572994 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh-male | hair-1-swept.png | hair / 1 | 1,3,5,7,9,11 | 157225 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh-male | hair-2-waves.png | hair / 2 | 2,4,6,8,10,12 | 171165 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh-male | ears-1-notched.png | ears / 1 | 1,3,5,7,9,11 | 20521 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh-male | ears-2-rounder.png | ears / 2 | 2,4,6,8,10,12 | 28813 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh-male | eyes-1-narrow.png | eyes / 1 | 1,4,7,10 | 26873 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh-male | eyes-2-round.png | eyes / 2 | 2,5,8,11 | 31112 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh-male | eyes-3-eyepatch.png | eyes / 3 | 3,6,9,12 | 21367 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh-male | nose-1-crooked.png | nose / 1 | 1,3,5,7,9,11 | 14187 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh-male | nose-2-broad.png | nose / 2 | 2,4,6,8,10,12 | 16574 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh-male | mouth-1-teeth.png | mouth / 1 | 1,4,7,10 | 17616 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh-male | mouth-2-grin.png | mouth / 2 | 2,5,8,11 | 14869 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh-male | mouth-3-cigar.png | mouth / 3 | 3,6,9,12 | 17995 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh-male | chin-1-square.png | chin / 1 | 1,3,5,7,9,11 | 13453 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh-male | chin-2-rounded.png | chin / 2 | 2,4,6,8,10,12 | 10788 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh-male | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 1746 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh-male | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 3469 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh-male | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 4187 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh-male | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11 | 14437 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh-male | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12 | 23950 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh-male | outfit-1-tunic-build0.png | outfit / 1 | 1,2,3,4,5,6,7,8,9,10,11,12 | 833628 | known-accepted-cosmetic | Known accepted cosmetic issue: bright rectangular neckline transition at canvas x210..590,y825..865 in the unchanged outfit source; no further composition or raster correction requested. Source/partial preview: [unchanged outfit PNG](../variants/DarkElf.mesh-male/outfit-1-tunic-build0.png), canvas ROI x210..590,y825..865. Composite preview: [existing unchanged native face panels](../variants/DarkElf.mesh-male/review/native-face-check.jpg), original canvas crop [80,190,780,955], combinations 1,2,3,4,7,8; neckline defect visible. |
| DarkElf.mesh | build-1-slim.png | build / 1 | 4,5,6 | 508740 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh | build-2-sturdy.png | build / 2 | 7,8,9 | 512353 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh | hair-1-swept.png | hair / 1 | 1,3,5,7,9,11 | 235313 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh | hair-2-waves.png | hair / 2 | 2,4,6,8,10,12 | 180535 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh | ears-1-notched.png | ears / 1 | 1,3,5,7,9,11 | 16281 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh | ears-2-rounder.png | ears / 2 | 2,4,6,8,10,12 | 25113 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh | eyes-1-narrow.png | eyes / 1 | 1,4,7,10 | 28648 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh | eyes-2-round.png | eyes / 2 | 2,5,8,11 | 31343 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh | eyes-3-eyepatch.png | eyes / 3 | 3,6,9,12 | 20511 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh | nose-1-crooked.png | nose / 1 | 1,3,5,7,9,11 | 13311 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh | nose-2-broad.png | nose / 2 | 2,4,6,8,10,12 | 14086 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh | mouth-1-teeth.png | mouth / 1 | 1,4,7,10 | 16462 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh | mouth-2-grin.png | mouth / 2 | 2,5,8,11 | 15885 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh | mouth-3-cigar.png | mouth / 3 | 3,6,9,12 | 17640 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh | chin-1-square.png | chin / 1 | 1,3,5,7,9,11 | 12165 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh | chin-2-rounded.png | chin / 2 | 2,4,6,8,10,12 | 9084 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 2562 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 2226 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 3256 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11 | 17099 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12 | 37862 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| DarkElf.mesh | outfit-1-tunic-build0.png | outfit / 1 | 1,2,3,4,5,6,7,8,9,10,11,12 | 834994 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh | build-1-slim.png | build / 1 | 4,5,6 | 506451 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh | build-2-sturdy.png | build / 2 | 7,8,9 | 533838 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh | ears-1-notched.png | ears / 1 | 1,3,5,7,9,11 | 25825 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh | ears-2-rounder.png | ears / 2 | 2,4,6,8,10,12 | 34360 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh | eyes-1-narrow.png | eyes / 1 | 1,4,7,10 | 19799 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh | eyes-2-round.png | eyes / 2 | 2,5,8,11 | 22005 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh | eyes-3-eyepatch.png | eyes / 3 | 3,6,9,12 | 18073 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh | nose-1-crooked.png | nose / 1 | 1,3,5,7,9,11 | 16417 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh | nose-2-broad.png | nose / 2 | 2,4,6,8,10,12 | 19646 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh | mouth-1-teeth.png | mouth / 1 | 1,4,7,10 | 22093 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh | mouth-2-grin.png | mouth / 2 | 2,5,8,11 | 22967 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh | mouth-3-cigar.png | mouth / 3 | 3,6,9,12 | 18326 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh | chin-1-square.png | chin / 1 | 1,3,5,7,9,11 | 33238 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh | chin-2-rounded.png | chin / 2 | 2,4,6,8,10,12 | 36373 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 2373 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 6755 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 6880 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11 | 16830 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12 | 40900 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh | outfit-1-leather-build0.png | outfit / 1 | 1,2,3,4,5,6,7,8,9,10,11,12 | 817966 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh-female | build-1-slim.png | build / 1 | 4,5,6 | 474486 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh-female | build-2-sturdy.png | build / 2 | 7,8,9 | 466511 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh-female | hair-1-mossstrands.png | hair / 1 | 1,3,5,7,9,11 | 163446 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh-female | hair-2-mossbraids.png | hair / 2 | 2,4,6,8,10,12 | 288847 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh-female | eyes-1-narrow.png | eyes / 1 | 1,4,7,10 | 23962 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh-female | eyes-2-round.png | eyes / 2 | 2,5,8,11 | 29261 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh-female | eyes-3-eyepatch.png | eyes / 3 | 3,6,9,12 | 21326 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh-female | nose-1-crooked.png | nose / 1 | 1,3,5,7,9,11 | 18387 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh-female | nose-2-broad.png | nose / 2 | 2,4,6,8,10,12 | 18271 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh-female | mouth-1-teeth.png | mouth / 1 | 1,4,7,10 | 19307 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh-female | mouth-2-grin.png | mouth / 2 | 2,5,8,11 | 20699 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh-female | mouth-3-cigar.png | mouth / 3 | 3,6,9,12 | 19708 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh-female | chin-1-square.png | chin / 1 | 1,3,5,7,9,11 | 34696 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh-female | chin-2-rounded.png | chin / 2 | 2,4,6,8,10,12 | 32863 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh-female | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 2545 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh-female | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 6686 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh-female | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 5998 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh-female | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11 | 26839 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh-female | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12 | 39951 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Troll.mesh-female | outfit-1-leather-build0.png | outfit / 1 | 1,2,3,4,5,6,7,8,9,10,11,12 | 827486 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Lizardman.mesh | build-1-slim.png | build / 1 | 4,5,6 | 530851 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Lizardman.mesh | build-2-sturdy.png | build / 2 | 7,8,9 | 567971 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Lizardman.mesh | eyes-1-narrow.png | eyes / 1 | 1,4,7,10 | 22458 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Lizardman.mesh | eyes-2-round.png | eyes / 2 | 2,5,8,11 | 28834 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Lizardman.mesh | eyes-3-eyepatch.png | eyes / 3 | 3,6,9,12 | 16994 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Lizardman.mesh | nose-1-crooked.png | nose / 1 | 1,3,5,7,9,11 | 14521 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Lizardman.mesh | nose-2-broad.png | nose / 2 | 2,4,6,8,10,12 | 16146 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Lizardman.mesh | mouth-1-teeth.png | mouth / 1 | 1,4,7,10 | 18033 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Lizardman.mesh | mouth-2-grin.png | mouth / 2 | 2,5,8,11 | 17552 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Lizardman.mesh | mouth-3-cigar.png | mouth / 3 | 3,6,9,12 | 17479 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Lizardman.mesh | chin-1-square.png | chin / 1 | 1,3,5,7,9,11 | 18984 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Lizardman.mesh | chin-2-rounded.png | chin / 2 | 2,4,6,8,10,12 | 21630 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Lizardman.mesh | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 3750 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Lizardman.mesh | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 6964 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Lizardman.mesh | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 5993 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Lizardman.mesh | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11 | 20179 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Lizardman.mesh | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12 | 40380 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Lizardman.mesh | outfit-1-leather-build0.png | outfit / 1 | 1,2,3,4,5,6,7,8,9,10,11,12 | 843137 | inspected-native-and-50x100 | Existing native and 50x100 catalog panels inspected; no additional matching placement fault identified. |
| Lizardman.mesh-female | build-1-slim.png | build / 1 | 4,5,6 | 515694 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured eye overlap; option 1 unchanged. |
| Lizardman.mesh-female | build-2-sturdy.png | build / 2 | 7,8,9 | 515893 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured eye overlap; option 1 unchanged. |
| Lizardman.mesh-female | hair-1-shortfrill.png | hair / 1 | 1,3,5,7,9,11 | 148341 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured eye overlap; option 1 unchanged. |
| Lizardman.mesh-female | hair-2-wavefrill.png | hair / 2 | 2,4,6,8,10,12 | 165246 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured eye overlap; option 1 unchanged. |
| Lizardman.mesh-female | eyes-1-narrow.png | eyes / 1 | 1,4,7,10 | 19147 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured eye overlap; option 1 unchanged. |
| Lizardman.mesh-female | eyes-2-round.png | eyes / 2 | 2,5,8,11 | 29799 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured eye overlap; option 1 unchanged. |
| Lizardman.mesh-female | eyes-3-eyepatch.png | eyes / 3 | 3,6,9,12 | 18324 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured eye overlap; option 1 unchanged. |
| Lizardman.mesh-female | nose-1-crooked.png | nose / 1 | 1,3,5,7,9,11 | 11732 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured eye overlap; option 1 unchanged. |
| Lizardman.mesh-female | nose-2-broad.png | nose / 2 | 2,4,6,8,10,12 | 17342 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured eye overlap; option 1 unchanged. |
| Lizardman.mesh-female | mouth-1-teeth.png | mouth / 1 | 1,4,7,10 | 13828 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured eye overlap; option 1 unchanged. |
| Lizardman.mesh-female | mouth-2-grin.png | mouth / 2 | 2,5,8,11 | 13752 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured eye overlap; option 1 unchanged. |
| Lizardman.mesh-female | mouth-3-cigar.png | mouth / 3 | 3,6,9,12 | 14130 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured eye overlap; option 1 unchanged. |
| Lizardman.mesh-female | chin-1-square.png | chin / 1 | 1,3,5,7,9,11 | 11300 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured eye overlap; option 1 unchanged. |
| Lizardman.mesh-female | chin-2-rounded.png | chin / 2 | 2,4,6,8,10,12 | 11671 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured eye overlap; option 1 unchanged. |
| Lizardman.mesh-female | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 3414 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured eye overlap; option 1 unchanged. |
| Lizardman.mesh-female | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 6275 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured eye overlap; option 1 unchanged. |
| Lizardman.mesh-female | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 2314 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured eye overlap; option 1 unchanged. |
| Lizardman.mesh-female | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11 | 21011 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured eye overlap; option 1 unchanged. |
| Lizardman.mesh-female | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12 | 38384 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured eye overlap; option 1 unchanged. |
| Lizardman.mesh-female | outfit-1-leather-build0.png | outfit / 1 | 1,2,3,4,5,6,7,8,9,10,11,12 | 788062 | inspected-native-and-50x100 | Hair option 2 contour mask removes measured eye overlap; option 1 unchanged. |
| Knight.mesh | build-1-slim.png | build / 1 | 4,5,6 | 577550 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Knight.mesh | build-2-sturdy.png | build / 2 | 7,8,9 | 618530 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Knight.mesh | hair-1-swept.png | hair / 1 | 1,3,5,7,9,11,13,15 | 44542 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Knight.mesh | hair-2-waves.png | hair / 2 | 2,4,6,8,10,12,14 | 48724 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Knight.mesh | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 7307 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Knight.mesh | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 9222 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Knight.mesh | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 18452 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Knight.mesh | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11,13,15 | 17637 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Knight.mesh | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12,14 | 28569 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Knight.mesh | helmet-1-greathelm.png | helmet / 1 | 1,5,9 | 163818 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Knight.mesh | helmet-2-bascinet.png | helmet / 2 | 2,6,10 | 140673 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Knight.mesh | helmet-3-houndskull.png | helmet / 3 | 3,7,11 | 159013 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Knight.mesh | helmet-4-barrel.png | helmet / 4 | 4,8,12 | 171262 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Knight.mesh | outfit-1-plate-build0.png | outfit / 1 | 1,4,7,10,13 | 995355 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Knight.mesh | outfit-2-chain-build0.png | outfit / 2 | 2,5,8,11,14 | 958991 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Knight.mesh | outfit-3-padded-build0.png | outfit / 3 | 3,6,9,12,15 | 936642 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Knight.mesh-female | build-1-slim.png | build / 1 | 4,5,6 | 521018 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Knight.mesh-female | build-2-sturdy.png | build / 2 | 7,8,9 | 558761 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Knight.mesh-female | hair-1-swept.png | hair / 1 | 1,3,5,7,9,11,13,15 | 81595 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Knight.mesh-female | hair-2-waves.png | hair / 2 | 2,4,6,8,10,12,14 | 65530 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Knight.mesh-female | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 7277 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Knight.mesh-female | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 9260 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Knight.mesh-female | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 18633 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Knight.mesh-female | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11,13,15 | 12855 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Knight.mesh-female | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12,14 | 28721 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Knight.mesh-female | helmet-1-greathelm.png | helmet / 1 | 1,5,9 | 166783 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Knight.mesh-female | helmet-2-bascinet.png | helmet / 2 | 2,6,10 | 148163 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Knight.mesh-female | helmet-3-houndskull-corrected.png | helmet / 3 | 3,7,11 | 194119 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Knight.mesh-female | helmet-4-barrel.png | helmet / 4 | 4,8,12 | 177074 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Knight.mesh-female | outfit-1-plate-build0.png | outfit / 1 | 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 | 946865 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Cultist.mesh | build-1-slim.png | build / 1 | 4,5,6 | 556016 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Cultist.mesh | build-2-sturdy.png | build / 2 | 7,8,9 | 568029 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Cultist.mesh | hair-1-swept.png | hair / 1 | 1,3,5,7,9,11,13,15 | 48074 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Cultist.mesh | hair-2-waves.png | hair / 2 | 2,4,6,8,10,12,14 | 35704 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Cultist.mesh | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 7013 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Cultist.mesh | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 10391 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Cultist.mesh | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 13299 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Cultist.mesh | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11,13,15 | 20344 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Cultist.mesh | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12,14 | 34094 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Cultist.mesh | helmet-1-arch.png | helmet / 1 | 1,5,9 | 253523 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Cultist.mesh | helmet-2-crown.png | helmet / 2 | 2,6,10 | 211443 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Cultist.mesh | helmet-3-horned.png | helmet / 3 | 3,7,11 | 175914 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Cultist.mesh | helmet-4-ridged.png | helmet / 4 | 4,8,12 | 189873 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Cultist.mesh | outfit-1-robe-build0.png | outfit / 1 | 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 | 822906 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Cultist.mesh-female | build-1-slim.png | build / 1 | 4,5,6 | 482075 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Cultist.mesh-female | build-2-sturdy.png | build / 2 | 7,8,9 | 549178 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Cultist.mesh-female | hair-1-swept.png | hair / 1 | 1,3,5,7,9,11,13,15 | 69475 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Cultist.mesh-female | hair-2-waves.png | hair / 2 | 2,4,6,8,10,12,14 | 80361 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Cultist.mesh-female | scar-1-diagonal.png | scar / 1 | 1,4,7,10 | 5926 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Cultist.mesh-female | scar-2-scratches.png | scar / 2 | 2,5,8,11 | 10171 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Cultist.mesh-female | scar-3-warpaint.png | scar / 3 | 3,6,9,12 | 17427 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Cultist.mesh-female | neck-1-teeth.png | neck / 1 | 1,3,5,7,9,11,13,15 | 19845 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Cultist.mesh-female | neck-2-torque.png | neck / 2 | 2,4,6,8,10,12,14 | 34517 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Cultist.mesh-female | helmet-1-arch.png | helmet / 1 | 1,5,9 | 247235 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Cultist.mesh-female | helmet-2-crown.png | helmet / 2 | 2,6,10 | 178782 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Cultist.mesh-female | helmet-3-horned.png | helmet / 3 | 3,7,11 | 159634 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Cultist.mesh-female | helmet-4-ridged-clean.png | helmet / 4 | 4,8,12 | 150324 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
| Cultist.mesh-female | outfit-1-robe-build0.png | outfit / 1 | 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 | 742384 | inspected-native-and-50x100 | Only build, outfit, hair, helmet, scar and neck slots are registered; no eyes, nose, mouth or chin options exist. Additional offline views omit the helmet for hair inspection only. |
