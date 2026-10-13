# Portrait plan completion review

Checked against sections 3 and 7 of the current portrait task.
Run `python tools/portraits/check_plan_completion.py` to repeat the checks.

| Requirement | Evidence and result |
|---|---|
| 3.1 Canvas height | All 34 canonical bases are 887x1774. The two height fixes preserve the first 1773 rows and repeat only the final row; backups and SHA-256 proof remain in `neutral-bases/canvas-height-fixes.json`. |
| 3.2 Missing clothing | Thirty selected overlays added for the thirty previously unclothed bases; ten existing selections reused. Every manifest has an outfit. Raw sources, correction prompts and provenance remain in `generated-outfits/` and the inventory. |
| 3.3 Placement | 688 options delivered in 34 TAB-separated manifests; registered sizes, alpha masks, draw order and recipe hashes recorded. Original isolated sources remain unchanged. |
| 3.4 Findings | All 104 recorded concrete findings are closed, including the original thirteen and the last six Lizardman findings; each closure records its final evidence in `feature-review.json` and `.md`. |
| 3.5 Composite checks | Each of the 688 options appears in the recorded native combinations and twelve 50x100 previews per base; coverage and final accepted file hashes are retained. Native face/body panels and small sheets are committed; reproducible full-size diagnostic PNGs remain local. |
| 3.6 Source hashes | Passed for 34 costume previews, 632 isolated feature sources, 34 neutral bases including the two documented fixes, 16 helmets and 40 selected clothing sources. |
| 7 Tint regions | Actual neutral-base species-colour pixels measured for all 34 bases; normalized skin rectangles and source hashes recorded in `base-tint-measurements.json` and the TAB-separated tint configuration. The visually inspected blank bases have no painted hair, eyes or beard; these regions are recorded as absent rather than invented from feature slots. No image generation was used. |

Inventory, CREDITS and asset/tool documentation accompany the delivery.
Runtime selection is a separate task; no runtime version change is required.
The user's separate gambeson test and reproducible diagnostic copies stay local.
