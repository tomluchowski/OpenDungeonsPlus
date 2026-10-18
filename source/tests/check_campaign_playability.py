"""Static campaign structure and necessary winning-path checks; never starts a game.

This is a necessary-condition check, not a proof of combat balance or script solvability.
Sparse maps default to full dirt, as GameMap::createNewMap does. Four-way routes
allow dry ground and removable dirt/gold; full gems, rock, water, lava and foreign
claimed walls block. Existing bridges are traversable. Only terrain openings in
once triggers gated solely by elapsed time are included; other script conditions
are never presumed satisfied.
"""
import argparse
from collections import deque
import hashlib
import math
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
GOALS = {"KillAllEnemies", "ProtectDungeonTemple", "ProtectCreature", "ClaimNTiles", "MineNGold"}
REQUIRED = ("Info", "Seats", "Goals", "Tiles", "Rooms", "Traps", "Lights",
            "CreatureDefinitions", "EquipmentDefinitions", "Creatures", "Spells",
            "CraftedTraps", "SkillEntity", "GiftBoxEntity", "Missiles", "TreasuryObject", "Chickens")
# Reviewed goal-bearing sentences (one-based), with their actual level checks.
BRIEFING_MATCH = {
    "Mossgate": {3: "no: mining and meadow claiming are not winning conditions", 4: "no: reeve elimination is checked; breaking the watch post separately is not"},
    "Brackenford": {3: "no: beds, hatchery, grove and gathering are not winning conditions", 4: "yes: KillAllEnemies and the warden seat elimination trigger"},
    "Coldwell": {3: "no: plaza ownership and mined gold are not winning conditions", 4: "no: Cole elimination is checked; breaking the gate separately is not"},
    "Tinmoor": {2: "no: enemy elimination is checked; seams and both posts are not independent goals", 3: "no: optional stone discovery is not required to win"},
    "BoulderCourse": {3: "yes: courseDone checks all four cups; the timeout trigger limits the trial"},
    "Ravensledge": {3: "no: bridgeheads and shelf progression are not independent goals", 4: "yes: KillAllEnemies includes Vane"},
    "Ashcombe": {3: "no: Ashby elimination/escape is checked; crossing the ashes is not an independent goal"},
    "Greywater": {3: "no: ProtectDungeonTemple checks the heart; mining is not a winning condition", 4: "yes: KillAllEnemies includes Gray"},
    "HollinFen": {2: "no: enemy elimination and heart protection are checked; building and breaking the ward are not independent goals", 3: "no: optional stone discovery is not required to win"},
    "CrowshotGallery": {3: "no: galleryClear checks the flock; possession is not a separate winning condition"},
    "Ironbridge": {3: "no: building and bridge order are not independent goals", 4: "yes: KillAllEnemies includes the rival temple"},
    "Lanternhill": {2: "no: mining and digging the plugs are not independent goals", 3: "yes: KillAllEnemies includes the northern rival temple", 4: "no: optional stone discovery is not required to win"},
    "VarnsCrossing": {3: "no: heart protection and enemy temples are checked; quarter and fortress ownership are not independent goals"},
    "Thornreach": {3: "no: enemy temples are checked; clearing ownership and maze progression are not independent goals"},
    "Highcairn": {2: "no: heart protection is checked; building and ascent are not independent goals", 3: "no: enemy elimination is checked; carrying the optional stone is not required to win"},
    "SkittleCavern": {3: "no: laneCleared checks opened lanes and no remaining guards, not whether boulders killed them"},
    "Stormhaven": {3: "no: enemy temples are checked; stone-span control is not an independent goal"},
    "Cinderhollow": {3: "no: ProtectDungeonTemple checks survival; mining is not required", 4: "no: enemy temple elimination is checked; bridge crossing is not an independent goal"},
    "Saltmere": {2: "no: mining and the travel route are not independent goals", 3: "yes: ProtectDungeonTemple", 4: "yes: KillAllEnemies includes Seward"},
    "Dunmarrow": {3: "no: enemy elimination/escape is checked; the route used is not a winning condition"},
    "Goldspire": {2: "no: building before attacking is not a winning condition", 3: "no: enemy elimination is checked; the ordered low-town/link route is not an independent goal"},
    "Mirewatch": {2: "no: digging/building order is not a winning condition", 3: "no: all tower ownership and both approaches are not independent winning goals", 4: "yes: KillAllEnemies includes the watcher"},
    "Bellwick": {3: "no: enemy temple elimination is checked; holding until the gate event is not an independent goal"},
    "Wolfscar": {3: "no: enemy temples and own-heart protection are checked; wolf-den ownership and the route are not independent goals"},
    "TwistingHalls": {3: "no: hallReached checks region presence; it does not independently require possession"},
    "Silverdeep": {2: "no: mining/building are not independent goals", 3: "no: enemy temples are checked; building exactly three wooden spans is not a winning condition"},
    "Ebonrook": {2: "no: mining and feeding are not winning conditions", 3: "no: enemy elimination is checked; the named route is not an independent goal"},
    "Wraithwood": {3: "yes: ProtectDungeonTemple and KillAllEnemies include both rival temples"},
    "Kingsfall": {2: "no: prince elimination is checked; crossing the watched spans is not independent", 3: "no: collectWin checks all champions and twelve creatures, but courtTaken also wins without all three champions and neither checks their location at the heart"},
    "SwarmNight": {3: "yes: dawn at 780 seconds with ProtectDungeonTemple", 4: "yes: dawn invokes keepminion before win; no Heartstone reward is granted"},
    "HollowmarkCitadel": {3: "no: gate routes and treasury use are not independent winning goals", 4: "no: kingDead checks Castellan's death, not throne ownership"},
}
# Only existing triggers that actually satisfy each objective may change its state.
# None is an unresolved mapping, deliberately reported as an error instead of inferred at victory.
OBJECTIVE_TRIGGERS = {
    "Ashcombe": [("wardenDown",), ("holdBreached",), ("wardenDown",)],
    "Bellwick": [("keeperHeartDown",), None, ("naveGatesKills", "naveGatesLate"), ("transept",)],
    "BoulderCourse": [("courseDone",)],
    "Brackenford": [("wardenGone",), ("bedsReady",), ("larderReady",), ("groveClaimed",), ("villageOpen",), ("wardenGone",)],
    "Campaign1": [None], "Campaign2": [None], "Campaign3": [None],
    "Cinderhollow": [("rivalHeartDown", "rivalHeartFalls"), None, None, None, ("rivalHeartDown", "rivalHeartFalls")],
    "Coldwell": [("wardenSlain",), ("plazaReached",), ("wellEmptied",), ("courtBreached",)],
    "CrowshotGallery": [("galleryClear",)],
    "Dunmarrow": [("wardenCaught",), ("barrowsEntered",), None],
    "Ebonrook": [("wardenSlain",), ("crossed",), None, ("scoutPossessed",), ("wardenSlain",)],
    "Goldspire": [("wardenSlain",), None, ("lowTown",), ("onRing",), ("wardenSlain",)],
    "Greywater": [("wardenFalls",), ("firstCoin",), ("wardenFalls",), ("siegeSeen",)],
    "Highcairn": [("wardenSlain",), None, ("wardenSlain",), ("wardenSlain",), ("stoneHome", "stoneHome2"), ("holdSealBreaks",)],
    "HollinFen": [("wardenSlain",), ("wardenSlain",), ("wardEntered",), ("wardenSlain",)],
    "HollowmarkCitadel": [("kingDead",), None, ("kingDead",)],
    "Ironbridge": [("rivalFalls", "rivalHeartDown"), None, ("yardReached",), None],
    "Kingsfall": [("collectWin",), ("courtTaken", "collectWin"), None],
    "Lanternhill": [("rivalHeartDown",), None, ("rivalHeartDown",)],
    "Mirewatch": [("watcherSlain",), None, None, ("scoutTowerC",), ("watcherSlain",)],
    "Mossgate": [("reeveFalls",), ("firstGold",), ("meadowOurs",), ("hallOpened",), ("reeveFalls",)],
    "Ravensledge": [("captainDown",), ("shelfHeld",), ("shelfHeld",), ("yardEntered",), None],
    "Saltmere": [("admiralFalls",), ("firstSalt",), ("admiralFalls",), ("flatHeld", "portGatesTimeout"), ("admiralFalls",)],
    "Silverdeep": [("silverWonGoal",), None, None],
    "SkittleCavern": [("laneCleared",)],
    "Stormhaven": [("stormEndsGoal",), None, None],
    "SwarmNight": [("dawn",), ("dawn",)],
    "Thornreach": [("keepersGoneGoal",), None, ("keepersGoneGoal",)],
    "Tinmoor": [("wardenDown",), ("seamsFilled",), ("postACleared",), None, ("scoutSeams",)],
    "TwistingHalls": [("hallReached",)],
    "VarnsCrossing": [("crossingHeldGoal",), ("crossingHeldGoal",), None],
    "Wolfscar": [("huntersGone", "huntersGoneGoal"), ("southCrossed",), ("huntersGoneGoal",)],
    "Wraithwood": [("bothFallGoal",), ("bothFallGoal",), ("bothFallGoal",)],
}

