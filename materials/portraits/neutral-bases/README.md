# Neutral Dungeonbook bases

Owner-approved requirement: naked smooth neutral mannequin bodies without clothing, equipment, facial features or visible sexual characteristics; gender suffixes identify catalog entries only.

33 of 34 bases generated, visually inspected and saved. All creature pairs are saved except Knight: the female catalog entry is saved, the male entry remains blocked.

Dimension check (2026-10-04): 31 outputs are exactly 887x1774; `Kobold.mesh.png` and `RunelordDwarf.mesh-female.png` are 887x1773 and require a one-pixel canvas correction before technical acceptance. No resizing or pixel manipulation was performed. Thus 33 files are saved, but only 31 meet the required dimensions.

The image-service canvas-only correction attempt for Kobold male returned 953x1651 and changed the composition; it was rejected and did not replace the saved base. Generator output: `C:/Users/mario/.codex/generated_images/01a0f765-dd88-7a90-b3e3-6becedd2dd5f/exec-76036d7c-1ad2-4c62-a241-58e4bd94586f.png`. Exact prompt: "Correct ONLY the canvas dimensions of this existing neutral purple game mannequin illustration: input is 887 pixels wide by 1773 pixels tall; output MUST be exactly 887 wide by 1774 tall. Add one matching row at the bottom, preserve the image composition, featureless head, smooth torso, colors, background and brushwork. No new features, clothing, text or accessories."

Adventurer female's earlier input moderation block (category `other`, request `2ba69dd5-d61e-4941-af1a-dcc09291b434`) is resolved: following the owner's explicit dummy clarification, a genderless featureless game dummy was generated from the accepted Adventurer dummy reference and saved.

The Monk female usage-limit block has resolved; all remaining pairs through Cultist were processed. Current blocker: Knight male's correction was rejected by output moderation (category sexual, request ccda48d7-bac0-4820-b4d5-77577858bbef). Its first candidate introduced legs and changed the arm pose, so it was rejected and saved only as `work/neutral-base-rejected/Knight.mesh-added-legs.png`; it does not count. Exact accepted prompts, source paths and the blocked correction are in `generation-records.json`. Do not retry the rejected request or bypass moderation.

Knight female is saved with a placement finding: generated forearms and framing differ from the original reference; compatibility with existing overlays is unverified. Reference angles and poses were guided by input images and visually inspected, not numerically measured.

An additional owner-requested alternative explicitly depicting an inanimate, genderless upper-body display mannequin was also rejected by output moderation (sexual; request 39a45b8e-deb9-4a4f-8bef-eedde81bf136); no image was returned. Its exact prompt and references are preserved in `knight-final-attempt.json`. Saved progress remains 33/34.

The original clothed Kobold candidate was rejected and moved to `work/neutral-base-rejected/Kobold.mesh-clothed.png`; it does not count. The older `prompts.md` describes that rejected candidate only; accepted asset prompts are in `generation-records.json`.

Existing preview portraits and isolated features were not edited during neutral-base generation. Owner instruction on 2026-10-04 authorizes assessment with 33 saved bases and defers Knight male. Initial overview inspection covers 623 existing options; exact per-file findings and evidence paths are in [feature-review.md](feature-review.md) and [feature-review.json](feature-review.json). This pass uses old placement coordinates, so anatomical registration, occlusion masking and native-size plus 50x100 combined acceptance remain open. No feature regeneration has been established or authorized; a final regeneration count is not yet known.

Source/licence: generated for this project from its existing portrait references, GPLv3+, credited in CREDITS; generator outputs remain preserved at their original paths.