# Individually reviewed non-matches: candidate triggers, missing requirement and proposed decision.
# Candidates are never treated as completions without an explicit design decision.
OBJECTIVE_REVIEW = {
    "bellwick-o02": ("naveGatesKills, naveGatesLate", "Neither checks the end of the faithful attacks; later waves continue.", "Use the existing gate-opening milestone only if holding the nave is meant to end there; should that milestone complete this text?"),
    "campaign1-o01": ("none", "All existing triggers are time hints, not goal completion.", "Add a presentation-only completion trigger using the existing KillAllEnemies goal; may this additional trigger be added without altering any existing condition?"),
    "campaign2-o01": ("none", "All existing triggers are time hints, not goal completion.", "Add a presentation-only completion trigger using the existing KillAllEnemies goal; may this additional trigger be added without altering any existing condition?"),
    "campaign3-o01": ("none", "All existing triggers are time hints, not goal completion.", "Add a presentation-only completion trigger using the existing KillAllEnemies goal; may this additional trigger be added without altering any existing condition?"),
    "cinderhollow-o02": ("mined1, ashWell", "Global mined gold or exhausted GoldHollow does not check both named seams before the first wave.", "Prefer ashWell as the existing mining milestone only if the two-seam deadline is descriptive; may it complete this objective?"),
    "cinderhollow-o03": ("waveThree", "The trigger spawns the third wave; it does not check any wave's defeat or its position at the heart.", "Keep this objective open until a specific wave-defeat condition is authorized; may a new presentation trigger check those existing wave creatures?"),
    "cinderhollow-o04": ("craterSeen", "Entering RivalCrater does not explicitly require dawn or crossing the lava bridge.", "Use craterSeen as the crossing milestone if arrival is sufficient; should it complete this objective?"),
    "dunmarrow-o03": ("campEntered", "Entering Camp does not check that the wagon departure timer has not expired.", "Use campEntered only if arrival alone is sufficient; must the existing wagon deadline also be required?"),
    "ebonrook-o03": ("firstCoin", "Mining 300 gold does not check feeding the army from the glades.", "Use firstCoin for the mining part only; should feeding remain advice rather than a separately checked requirement?"),
    "goldspire-o02": ("firstCoin", "Mining 500 gold does not check building an army; the text specifies no army size.", "Use firstCoin only if building a force is advice; should this objective complete at the existing mining milestone?"),
    "highcairn-o02": ("terrace1, terrace2, terrace3, terrace4", "Each trigger checks one terrace independently; none checks all terraces in order.", "Use terrace4 as the final climbing milestone if the order is descriptive; should reaching it complete this objective?"),
    "hollowmarkcitadel-o02": ("gate3Open, thirdGate, throneHall", "Tower visits open gates; no trigger checks three conquered courts and fallen towers together.", "Use throneHall as the end of the court route if arrival is sufficient; should it complete the objective?"),
    "ironbridge-o02": ("gateReached", "Entering RivalGate does not explicitly check all three rift crossings.", "Use gateReached as the last crossing milestone if route arrival is sufficient; should it complete this objective?"),
    "ironbridge-o04": ("gateReached, keepReached", "RivalGate is the gate approach; OreDock is outside RivalGate and RivalKeep, so neither proves breaking through the gate.", "Do not use keepReached; should gateReached count as completion, or is a new inside-hold completion check required?"),
    "kingsfall-o03": ("courtTaken, collectWin", "Both count twelve creatures anywhere, not at the player's heart.", "Keep the location requirement unless explicitly relaxed; may the existing whole-map creature count complete this objective?"),
    "lanternhill-o02": ("ringEntered", "Entering Ring does not check digging the earth plugs or opening the full ring road.", "Use ringEntered only if entering the road is sufficient; should it complete the objective?"),
    "mirewatch-o02": ("firstCoin, hollowHeld", "Mining 400 gold or claiming 40 percent of Hollow does not check building before marching.", "Use hollowHeld as the preparation milestone only if building and order are advice; should it complete this objective?"),
    "mirewatch-o03": ("bothHeld, towerA, towerB, towerC, towerD, towerE, towerF, towerG, wardenSeen", "Two held flags or individual region visits do not check taking all towers one by one.", "Use bothHeld only if the two seal towers are the intended requirement; should those existing flags complete this text?"),
    "ravensledge-o05": ("yardEntered", "Entering Yard does not check breaking the inner keep gate.", "Do not complete it on the yard approach; may an additional inside-keep presentation trigger be added?"),
    "silverdeep-o02": ("spanA, spanB, spanC, southBank", "These are individual bank visits, with duplicate SouthBank conditions, not a combined three-bridge check.", "Prefer spanC as the last span milestone if arrival is sufficient; should that existing trigger complete this objective?"),
    "silverdeep-o03": ("firstSilver", "The trigger checks 400 globally mined gold, not its silver-seam location or mining before other actions.", "Use firstSilver as the existing silver-mining milestone if the location and order are descriptive; should it complete the objective?"),
    "stormhaven-o02": ("spanSeen, northSpan", "Each checks one span region; neither checks crossing the inlet by the stone spans.", "Use northSpan as the final crossing milestone if arrival is sufficient; should it complete this objective?"),
    "stormhaven-o03": ("tideTurns, tideTurns2, stormEndsGoal", "Timed alliances are actions and the win goal checks enemies; none checks preventing the keepers from joining.", "Keep the prevention requirement open; may a new presentation check observe keeper defeat before the existing alliance timer?"),
    "thornreach-o02": ("hedgeA, hedgeB, clearingSeen", "Region visits do not check digging and burning a path through the maze.", "Use clearingSeen as the route-end milestone if the travel method is advice; should it complete this objective?"),
    "tinmoor-o04": ("postBEntered", "Entering PostB does not check destroying the upper watch post.", "Do not use entry as destruction; may a presentation-only check for the existing upper-post enemies be added?"),
    "varnscrossing-o03": ("quarterHeld, keepEntered, crossingHeldGoal", "These independently check claiming, entering or all enemies; none checks claiming the quarter before taking the fortress.", "Use crossingHeldGoal if ordering is advice and defeating the fortress is sufficient; should that existing goal trigger complete this text?"),
}

# Fixed fingerprints of the reviewed existing conditions; never recalculated as expectations.
OBJECTIVE_CONDITIONS = {'Ashcombe/holdBreached': 'bae0a393bc01e08ad3ae0b95d56ee971dc346396388423a3d0c019f97ff58aaf',
 'Ashcombe/wardenDown': '9099b47ed59d017b8ca6cede9c74e2bbfe5d91a9ee0d4ee005dbbf423926927f',
 'Ashcombe/wardenEscaped': '56e37df8874d71b952e4aff8e246d5568afc363fa678dde537ce5b53d3bb0604',
 'Bellwick/keeperHeartDown': '2a99324afebb946debdf455a762b0a3397ab53239aff6aa5c15a82d7ca0af1e6',
 'Bellwick/naveGatesKills': 'e796048b8442103fde2ec73588b9d953975383db05e5bdf9017869aff366deb6',
 'Bellwick/naveGatesLate': '19a266ae96382f14cdc2629ae7b76df0f1c0af1308564afef74ad5f564e4da11',
 'Bellwick/transept': 'b8dd598cef221a526a3e14566cc0c21315e63d25e9dcf7159ffc5ae473d12f9e',
 'BoulderCourse/courseDone': '1568e1b864d5e350c0ccf8d87fdba7f79a0d3c1bfe09b6b32ba50ea38f830ece',
 'Brackenford/bedsReady': 'a753667474f8af0a9fefaeb6a9bd6a426c94e9dbf947f3654dafbfb333427e1d',
 'Brackenford/groveClaimed': '4876c136061a43ce924a55baa9dc20f30a00ee0f1b9cb9598ce1e5a9c1c8a031',
 'Brackenford/larderReady': '562b2b930847e8f1fb6f9fe93dc680903ec501b0198b5d6696cce209c37aa637',
 'Brackenford/villageOpen': 'f979ec5bfc4d35b4bb42bccfdf08eb59d408b8688e3fc3b31e8ec7d10506f5ef',
 'Brackenford/wardenGone': '64247d08b33c60aecb9970aaa61cad974015e18496b97341fb463acf439130ca',
 'Cinderhollow/rivalHeartDown': 'b490f8085c95c318f0aee37b0bcb7f903d47f272d0bedb3114676bcf5146fc3b',
 'Cinderhollow/rivalHeartFalls': 'e39927741e06e2f5d2a2863a979535b542e7774e671af7d7838cc8153b14097d',
 'Coldwell/courtBreached': '44ebb7a1fa5c16d2bc4cf4cfd4e82ec1ea98501c6839fb6190f50f6b51175f33',
 'Coldwell/plazaReached': 'b3c03fd9a567992624fe4dfaffb2bd550a0585faddb0875c6d59676e3472c48e',
 'Coldwell/wardenSlain': '64247d08b33c60aecb9970aaa61cad974015e18496b97341fb463acf439130ca',
 'Coldwell/wellEmptied': '6591ba5aceeb629439c2b37862d24cadf5846841e8477f759af3d91288c59ad1',
 'CrowshotGallery/galleryClear': '70a36becadb693911641ad3bf0ea8bfcb986d9f6d515c0e1d89ee1ca6a9944e0',
 'Dunmarrow/barrowsEntered': '085380acca4d034f15c91a10680450c59c2d33c9865fef70b3a793273eb2e149',
 'Dunmarrow/wardenCaught': 'f50e1bd028271d6808509e83e18a5ecf8d30c344d57b3b49c68b9e29e0734865',
 'Dunmarrow/wardenEscaped': 'e624c6cc8fd2849cdc16f3d18df11a92436d2dcd0a89b4dd72a8e2c1c6a27b37',
 'Ebonrook/crossed': 'f074e0ce94d380e77fed879ecdb320a5953d48adefdf66c63e4ec876a6998358',
 'Ebonrook/scoutPossessed': '3b6089fe0ea54d0fb030187fcc8d905f38edcaaa1c6757938aaa4d0caced81cb',
 'Ebonrook/wardenSlain': '64247d08b33c60aecb9970aaa61cad974015e18496b97341fb463acf439130ca',
 'Goldspire/lowTown': 'b0850f7040d983080895d0dde679743b6d2296c36ef4f248e7c3de5ec3c76dec',
 'Goldspire/onRing': '7f80618dff9ef40c3c8ba326475dea955da625c4cec0f63e159540e0716d0160',
 'Goldspire/wardenSlain': '64247d08b33c60aecb9970aaa61cad974015e18496b97341fb463acf439130ca',
 'Greywater/firstCoin': '6591ba5aceeb629439c2b37862d24cadf5846841e8477f759af3d91288c59ad1',
 'Greywater/siegeSeen': '3abd24d1062606a705b9a1eccbf238205629f9896f24ea13ce4ccd40d444210f',
 'Greywater/wardenFalls': '3a263d0a43819eb0c082efacc0eccd1096b304cddf3227642cd0db922bb597ca',
 'Highcairn/holdSealBreaks': '8d98fbb5f48f929b4e4e9eca60cfdb982bfd32ca5d430f8918bdf3de7041f9fb',
 'Highcairn/stoneHome': 'bf6c9dad942cb5d2f04f994d6355cd9af755cedc6c9e7d1122d8205843db3c19',
 'Highcairn/stoneHome2': '14e787f8a1aa9a89f3e765383f0c68e97714f9cc40f58056110e73e77baff80c',
 'Highcairn/wardenSlain': '64247d08b33c60aecb9970aaa61cad974015e18496b97341fb463acf439130ca',
 'HollinFen/wardEntered': '5104e92aff9354bc899db8c3294c4399f167ae7405b47ae4c7c15f58990f8c4a',
 'HollinFen/wardenSlain': '64247d08b33c60aecb9970aaa61cad974015e18496b97341fb463acf439130ca',
 'HollowmarkCitadel/kingDead': '3ae8027124d07835bcc3ccbe6538e8c04c3e7c11a1a389cd2ad1b32e4bb37b4a',
 'Ironbridge/keepReached': '52168c7a587567af570b8b1ad537643c3516be73a46144e3986c89c26559f148',
 'Ironbridge/rivalFalls': '05b5e72367cf0376729d09cfb8ed22f46c191cc0c0b936db820e8cbf975aa7d5',
 'Ironbridge/rivalHeartDown': '2a99324afebb946debdf455a762b0a3397ab53239aff6aa5c15a82d7ca0af1e6',
 'Ironbridge/yardReached': '9baa954870ffac33927dc1717bf9910ed278b7f82a585cb92216a909f587fba6',
 'Kingsfall/collectWin': '248ae73a6216d7d671e187040748d230b66ca878354543dcd4139d5ead708d53',
 'Kingsfall/courtTaken': '71e02274670b0b0e5cf5cf93d4ec3f43036487e9d3972f95314d7d52de10cb1d',
 'Lanternhill/rivalHeartDown': '2a99324afebb946debdf455a762b0a3397ab53239aff6aa5c15a82d7ca0af1e6',
 'Mirewatch/scoutTowerC': '47716bb4d4cf382a3c8361ed7ea924e83aec55ebe50a73512cced36d5cfa45b6',
 'Mirewatch/watcherSlain': '64247d08b33c60aecb9970aaa61cad974015e18496b97341fb463acf439130ca',
 'Mossgate/firstGold': '4bb67357526e67e6cebc76c5f68d3724d44496fa4fa0af3b661369e6a7822232',
 'Mossgate/hallOpened': '835d13b12817b58bb8a9342ce8e44ecc3fbc12ad6845c34392fefbf960ef93fc',
 'Mossgate/meadowOurs': 'dc24a3c61e63b2361437ecff16031829c4603c9cf3914b514de872c70ddb52e2',
 'Mossgate/reeveFalls': '64247d08b33c60aecb9970aaa61cad974015e18496b97341fb463acf439130ca',
 'Ravensledge/captainDown': '64247d08b33c60aecb9970aaa61cad974015e18496b97341fb463acf439130ca',
 'Ravensledge/shelfHeld': 'f3b5f12fb09cd6590ec8fe5d13f74cd7956b27e0a3bb9524b0a49ad2782cbef1',
 'Ravensledge/yardEntered': '9f57851e1a5a3e3b77dca2af5a58970e607f4f71b73268c2872e00d8b9eac1db',
 'Saltmere/admiralFalls': 'c96e98ffdf3e6804c1011c44eb491c5380b0511c0097e4208292b74128f88dbd',
 'Saltmere/firstSalt': '44a33805891e8bde4816432f6122e61047c42e6a3206e750e684f084e0a8d625',
 'Saltmere/flatHeld': '013d7fc84c5e7c7c651d50bf9d20ed6116158743113badc7883dbbf4de2e0ce1',
 'Saltmere/portGatesTimeout': '461dcfa3ab8be3d790feeaaf6488d364908584efbd2cb9c4a0f0d4bebf5dc769',
 'Silverdeep/silverWonGoal': '2a99324afebb946debdf455a762b0a3397ab53239aff6aa5c15a82d7ca0af1e6',
 'SkittleCavern/laneCleared': '34ca017e8ab755fe28f0a2da2aa7a24fc0fb75e59cad21f923a1aa3631fbf231',
 'Stormhaven/stormEndsGoal': '2a99324afebb946debdf455a762b0a3397ab53239aff6aa5c15a82d7ca0af1e6',
 'SwarmNight/dawn': '2335939255b4fd381eca7f23b78c9b0d57725213bced5336b88fd8d105dfaf0e',
 'Thornreach/keepersGoneGoal': '2a99324afebb946debdf455a762b0a3397ab53239aff6aa5c15a82d7ca0af1e6',
 'Tinmoor/postACleared': 'dac3217efa10c0755d25613c88742aee86e44f9f96087c3fe996ec4d3e389fad',
 'Tinmoor/scoutSeams': 'b0046d533f6e970dbd28d295d8c86f436eafb42379ac010acd97b4cbe7587ac2',
 'Tinmoor/seamsFilled': '9cf34e5e445fe8b72c19705c624d55970832076f065272e785544ee60a0d46ea',
 'Tinmoor/wardenDown': '64247d08b33c60aecb9970aaa61cad974015e18496b97341fb463acf439130ca',
 'TwistingHalls/hallReached': '9e2ac4d840cc4b2cd6b37ea9260f16250976ae7ea3e0d5ae9563c5387778f0f9',
 'VarnsCrossing/crossingHeldGoal': '2a99324afebb946debdf455a762b0a3397ab53239aff6aa5c15a82d7ca0af1e6',
 'Wolfscar/huntersGone': '817957baa91166588c3cf255cb0a0c3b1337b587953edcab61f720389de35680',
 'Wolfscar/huntersGoneGoal': '2a99324afebb946debdf455a762b0a3397ab53239aff6aa5c15a82d7ca0af1e6',
 'Wolfscar/southCrossed': 'a8f87a6499f3f4999683baba47d5c312ff9e89caf3a9af1f46bd762a6510941b',
 'Wraithwood/bothFallGoal': '2a99324afebb946debdf455a762b0a3397ab53239aff6aa5c15a82d7ca0af1e6'}

# Return nonempty source lines with comments removed and original line numbers retained.
def lines(text):
    return [(n, line.split("#", 1)[0].strip()) for n, line in enumerate(text.splitlines(), 1)
            if line.split("#", 1)[0].strip()]


# Extract exactly one named section without consuming unrelated nested blocks.
def section(text, name):
    depth, start, matches = 0, None, []
    for match in re.finditer(r"^\[(/?)(\w+)\][ \t]*$", text, re.M):
        closing, tag = match.groups()
        if closing:
            depth -= 1
            if depth == 0 and tag == name and start is not None:
                matches.append(text[start:match.start()])
                start = None
        else:
            if depth == 0 and tag == name:
                start = match.end()
            depth += 1
    if len(matches) != 1:
        raise ValueError("expected one [" + name + "] section")
    return matches[0]


# Validate balanced section nesting and the fixed loading order used by MapHandler.
def validate_sections(text):
    stack, top = [], []
    for number, line in lines(text):
        if not line.startswith("["):
            continue
        match = re.fullmatch(r"\[(/?)(\w+)\]", line)
        if not match:
            raise ValueError("line %d: malformed section tag" % number)
        closing, name = match.groups()
        if closing:
            if not stack or stack.pop() != name:
                raise ValueError("line %d: mismatched section %s" % (number, line))
        else:
            if not stack:
                top.append(name)
            stack.append(name)
    if stack:
        raise ValueError("unclosed sections: " + ", ".join(stack))
    if tuple(top[:len(REQUIRED)]) != REQUIRED:
        raise ValueError("sections do not follow MapHandler loading order")
    if lines(text)[0][1].split()[0] not in ("OpenDungeons_Version:0.7.1", "OpenDungeons_Version:0.7.2"):
        raise ValueError("unsupported level version")


# Load seats, sparse tile overrides, room footprints and creatures using the file contract.
def parse_level(text):
    validate_sections(text)
    seats = {0: {"teamId": "0", "player": "Inactive"}}
    for block in re.findall(r"\[Seat\](.*?)\[/Seat\]", section(text, "Seats"), re.S):
        fields = dict(line.split(None, 1) for _, line in lines(block) if not line.startswith("[") and len(line.split(None, 1)) == 2)
        required = ("seatId", "teamId", "player", "faction", "startingX", "startingY")
        if any(key not in fields for key in required):
            raise ValueError("seat lacks required fields")
        seat = int(fields["seatId"])
        if seat <= 0 or seat in seats or any(int(team) <= 0 for team in fields["teamId"].split("/")):
            raise ValueError("duplicate/invalid seat or team")
        seats[seat] = fields
    humans = [seat for seat, fields in seats.items() if fields["player"] == "Human"]
    if len(humans) != 1:
        raise ValueError("expected exactly one human seat")
    tile_lines = lines(section(text, "Tiles"))
    width, height = int(tile_lines[0][1]), int(tile_lines[1][1])
    if width <= 0 or height <= 0:
        raise ValueError("invalid map dimensions")
    tiles = {(x, y): (1, 100.0, None) for x in range(width) for y in range(height)}
    seen = set()
    for number, line in tile_lines[2:]:
        values = line.split()
        if len(values) not in (4, 5):
            raise ValueError("tile requires x y type fullness [seat]")
        x, y, kind = map(int, values[:3])
        fullness = float(values[3])
        if (x, y) not in tiles or (x, y) in seen or kind not in range(1, 8) or not math.isfinite(fullness) or not 0 <= fullness <= 100:
            raise ValueError("invalid or duplicate tile at %d,%d" % (x, y))
        if kind in (4, 5, 7):
            fullness = 0.0
        owner = int(values[4]) if len(values) == 5 and (kind in (1, 7) or kind == 2 and fullness == 0) else None
        if owner is not None and owner not in seats:
            raise ValueError("tile references unknown seat")
        tiles[x, y] = kind, fullness, owner
        seen.add((x, y))
    rooms = []
    for block in re.findall(r"\[Room\](.*?)\[/Room\]", section(text, "Rooms"), re.S):
        data = lines(block)
        kind, name, seat, count = data[0][1].split()
        kind, seat, count = int(kind), int(seat), int(count)
        if seat not in seats or kind not in range(1, 19) or count < 1 or len(data) < count + 1:
            raise ValueError("invalid room " + name)
        footprint = [tuple(map(int, row.split())) for _, row in data[1:count + 1]]
        if any(tile not in tiles for tile in footprint) or len(set(footprint)) != count:
            raise ValueError("invalid room footprint " + name)
        if kind == 4 and len(data) != count + 2:
            raise ValueError("portal lacks claimed value and capacity " + name)
        rooms.append((kind, name, seat, footprint))
    creatures = []
    for number, line in lines(section(text, "Creatures")):
        fields = line.split()
        if len(fields) < 18:
            raise ValueError("short creature row")
        seat, name = int(fields[0]), fields[1]
        tile = tuple(math.floor(float(value) + 0.5) for value in fields[3:5])
        if seat not in seats or tile not in tiles:
            raise ValueError("creature seat/position invalid: " + name)
        creatures.append((name, seat, tile))
    goals = []
    for number, line in lines(section(text, "Goals")):
        if line.startswith(("+", "-")):
            if not re.fullmatch(r"[+-]\s+\d+", line):
                raise ValueError("invalid subgoal marker")
            continue
        name, argument = line.split(None, 1)
        if name not in GOALS:
            raise ValueError("unknown goal " + name)
        if name in ("ClaimNTiles", "MineNGold") and int(argument) <= 0:
            raise ValueError("goal count must be positive")
        goals.append((name, argument))
    scripts = [(n, line.split(None, 3)) for n, line in lines(text)
               if line.startswith(("Action", "Cond"))]
    human = humans[0]
    start = int(seats[human]["startingX"]), int(seats[human]["startingY"])
    if start not in tiles:
        raise ValueError("player start is outside map")
    return dict(seats=seats, human=human, start=start, tiles=tiles, rooms=rooms,
                creatures=creatures, goals=goals, scripts=scripts, text=text)


# Include only guaranteed elapsed-time openings, preserving conditional gates as blockers.
def timed_openings(level):
    tiles = dict(level["tiles"])
    for block in re.findall(r"\[Trigger\](.*?)\[/Trigger\]", level["text"], re.S):
        rows = [row.split() for _, row in lines(block)]
        conditions = [row for row in rows if row[0] == "Cond"]
        if ["Mode", "once"] not in rows or not conditions or any(
                len(row) != 3 or row[1] != "time" or not row[2].isdigit()
                for row in conditions):
            continue
        for row in rows:
            if row[:2] != ["Action", "terrain"] or len(row) != 7 or row[-1] not in ("path", "claimed"):
                continue
            x1, y1, x2, y2 = map(int, row[2:6])
            for x in range(min(x1, x2), max(x1, x2) + 1):
                for y in range(min(y1, y2), max(y1, y2) + 1):
                    if (x, y) in tiles:
                        tiles[x, y] = (1, 0.0, level["human"] if row[-1] == "claimed" else None)
    return dict(level, tiles=tiles)


# Yield only cardinal neighbours, matching the tile navigation graph.
def neighbours(tile):
    x, y = tile
    return ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1))


# Test removable ground without treating inexhaustible gem seams as removable walls.
def passable(level, tile):
    kind, fullness, owner = level["tiles"][tile]
    if any(kind_room in (12, 13) and tile in footprint for kind_room, _, _, footprint in level["rooms"]):
        return True
    if fullness == 0:
        return kind not in (4, 5)
    if kind not in (1, 2):
        return False
    return owner is None or owner == level["human"]


# Flood fill the player's potential dry walk/dig area and record its blocking frontier.
def reachable(level):
    visited, blocked = set(), set()
    queue = deque([level["start"]])
    while queue:
        tile = queue.popleft()
        if tile in visited or tile not in level["tiles"]:
            continue
        if not passable(level, tile):
            blocked.add(tile)
            continue
        visited.add(tile)
        queue.extend(neighbours(tile))
    return visited, blocked


# Check necessary structural and resource conditions without changing or excusing level data.
def check_level(level):
    errors = []
    level = timed_openings(level)
    human, seats = level["human"], level["seats"]
    names = {name for name, _ in level["goals"]}
    wins = [parts for _, parts in level["scripts"] if len(parts) >= 3 and parts[:2] == ["Action", "win"] and parts[2] == str(human)]
    if not any(kind == 1 and owner == human for kind, _, owner, _ in level["rooms"]):
        errors.append("player has no dungeon temple")
    if not names - {"ProtectDungeonTemple"} and not wins:
        errors.append("no winning goal or player script win")
    if not names and not any(parts[0] == "Action" and parts[1] in ("objective", "objectiveid") and parts[2] == str(human) for _, parts in level["scripts"]):
        errors.append("no player goal description")
    for number, parts in level["scripts"]:
        if parts[:2] == ["Cond", "goal"]:
            if len(parts) != 4 or int(parts[2]) not in seats or parts[3] not in names:
                errors.append("line %d: Cond goal references missing seat/goal" % number)
    area, frontier = reachable(level)
    if level["start"] not in area:
        errors.append("player start is blocked")
    if "KillAllEnemies" in names:
        targets = [("creature " + name, owner, [tile]) for name, owner, tile in level["creatures"]]
        targets += [("temple " + name if kind == 1 else "portal " + name, owner, footprint)
                    for kind, name, owner, footprint in level["rooms"] if kind in (1, 4)]
        for name, owner, footprint in targets:
            if seats[owner]["teamId"] == seats[human]["teamId"] or any(tile in area for tile in footprint):
                continue
            nearest = sorted(frontier, key=lambda tile: min(abs(tile[0] - target[0]) + abs(tile[1] - target[1]) for target in footprint))[:8]
            blockers = "; ".join("%d,%d(type=%d,full=%g,seat=%s)" % (*tile, *level["tiles"][tile]) for tile in nearest)
            errors.append("unreachable enemy %s seat=%d at %s; blocking frontier: %s" % (name, owner, footprint[0], blockers or "none"))
    for name, argument in level["goals"]:
        if name == "ClaimNTiles":
            count = sum(1 for tile in area if level["tiles"][tile][0] in (1, 2, 7))
            if count < int(argument):
                errors.append("only %d potentially claimable tiles for %s" % (count, argument))
        if name == "MineNGold":
            # DigCoefGold is fixed at 5; a reachable adjacent gem seam yields unlimited gold.
            gems = any(kind == 6 and fullness > 0 and any(n in area for n in neighbours(tile))
                       for tile, (kind, fullness, _) in level["tiles"].items())
            gold = sum(fullness * 5 for tile, (kind, fullness, _) in level["tiles"].items() if tile in area and kind == 2)
            gold += int(seats[human].get("goldMined", "0"))
            if not gems and gold < int(argument):
                errors.append("at most %g mineable gold for %s" % (gold, argument))
    return errors


# Validate explicit objective identities and only the individually mapped completion triggers.
def objective_errors(text, level_name):
    declarations, states, errors = {}, {}, []
    for block in re.findall(r"\[Trigger\](.*?)\[/Trigger\]", text, re.S):
        rows = lines(block)
        trigger = next((row.split(None, 1)[1] for _, row in rows if row.startswith("Name\t")), "")
        conditions = "\n".join(row for _, row in rows if row.startswith("Cond\t"))
        fingerprint = hashlib.sha256(conditions.encode("utf-8")).hexdigest()
        has_state = any(row.startswith("Action\tobjectivestate\t") for _, row in rows)
        if has_state and OBJECTIVE_CONDITIONS.get(level_name + "/" + trigger) != fingerprint:
            errors.append("objective status trigger conditions differ from reviewed conditions: " + trigger)
        for number, row in rows:
            fields = row.split(None, 5)
            if fields[:2] == ["Action", "objective"]:
                errors.append("legacy objective without an individual ID: " + trigger)
            elif fields[:2] == ["Action", "objectiveid"]:
                if len(fields) < 5:
                    errors.append("invalid objectiveid in " + trigger)
                    continue
                key = fields[2], fields[3]
                if key in declarations:
                    errors.append("duplicate objective ID " + fields[3])
                declarations[key] = row.split(None, 4)[4]
            elif fields[:2] == ["Action", "objectivestate"]:
                if len(fields) != 5 or fields[4] not in ("reached", "failed"):
                    errors.append("invalid objective state in " + trigger)
                    continue
                key = fields[2], fields[3]
                states.setdefault(key, []).append((trigger, fields[4]))
    for key in states:
        if key not in declarations:
            errors.append("status references undeclared objective " + key[1])
    for index, (key, description) in enumerate(declarations.items(), 1):
        expected_id = level_name.lower() + "-o%02d" % index
        if key[1] != expected_id:
            errors.append("objective ID differs from fixed inventory: " + key[1])
        completions = {trigger for trigger, state in states.get(key, []) if state == "reached"}
        if not completions:
            errors.append("objective has no completing trigger: %s: %s" % (key[1], description))
        expected = OBJECTIVE_TRIGGERS.get(level_name, [])
        if index > len(expected) or completions != set(expected[index - 1] or ()):
            errors.append("objective completion differs from reviewed trigger mapping: " + key[1])
    return errors


# Read ordered campaign entries without interpreting story prose as configuration syntax.
def campaign_entries(root):
    text = (root / "levels/campaign/Campaign.cfg").read_text(encoding="utf-8")
    return [dict(re.findall(r"^(\w+)=(.*)$", block, re.M))
            for block in re.split(r"^\[Level\]\s*$", text, flags=re.M)[1:]]


# Escape Markdown table separators while preserving the original story sentences.
def cell(value):
    return value.replace("|", "\\|").replace("\\n", " ").replace("\n", " ")


# Build a reproducible inventory of all level files, exact goals, script conditions and story wording.
def inventory(root):
    entries = {entry["File"]: entry for entry in campaign_entries(root)}
    result = ["# Campaign goal inventory", "", "Generated by source/tests/check_campaign_playability.py --write-inventory.", "",
              "This lists exact registered goals and script instructions, including the four legacy files outside Campaign.cfg.",
              "Every goal-bearing briefing sentence is reproduced verbatim. Match yes means its gameplay requirement is checked by a registered goal or script; no identifies the exact unchecked clause, including mixed sentences. Narrative context is omitted. Route/order instructions are not treated as satisfied merely because enemies are eliminated.", "",
              "Static reachability is a necessary condition, not a combat or script-solvability proof. Only once triggers whose sole conditions are elapsed time may open terrain for this analysis; flags, kills and other conditions are never assumed fulfilled.", ""]
    for path in sorted((root / "levels/campaign").glob("*.level")):
        text = path.read_text(encoding="utf-8")
        entry = entries.get("campaign/" + path.name)
        result += ["## " + path.stem, "", "File: `levels/campaign/" + path.name + "`" + (" (not listed in Campaign.cfg)." if entry is None else "."), "", "| Source | Exact requirement | Match |", "| --- | --- | --- |"]
        for _, line in lines(section(text, "Goals")):
            result.append("| [Goals] | `" + cell(line) + "` | yes |")
        for number, line in lines(text):
            parts = line.split(None, 3)
            if len(parts) >= 2 and (parts[:2] in (["Action", "objective"], ["Action", "objectiveid"], ["Action", "win"], ["Cond", "goal"])):
                result.append("| Script line %d | `%s` | yes |" % (number, cell(line)))
        trigger_conditions = {}
        for block in re.findall(r"\[Trigger\](.*?)\[/Trigger\]", text, re.S):
            rows = [row for _, row in lines(block)]
            trigger = next((row.split(None, 1)[1] for row in rows if row.startswith("Name\t")), "")
            conditions = "; ".join(row for row in rows if row.startswith("Cond\t"))
            trigger_conditions[trigger] = conditions
            for row in rows:
                if row.startswith("Action\tobjectivestate\t"):
                    result.append("| Trigger `%s` | `%s`; conditions: `%s` | yes |" % (trigger, cell(row), cell(conditions)))
        for error in objective_errors(text, path.stem):
            result.append("| Objective mapping | " + cell(error) + " | no: requires design decision |")
        for block in re.findall(r"\[Trigger\](.*?)\[/Trigger\]", text, re.S):
            for _, row in lines(block):
                fields = row.split(None, 4)
                if fields[:2] != ["Action", "objectiveid"] or len(fields) != 5:
                    continue
                review = OBJECTIVE_REVIEW.get(fields[3])
                if review is None:
                    continue
                candidates, missing, question = review
                candidate_conditions = "; ".join(candidate + ": " + trigger_conditions.get(candidate, "no matching existing trigger")
                                                 for candidate in candidates.split(", "))
                result.append("| Decision for `%s`: %s | Candidates/conditions: `%s`; missing: %s Proposal/question: %s | open |" %
                              (fields[3], cell(fields[4]), cell(candidate_conditions), cell(missing), cell(question)))
        if entry:
            sentences = re.split(r"(?<=[.!?])\s+", entry.get("Briefing", "").replace("\\n", " "))
            for index, match in BRIEFING_MATCH[path.stem].items():
                result.append("| Briefing | " + cell(sentences[index - 1].strip()) + " | " + match + " |")
        result.append("")
    return "\n".join(result)


# Exercise bad fixtures with fixed expectations, including a sealed Mossgate enemy and portal.
def self_test(root):
    original = (root / "levels/campaign/Mossgate.level").read_text(encoding="utf-8")
    level = parse_level(original)
    assert level["tiles"][1, 1] == (1, 100.0, None), "sparse defaults must be full dirt"
    assert not any("ReeveMarsh" in error for error in check_level(level)), "Mossgate reeve must be reachable"
    for target in ((7, 5), (31, 30)):
        bad = parse_level(original)
        for tile in neighbours(target):
            bad["tiles"][tile] = (3, 100.0, None)
        if target == (31, 30):
            bad["rooms"].append((4, "SealedPortal", 2, [target]))
        errors = check_level(bad)
        expected = "ReeveMarsh" if target == (7, 5) else "SealedPortal"
        assert any(expected in error and "blocking frontier" in error for error in errors), expected
    bad = parse_level(original)
    bad["scripts"].append((1, ["Cond", "goal", "1", "MissingGoal"]))
    assert any("missing seat/goal" in error for error in check_level(bad))
    bad = parse_level(original)
    bad["goals"] = [("ProtectDungeonTemple", "NULL")]
    bad["scripts"] = []
    assert "no winning goal or player script win" in check_level(bad)
    for kind in (3, 4, 5, 6):
        bad["tiles"][1, 1] = (kind, 100.0, None)
        assert not passable(bad, (1, 1)), "indestructible/liquid tile must block"
    bad["tiles"][1, 1] = (1, 100.0, 2)
    assert not passable(bad, (1, 1)), "enemy claimed wall must block"
    bad = parse_level(original)
    bad["rooms"] = [room for room in bad["rooms"] if room[0] != 1]
    assert "player has no dungeon temple" in check_level(bad)
    bad = parse_level(original)
    bad["goals"] += [("ClaimNTiles", "999999"), ("MineNGold", "999999999")]
    errors = check_level(bad)
    assert any("claimable tiles" in error for error in errors)
    assert any("mineable gold" in error for error in errors)
    for malformed in (original.replace("[/Tiles]", "[/Seats]", 1),
                      original.replace("seatId\t1", "seatId\t0", 1),
                      original.replace("9\t5\n[/Room]", "[/Room]", 1)):
        rejected = False
        try:
            parse_level(malformed)
        except ValueError:
            rejected = True
        assert rejected, "malformed loading input must be rejected"
    bad = parse_level(original)
    bad["tiles"][7, 5] = (3, 100.0, None)
    opening = "\n[Trigger]\nMode\tonce\nCond\ttime\t2400\nAction\tterrain\t7\t5\t7\t5\tpath\n[/Trigger]\n"
    bad["text"] += opening
    assert timed_openings(bad)["tiles"][7, 5] == (1, 0.0, None)
    bad["text"] = bad["text"].replace("Cond\ttime\t2400", "Cond\tflag\tblocked\t1")
    assert timed_openings(bad)["tiles"][7, 5] == (3, 100.0, None), "conditional gate must stay blocked"
    wraithwood = parse_level((root / "levels/campaign/Wraithwood.level").read_text(encoding="utf-8"))
    assert wraithwood["creatures"], "nested tuning sections must not replace actual creatures"
    assert not objective_errors(original, "Mossgate"), "all Mossgate objectives need exact completions"
    missing_state = re.sub(r"^Action\tobjectivestate\t1\tmossgate-o02\treached\r?\n", "", original, flags=re.M)
    assert any("no completing trigger: mossgate-o02" in error for error in objective_errors(missing_state, "Mossgate"))
    altered = original.replace("Cond\tmined\t1\t>=\t60", "Cond\tmined\t1\t>=\t1")
    assert any("conditions differ" in error for error in objective_errors(altered, "Mossgate"))
    wrong_status = original.replace("mossgate-o02\treached", "mossgate-o02\tunseen")
    assert any("invalid objective state" in error for error in objective_errors(wrong_status, "Mossgate"))
    unknown_status = original.replace("mossgate-o02\treached", "mossgate-unknown\treached")
    assert any("undeclared objective" in error for error in objective_errors(unknown_status, "Mossgate"))
    print("campaign playability self-test passed")


# Check every configured level and fail on errors or a stale goal inventory.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--write-inventory", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test(ROOT)
        return 0
    document = inventory(ROOT)
    path = ROOT / "materials/campaign-goals/GOALS-INVENTORY.md"
    if args.write_inventory:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(document, encoding="utf-8")
    failures = []
    entries = campaign_entries(ROOT)
    if len(entries) != 31 or len({entry.get("File") for entry in entries}) != 31:
        failures.append("expected 31 distinct configured campaign levels")
    for entry in entries:
        name = entry.get("File", "")
        path_level = ROOT / "levels" / name
        try:
            if path_level.parent != ROOT / "levels/campaign" or not path_level.is_file():
                raise ValueError("missing/invalid campaign level path")
            level = parse_level(path_level.read_text(encoding="utf-8"))
            failures.extend(name + ": " + error for error in check_level(level))
            failures.extend(name + ": " + error for error in objective_errors(level["text"], path_level.stem))
        except (ValueError, IndexError, KeyError, OSError) as error:
            failures.append(name + ": " + str(error))
    configured = {entry.get("File") for entry in entries}
    for legacy in sorted((ROOT / "levels/campaign").glob("*.level")):
        if "campaign/" + legacy.name not in configured:
            failures.extend("campaign/" + legacy.name + ": " + error
                            for error in objective_errors(legacy.read_text(encoding="utf-8"), legacy.stem))
    if not path.is_file() or path.read_text(encoding="utf-8") != document:
        failures.append("goal inventory is missing or stale; regenerate with --write-inventory")
    for error in failures:
        print(error)
    print("campaign playability: %d levels, %d errors" % (len(entries), len(failures)))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
