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
OBJECTIVE_TRIGGERS = {'Ashcombe': [('wardenDown',),
              ('holdBreached',),
              ('goalHoldBeforeDeparture',
               'goalRoadCatch01',
               'goalRoadCatch02',
               'goalRoadCatch03',
               'goalRoadCatch04',
               'goalRoadCatch05',
               'goalRoadCatch06',
               'goalRoadCatch07',
               'goalRoadCatch08',
               'goalRoadCatch09',
               'goalRoadCatch10',
               'goalRoadCatch11',
               'goalRoadCatch12',
               'goalRoadCatch13',
               'goalRoadCatch14',
               'goalRoadCatch15',
               'goalRoadCatch16',
               'goalRoadCatch17',
               'goalRoadCatch18',
               'goalRoadCatch19',
               'goalRoadCatch20',
               'goalRoadCatch21',
               'goalRoadCatch22',
               'goalRoadCatch23')],
 'Bellwick': [('keeperHeartDown',), None, ('naveGatesKills', 'naveGatesLate'), ('transept',)],
 'BoulderCourse': [('courseDone',)],
 'Brackenford': [('wardenGone',),
                 ('bedsReady',),
                 ('larderReady',),
                 ('groveClaimed',),
                 ('goalLandingCleared',),
                 ('wardenGone',)],
 'Campaign1': [('goalClassicComplete',)],
 'Campaign2': [('goalClassicComplete',)],
 'Campaign3': [('goalClassicComplete',)],
 'Cinderhollow': [('goalHeartSafe1BeforeRivalHeartDown', 'goalHeartSafe1BeforeRivalHeartFalls'),
                  ('goalBothFiniteSeamsMined',),
                  ('goalThreeWavesDefeatedAtHeart',),
                  ('goalCraterAfterBridge',),
                  ('rivalHeartDown', 'rivalHeartFalls')],
 'Coldwell': [('wardenSlain',), ('plazaReached',), ('goalUndercroft600',), ('courtBreached',)],
 'CrowshotGallery': [('galleryClear',)],
 'Dunmarrow': [('wardenCaught',), ('barrowsEntered',), ('goalCampBeforeLoading', 'goalCampBeforeDeparture')],
 'Ebonrook': [('wardenSlain',), ('crossed',), None, ('scoutPossessed',), ('wardenSlain',)],
 'Goldspire': [('wardenSlain',), None, ('lowTown',), ('onRing',), ('wardenSlain',)],
 'Greywater': [('wardenFalls',), ('firstCoin',), ('goalHeartSafe3BeforeWardenFalls',), ('siegeSeen',)],
 'Highcairn': [('wardenSlain',),
               ('goalTerrace4',),
               ('goalHeartSafe3BeforeWardenSlain',),
               ('wardenSlain',),
               ('stoneHome', 'stoneHome2'),
               ('holdSealBreaks',)],
 'HollinFen': [('wardenSlain',), ('goalHeartSafe2BeforeWardenSlain',), ('wardEntered',), ('wardenSlain',)],
 'HollowmarkCitadel': [('kingDead',), ('goalCourt3',), ('goalHeartSafe3BeforeKingDead',)],
 'Ironbridge': [('rivalFalls', 'rivalHeartDown'),
                ('goalIronbridgeAllSpans',),
                ('yardReached',),
                ('goalRivalGateNorth', 'goalRivalGateSouth')],
 'Kingsfall': [('collectWin',), ('courtTaken', 'collectWin'), ('goalArmyAtHeart',)],
 'Lanternhill': [('rivalHeartDown',), ('goalRingGroundPathOpened',), ('goalHeartSafe3BeforeRivalHeartDown',)],
 'Mirewatch': [('watcherSlain',), None, ('goalNineTowerRoomsClaimed',), ('scoutTowerC',), ('watcherSlain',)],
 'Mossgate': [('reeveFalls',), ('firstGold',), ('meadowOurs',), ('goalWatchPostCleared',), ('reeveFalls',)],
 'Ravensledge': [('captainDown',),
                 ('shelfHeld',),
                 ('shelfHeld',),
                 ('yardEntered',),
                 ('goalInnerKeepEntered',)],
 'Saltmere': [('admiralFalls',),
              ('firstSalt',),
              ('goalHeartSafe3BeforeAdmiralFalls',),
              ('flatHeld', 'portGatesTimeout'),
              ('admiralFalls',)],
 'Silverdeep': [('silverWonGoal',), ('goalSilverdeepAllSpans',), None],
 'SkittleCavern': [('laneCleared',)],
 'Stormhaven': [('stormEndsGoal',), ('goalStormhavenBothSpans',), ('goalPactsPrevented',)],
 'SwarmNight': [('goalHeartSafe1BeforeDawn',), ('goalHeartSafe2BeforeDawn',)],
 'Thornreach': [('keepersGoneGoal',), None, ('goalHeartSafe3BeforeKeepersGoneGoal',)],
 'Tinmoor': [('wardenDown',),
             ('seamsFilled',),
             ('goalPostACleared',),
             ('goalPostBCleared',),
             ('scoutSeams',)],
 'TwistingHalls': [('hallReached',)],
 'VarnsCrossing': [('crossingHeldGoal',),
                   ('goalHeartSafe2BeforeCrossingHeldGoal',),
                   ('goalQuarterThenFortress',)],
 'Wolfscar': [('huntersGone', 'huntersGoneGoal'),
              ('southCrossed',),
              ('goalHeartSafe3BeforeHuntersGoneGoal',)],
 'Wraithwood': [('bothFallGoal',), ('bothFallGoal',), ('goalHeartSafe3BeforeBothFallGoal',)]}
HINT_IDS = frozenset(['bellwick-o02', 'ebonrook-o03', 'goldspire-o02', 'mirewatch-o02', 'silverdeep-o03', 'thornreach-o02'])
# A proven positive mask does not establish exhaustive coverage of dynamic waypoint movement.
OBJECTIVE_COVERAGE_PENDING = {
    "ashcombe-o03": "road alternative has101 verified floor tiles, but Chebyshev-one waypoint skipping may use additional floor; exhaustive road coverage is not established",
}

# Individually reviewed non-matches: candidate triggers, missing requirement and proposed decision.
# Candidates are never treated as completions without an explicit design decision.
OBJECTIVE_REVIEW = {'ashcombe-o03': ('goalHoldBeforeDeparture, goalRoadCatch01..23',
                  'Hold-before-departure is exact. The road death alternative checks101 verified dry floor '
                  'tiles outside Hold, represented by23 narrow rectangles, and requires departure plus a '
                  'stored killedin event. The mask is the union of shortest four-neighbour paths between '
                  'authored escape waypoints; Chebyshev-one waypoint skipping leaves exhaustive dynamic-road '
                  'coverage unproven.',
                  'Keep this objective partial until the road domain is explicitly defined or exhaustive '
                  'dynamic-route coverage is established; never substitute generic Trail or a bounding '
                  'rectangle.'),
 'bellwick-o02': ('naveGatesKills, naveGatesLate',
                  'Neither checks the end of the faithful attacks; later waves continue.',
                  'Use the existing gate-opening milestone only if holding the nave is meant to end there; '
                  'should that milestone complete this text?'),
 'brackenford-o05': ('landingSeen',
                     'The exact original initial guards and spawn origin are known; ford-bank crossing '
                     'observations still need verified geometry.',
                     'Implementation: observe the actual ford crossing, killed FordGuard1/2 and spawncleared '
                     'landingSeen action1; no unrelated global kill counter.'),
 'campaign1-o01': ('none',
                   'All existing triggers are time hints, not goal completion.',
                   'Add a presentation-only completion trigger using the existing KillAllEnemies goal; may '
                   'this additional trigger be added without altering any existing condition?'),
 'campaign2-o01': ('none',
                   'All existing triggers are time hints, not goal completion.',
                   'Add a presentation-only completion trigger using the existing KillAllEnemies goal; may '
                   'this additional trigger be added without altering any existing condition?'),
 'campaign3-o01': ('none',
                   'All existing triggers are time hints, not goal completion.',
                   'Add a presentation-only completion trigger using the existing KillAllEnemies goal; may '
                   'this additional trigger be added without altering any existing condition?'),
 'cinderhollow-o02': ('GoldHollow, WestRoad',
                      'The named western and southern seams have no exact required extents or mined '
                      'quantity, while the first wave starts at420 seconds.',
                      'Which western/southern gold tiles count, and is any gold from each seam sufficient or '
                      'must both be exhausted? Recommend retaining the exact420-second deadline and '
                      'explicitly named existing seam areas.'),
 'cinderhollow-o03': ('waveThree',
                      "The trigger spawns the third wave; it does not check any wave's defeat or its "
                      'position at the heart.',
                      'Keep this objective open until a specific wave-defeat condition is authorized; may a '
                      'new presentation trigger check those existing wave creatures?'),
 'cinderhollow-o04': ('craterSeen',
                      'Entering RivalCrater does not explicitly require dawn or crossing the lava bridge.',
                      'Use craterSeen as the crossing milestone if arrival is sufficient; should it complete '
                      'this objective?'),
 'coldwell-o03': ('wellEmptied',
                  'The600 threshold is explicit, but global mined gold does not attribute it to the block '
                  'under the plaza.',
                  'Implementation: add regional mining history for the verified under-plaza gold block, '
                  'retaining exactly600 coins; do not relax it to global mining.'),
 'dunmarrow-o03': ('campEntered',
                   'Entering Camp does not check that the wagon departure timer has not expired.',
                   'Use campEntered only if arrival alone is sufficient; must the existing wagon deadline '
                   'also be required?'),
 'ebonrook-o03': ('firstCoin',
                  'Mining 300 gold does not check feeding the army from the glades.',
                  'Use firstCoin for the mining part only; should feeding remain advice rather than a '
                  'separately checked requirement?'),
 'goldspire-o02': ('firstCoin',
                   'Mining 500 gold does not check building an army; the text specifies no army size.',
                   'Use firstCoin only if building a force is advice; should this objective complete at the '
                   'existing mining milestone?'),
 'highcairn-o02': ('terrace1, terrace2, terrace3, terrace4',
                   'Each trigger checks one terrace independently; none checks all terraces in order.',
                   'Use terrace4 as the final climbing milestone if the order is descriptive; should '
                   'reaching it complete this objective?'),
 'hollowmarkcitadel-o02': ('gate3Open, thirdGate, throneHall',
                           'Tower visits open gates; no trigger checks three conquered courts and fallen '
                           'towers together.',
                           'Use throneHall as the end of the court route if arrival is sufficient; should it '
                           'complete the objective?'),
 'ironbridge-o02': ('gateReached',
                    'Entering RivalGate does not explicitly check all three rift crossings.',
                    'Use gateReached as the last crossing milestone if route arrival is sufficient; should '
                    'it complete this objective?'),
 'ironbridge-o04': ('gateReached, keepReached',
                    'RivalGate is the gate approach; OreDock is outside RivalGate and RivalKeep, so neither '
                    'proves breaking through the gate.',
                    'Do not use keepReached; should gateReached count as completion, or is a new inside-hold '
                    'completion check required?'),
 'kingsfall-o03': ('courtTaken, collectWin',
                   "Both count twelve creatures anywhere, not at the player's heart.",
                   'Keep the location requirement unless explicitly relaxed; may the existing whole-map '
                   'creature count complete this objective?'),
 'lanternhill-o02': ('none',
                     'Several different full-earth plugs exist and are bypassable; no existing region or '
                     'trigger identifies which plural plugs this text requires.',
                     'Which exact tile coordinates are the required earth plugs? Recommend explicitly '
                     'identifying those existing tiles; never treat all full dirt in Ring as mandatory '
                     'digging.'),
 'mirewatch-o02': ('firstCoin, hollowHeld',
                   'Mining 400 gold or claiming 40 tiles of Hollow does not check building before marching.',
                   'Use hollowHeld as the preparation milestone only if building and order are advice; '
                   'should it complete this objective?'),
 'mirewatch-o03': ('towerA, towerB, towerC, towerD, towerE, towerF, towerG, wardenSeen',
                   'All eight tower regions are known, but taking a tower can mean mere entry or clearing '
                   'and occupying it; no fixed visit order is stated.',
                   'Should a tower count after own presence and zero enemies in its region? Recommend that '
                   'cleared occupation for all eight, without inventing a fixed branch order.'),
 'mossgate-o04': ('none',
                  'yardAlarm plus3 global kills does not prove the actual watch-post guards were defeated.',
                  'Observe the actual initial and triggered post defenders; do not substitute unrelated '
                  'kills.'),
 'ravensledge-o05': ('yardEntered',
                     'Entering Yard does not check breaking the inner keep gate.',
                     'Do not complete it on the yard approach; may an additional inside-keep presentation '
                     'trigger be added?'),
 'silverdeep-o02': ('spanA, spanB, spanC, southBank',
                    'These are individual bank visits, with duplicate SouthBank conditions, not a combined '
                    'three-bridge check.',
                    'Prefer spanC as the last span milestone if arrival is sufficient; should that existing '
                    'trigger complete this objective?'),
 'silverdeep-o03': ('firstSilver',
                    'Mining has an observable result, but the text defines neither the order meant by first '
                    'nor a coin threshold;400 is merely a separate original milestone.',
                    'Is this preparation advice, or a required first-action sequence? Recommend hint only if '
                    'it is advice; otherwise define the prior event and required seam mining explicitly, '
                    'without borrowing400.'),
 'stormhaven-o02': ('spanSeen, northSpan',
                    'Each checks one span region; neither checks crossing the inlet by the stone spans.',
                    'Use northSpan as the final crossing milestone if arrival is sufficient; should it '
                    'complete this objective?'),
 'stormhaven-o03': ('tideTurns, tideTurns2',
                    'The two future alliance makes occur at2200 and3300 seconds; the phrase does not specify '
                    'how defeat of one pact participant counts as prevention.',
                    'Does preventing each timed pact mean at least one participant must already be defeated '
                    'when it would form? Recommend that concrete criterion rather than a generic victory '
                    'milestone; initial alliances need explicit treatment.'),
 'thornreach-o02': ('hedgeA, hedgeB, clearingSeen',
                    'Region visits do not check digging and burning a path through the maze.',
                    'Use clearingSeen as the route-end milestone if the travel method is advice; should it '
                    'complete this objective?'),
 'tinmoor-o03': ('none',
                 'postAAlarm plus7 global kills does not prove defeat of the lower-post defenders.',
                 'Observe the exact PostA initial and triggered defenders.'),
 'tinmoor-o04': ('postBEntered',
                 'Entering PostB does not check destroying the upper watch post.',
                 'Do not use entry as destruction; may a presentation-only check for the existing upper-post '
                 'enemies be added?'),
 'varnscrossing-o03': ('quarterHeld, keepEntered, crossingHeldGoal',
                       'These independently check claiming, entering or all enemies; none checks claiming '
                       'the quarter before taking the fortress.',
                       'Use crossingHeldGoal if ordering is advice and defeating the fortress is sufficient; '
                       'should that existing goal trigger complete this text?')}

# Fixed fingerprints of the reviewed existing conditions; never recalculated as expectations.
OBJECTIVE_CONDITIONS = {'Ashcombe/goalHoldBeforeDeparture': '776b58a0778ede6f329bf9703c105074745fe0c1d7b273766e3e2fb052d95454',
 'Ashcombe/goalHoldOrRoadMissed': '6b50aa8f09f86e56c7383895f34b7b828306cbfa792e8aaf22af95b105e6708a',
 'Ashcombe/goalRoadCatch01': '9d6bdbf104b8cb8b75a29807132a334b688271b6e63b85eefaa870679b6ff49f',
 'Ashcombe/goalRoadCatch02': '7fa941ce9e4f225d2775d5ddc8f7fa09831ba4b37f2aef2b146696d99b8ec5b9',
 'Ashcombe/goalRoadCatch03': 'd55c16b588a12a07fbe799b3b7cd8a7bffd3f23fb330116c0f4e90215eb5f83c',
 'Ashcombe/goalRoadCatch04': '51188b657313d597208e0e14dd977ecdd5015b1cf7fcdd8ce38fe16ed9c01731',
 'Ashcombe/goalRoadCatch05': '4a418368d2be57af97ccc6ac2f5b71f92481bffb250d9dd00dc9da79a776832d',
 'Ashcombe/goalRoadCatch06': '1cbb33c12afdd0054b7e36b38420c3f77a167a5d0f747acdc735758bfc65f1c6',
 'Ashcombe/goalRoadCatch07': '942298fa653a1e883939112887913492dafc0a780351ba01eaeccffd4e204ffe',
 'Ashcombe/goalRoadCatch08': 'eb2d1f720a475267ba8438c625f42425c5833f2adba773a78a699a58fd8094ef',
 'Ashcombe/goalRoadCatch09': '76752c3b3c3a2ca7d06cb8cc6bfbaf4e805486c34caf746f7fba4ecc9e03df17',
 'Ashcombe/goalRoadCatch10': 'e3221c50ed68ebfc4613cb03d230335ee17ed351e441fbec32b6e416a3abebc3',
 'Ashcombe/goalRoadCatch11': 'c9cf336292ae11f105f10caa3fb66d0bdc214ed32e7533858ce4c1ff575c5a0f',
 'Ashcombe/goalRoadCatch12': '5e7ed6191331e8281c575ea436076b4266a38c465e3215f7f10a43236442d896',
 'Ashcombe/goalRoadCatch13': '27de079fa517720460b0c80551921faf975faa1fedb12b09286f7ea6a4d74354',
 'Ashcombe/goalRoadCatch14': 'bf8f2e3b930c840a1a23a1ab01280680f752ff3f1287c34e9c454d6d99c70077',
 'Ashcombe/goalRoadCatch15': '23286f9d3edf25148d4afff68358b9155999c61ad4ea7a5b729d5c46bb9221f4',
 'Ashcombe/goalRoadCatch16': '7c95cbefba5fa1ece631f1bb8297ff32924b3be5a49f52f90cd974f9d765e1e5',
 'Ashcombe/goalRoadCatch17': '9017a43f3723644712509a13e92c06918b5caf6ceac3cf6dd43ee6abf0e76b78',
 'Ashcombe/goalRoadCatch18': 'bc4e96a00d30680d20bc8eb951251f48052579fe1dc1cbffbc36e91772cb28f1',
 'Ashcombe/goalRoadCatch19': 'e57f462a20087bea563df3ae317eec611a93281dc3dab12e3378ca47121a71c3',
 'Ashcombe/goalRoadCatch20': '783f13c1a67139406d2776b6a0c8d876f2d107b1aba83d985360f9a2670c4480',
 'Ashcombe/goalRoadCatch21': '5d831e7e0f7672353e2021990dd889e6f62fda28b0e82c6b04e55c436a2169aa',
 'Ashcombe/goalRoadCatch22': '2db9466c5d12e823f6ec015ba1e4226dc13ec86b9c821a36c4cabdefcab5fb7d',
 'Ashcombe/goalRoadCatch23': 'dcdfdb999b249c023a17026306993fe3e5e9a4a6b5d1a3f075468c1909996f33',
 'Ashcombe/holdBreached': 'bae0a393bc01e08ad3ae0b95d56ee971dc346396388423a3d0c019f97ff58aaf',
 'Ashcombe/wardenDown': '9099b47ed59d017b8ca6cede9c74e2bbfe5d91a9ee0d4ee005dbbf423926927f',
 'Ashcombe/wardenEscaped': '56e37df8874d71b952e4aff8e246d5568afc363fa678dde537ce5b53d3bb0604',
 'Bellwick/keeperHeartDown': '2a99324afebb946debdf455a762b0a3397ab53239aff6aa5c15a82d7ca0af1e6',
 'Bellwick/naveGatesKills': 'e796048b8442103fde2ec73588b9d953975383db05e5bdf9017869aff366deb6',
 'Bellwick/naveGatesLate': '19a266ae96382f14cdc2629ae7b76df0f1c0af1308564afef74ad5f564e4da11',
 'Bellwick/transept': 'b8dd598cef221a526a3e14566cc0c21315e63d25e9dcf7159ffc5ae473d12f9e',
 'BoulderCourse/courseDone': '1568e1b864d5e350c0ccf8d87fdba7f79a0d3c1bfe09b6b32ba50ea38f830ece',
 'Brackenford/bedsReady': 'a753667474f8af0a9fefaeb6a9bd6a426c94e9dbf947f3654dafbfb333427e1d',
 'Brackenford/goalFordEast1': '52594f52d76bb6de763749aa38a4371d651cafd3050acd041ed65ed4e9641652',
 'Brackenford/goalFordEast2': '1e9035b14ea563b6fc3c26a6ea6480322f26340008b188f3c123865d0a5c8486',
 'Brackenford/goalFordEast3': '15c49b410dd23e48b9c5667327056d3971715b1a59f652c9b3aa73179999c5a6',
 'Brackenford/goalFordWest1': '588ee0e475f33df7a63458cc30a7555039d21bd932031a28b63615930a79a5ac',
 'Brackenford/goalFordWest2': '0119b7cacd15b777788e9b79398cd741effbb61244828670f3e2114e0e856253',
 'Brackenford/goalFordWest3': 'b755112eeb7ca685f80a4a8b93bfff1d9323fa3b7c57030aa12048adaef2e4f4',
 'Brackenford/goalLandingCleared': '65bde3d69b3321e5e5370b3fe340581bc46d2ef4a619edf10d812b6e23938783',
 'Brackenford/groveClaimed': '4876c136061a43ce924a55baa9dc20f30a00ee0f1b9cb9598ce1e5a9c1c8a031',
 'Brackenford/larderReady': '562b2b930847e8f1fb6f9fe93dc680903ec501b0198b5d6696cce209c37aa637',
 'Brackenford/villageOpen': 'f979ec5bfc4d35b4bb42bccfdf08eb59d408b8688e3fc3b31e8ec7d10506f5ef',
 'Brackenford/wardenGone': '64247d08b33c60aecb9970aaa61cad974015e18496b97341fb463acf439130ca',
 'Campaign1/goalClassicComplete': '96473efcffb5efb40af72300b603f80aa89d0b62b4c4954fa2a7546f63e5581c',
 'Campaign2/goalClassicComplete': '96473efcffb5efb40af72300b603f80aa89d0b62b4c4954fa2a7546f63e5581c',
 'Campaign3/goalClassicComplete': '96473efcffb5efb40af72300b603f80aa89d0b62b4c4954fa2a7546f63e5581c',
 'Cinderhollow/goalBothFiniteSeamsMined': 'e62a2ec0adbe02d135469b88e1deb7dd4b92295ec84231fb8055c4f7eddc87e3',
 'Cinderhollow/goalCraterAfterBridge': '21fdcb397f1ea56cea9e66f4b260c7383dc924ec44f362220a6fcd2acd9cc678',
 'Cinderhollow/goalDawnBridge': '31bfa776f2fabb75b9c400e4c14449f2a23ae0ccb9c090599b354d62c37237b5',
 'Cinderhollow/goalDawnBridgeApproach': '7b7cc92553db0a630d9d8eeb64e9163000ea576ebde2679543af1f26f4ca5e02',
 'Cinderhollow/goalDawnBridgeCrossed': '05f90e1f8e74e92ee126418d8d02c528ef152fe4aff7e0f87c532bc0c3df4262',
 'Cinderhollow/goalThreeWavesDefeatedAtHeart': '739e7b104d57d3c922e9216d3f41d887c0efeff9d8c18523bd8c5b80500724b5',
 'Cinderhollow/rivalHeartDown': 'b490f8085c95c318f0aee37b0bcb7f903d47f272d0bedb3114676bcf5146fc3b',
 'Cinderhollow/rivalHeartFalls': 'e39927741e06e2f5d2a2863a979535b542e7774e671af7d7838cc8153b14097d',
 'Coldwell/courtBreached': '44ebb7a1fa5c16d2bc4cf4cfd4e82ec1ea98501c6839fb6190f50f6b51175f33',
 'Coldwell/goalUndercroft600': '3998fc8dbd07fe00c625e156ff639e809b6d44933db162f7a9e0425ad8d86da4',
 'Coldwell/plazaReached': 'b3c03fd9a567992624fe4dfaffb2bd550a0585faddb0875c6d59676e3472c48e',
 'Coldwell/wardenSlain': '64247d08b33c60aecb9970aaa61cad974015e18496b97341fb463acf439130ca',
 'Coldwell/wellEmptied': '6591ba5aceeb629439c2b37862d24cadf5846841e8477f759af3d91288c59ad1',
 'CrowshotGallery/galleryClear': '70a36becadb693911641ad3bf0ea8bfcb986d9f6d515c0e1d89ee1ca6a9944e0',
 'Dunmarrow/barrowsEntered': '085380acca4d034f15c91a10680450c59c2d33c9865fef70b3a793273eb2e149',
 'Dunmarrow/goalCampBeforeDeparture': '5085e5ee9ca8e0f6a1a9a14a603abf28a3abd5623d0a39bf0b60554a5f03b36d',
 'Dunmarrow/goalCampBeforeLoading': 'ba5e0e926b76ad3d7dcfbc068200752958f822daa72997d0fa4cab1d070f2b44',
 'Dunmarrow/goalCampMissed': '0191ce1746a390ebbdd0d27957a9a3cee2e08d6d5c44a23c8a805deb892cea11',
 'Dunmarrow/wardenCaught': 'f50e1bd028271d6808509e83e18a5ecf8d30c344d57b3b49c68b9e29e0734865',
 'Dunmarrow/wardenEscaped': 'e624c6cc8fd2849cdc16f3d18df11a92436d2dcd0a89b4dd72a8e2c1c6a27b37',
 'Ebonrook/crossed': 'f074e0ce94d380e77fed879ecdb320a5953d48adefdf66c63e4ec876a6998358',
 'Ebonrook/scoutPossessed': '3b6089fe0ea54d0fb030187fcc8d905f38edcaaa1c6757938aaa4d0caced81cb',
 'Ebonrook/wardenSlain': '64247d08b33c60aecb9970aaa61cad974015e18496b97341fb463acf439130ca',
 'Goldspire/lowTown': 'b0850f7040d983080895d0dde679743b6d2296c36ef4f248e7c3de5ec3c76dec',
 'Goldspire/onRing': '7f80618dff9ef40c3c8ba326475dea955da625c4cec0f63e159540e0716d0160',
 'Goldspire/wardenSlain': '64247d08b33c60aecb9970aaa61cad974015e18496b97341fb463acf439130ca',
 'Greywater/firstCoin': '6591ba5aceeb629439c2b37862d24cadf5846841e8477f759af3d91288c59ad1',
 'Greywater/goalHeartLost3': '65883912aaee2758fba63138f970f145b041ff9190c5a9b5c207577321079e3e',
 'Greywater/goalHeartSafe3BeforeWardenFalls': '7511c2d91d074bbb659838927d53cedb4bf3600b0df0b6b29f74d9d3ac32304f',
 'Greywater/siegeSeen': '3abd24d1062606a705b9a1eccbf238205629f9896f24ea13ce4ccd40d444210f',
 'Greywater/wardenFalls': '3a263d0a43819eb0c082efacc0eccd1096b304cddf3227642cd0db922bb597ca',
 'Highcairn/goalHeartLost3': '65883912aaee2758fba63138f970f145b041ff9190c5a9b5c207577321079e3e',
 'Highcairn/goalHeartSafe3BeforeWardenSlain': '1c8e23a3845b1a642d748b1ddf1a7fc16ee5208432da7349f65c4c98948417ee',
 'Highcairn/goalTerrace1': 'c9e3c2088afe2f1eccaee96b0ad324aa2f1646e27b9cb86a860726417d412f1b',
 'Highcairn/goalTerrace2': '4acedeef1aea3f655e52c27459519c7bdb001c839d3ed01b16cd0fd4d21644b7',
 'Highcairn/goalTerrace3': '985150fc38e89890402b5e67d691ae0800fcd05adb22c486b72b559ee37eaa69',
 'Highcairn/goalTerrace4': 'ccd3ee14ef9755706a7c4575ab18958da05c515c1fccef0a6a9f38e78d0d6a4e',
 'Highcairn/holdSealBreaks': '8d98fbb5f48f929b4e4e9eca60cfdb982bfd32ca5d430f8918bdf3de7041f9fb',
 'Highcairn/stoneHome': 'bf6c9dad942cb5d2f04f994d6355cd9af755cedc6c9e7d1122d8205843db3c19',
 'Highcairn/stoneHome2': '14e787f8a1aa9a89f3e765383f0c68e97714f9cc40f58056110e73e77baff80c',
 'Highcairn/wardenSlain': '64247d08b33c60aecb9970aaa61cad974015e18496b97341fb463acf439130ca',
 'HollinFen/goalHeartLost2': '65883912aaee2758fba63138f970f145b041ff9190c5a9b5c207577321079e3e',
 'HollinFen/goalHeartSafe2BeforeWardenSlain': '1c8e23a3845b1a642d748b1ddf1a7fc16ee5208432da7349f65c4c98948417ee',
 'HollinFen/wardEntered': '5104e92aff9354bc899db8c3294c4399f167ae7405b47ae4c7c15f58990f8c4a',
 'HollinFen/wardenSlain': '64247d08b33c60aecb9970aaa61cad974015e18496b97341fb463acf439130ca',
 'HollowmarkCitadel/goalCourt1': '3d1ce8ca64b9a981c52c1f8ebdbbe83f4da4ed551b30c1928670f20af029b1db',
 'HollowmarkCitadel/goalCourt2': '9f6e92ea03889c9a1a6f2f5a8c6960c9fc2e1288acb8b7194f3921d6c08f5197',
 'HollowmarkCitadel/goalCourt3': '2ae7109231ea81d177354b7a722794606c9c505424d37b461e8500a795cca839',
 'HollowmarkCitadel/goalHeartLost3': '65883912aaee2758fba63138f970f145b041ff9190c5a9b5c207577321079e3e',
 'HollowmarkCitadel/goalHeartSafe3BeforeKingDead': '14f2b1e2aad09c1df88d5009c5a517af3ec88c319f3644c7db73160daf1e4f4d',
 'HollowmarkCitadel/kingDead': '3ae8027124d07835bcc3ccbe6538e8c04c3e7c11a1a389cd2ad1b32e4bb37b4a',
 'Ironbridge/goalIronbridgeAllSpans': '6fb678898964c140879412d1ea1abf0b469c50007b55234fa26058644dd3bfd9',
 'Ironbridge/goalIronbridgeSpan1Path1After': 'fea92d67d0c5b1024bd003c64ef52d6bdbb6539794baee4d2a1f36bf45bee6c3',
 'Ironbridge/goalIronbridgeSpan1Path1Before': '082378aae54def52c9b74b1c8131d40833f57d3a9b31928027a9486b71808826',
 'Ironbridge/goalIronbridgeSpan1Path1On': '57cb1cd7b7596a7a63b12ce8ea1053d42ac96881b8fa7f4b54701d63e342207a',
 'Ironbridge/goalIronbridgeSpan1Path2After': '136de98036242f586128c29b7591721afc8e474fd09f8838880970777f92980c',
 'Ironbridge/goalIronbridgeSpan1Path2Before': 'b1a0ef1f33c2d02244a3ae542a4663ee7d473cea7c1b2ffa7cb3b98aeffe4914',
 'Ironbridge/goalIronbridgeSpan1Path2On': '1c74ffdcc42c2935ba4228013dca9bb4eca4656da36013f03901113c7610821c',
 'Ironbridge/goalIronbridgeSpan2Path1After': 'd504715afae26cc27907393ba37bd8ce1bb11580d6c6038bfc5827679cb2fbd1',
 'Ironbridge/goalIronbridgeSpan2Path1Before': '69201cf86e142f41432e28ee1271f6c3d2ed8f9949295a77e12e9ab74913b2b8',
 'Ironbridge/goalIronbridgeSpan2Path1On': '22cf555d07ab3c55ca691c1c7d7f42143ed23930f06d6f682248e7dc2870e8f5',
 'Ironbridge/goalIronbridgeSpan2Path2After': 'a0254701517f07d403f45a7e198bf9dbf1d9c8ae818c72133a2d5471b24f121f',
 'Ironbridge/goalIronbridgeSpan2Path2Before': 'ac8ad923d56ff29bdda0189b1fa32fe4a3284b646a939fdafb44a8a0187fd57c',
 'Ironbridge/goalIronbridgeSpan2Path2On': '301aea01ee2186ae0bce84da39277b37db8e306381ee013c61a331fb80e18595',
 'Ironbridge/goalIronbridgeSpan2Path3After': 'beff7b631178e8f3dc8885dbc8635466fdd0965ddd1929ae6f9ecb61219a6a44',
 'Ironbridge/goalIronbridgeSpan2Path3Before': '3cad02573938e52d908bd67b5e849a5b7fef9b0053d876247459e1a28fdaf787',
 'Ironbridge/goalIronbridgeSpan2Path3On': '6a8b78e067fb614b392e3a6413fc1c5e9d7ab85eb45d4f5635fb85dd542c6b36',
 'Ironbridge/goalIronbridgeSpan3Path1After': '5f4cb835bd1d128714dc68c5a6f0e41533f3b744ac91ea419844abe1649a8e88',
 'Ironbridge/goalIronbridgeSpan3Path1Before': 'bdf3fbb1622c9c4c7193d88a75d7b30738f322274df13c770e87b63044369dc6',
 'Ironbridge/goalIronbridgeSpan3Path1On': '472f424b57a8b0f24be4d4e9546da1b7b884c4aed4573e12151bc6328db4e16e',
 'Ironbridge/goalIronbridgeSpan3Path2After': 'b07f20333f338115514eff92fc1d94803c1dbe4c3da654921e0848e4f69b0b94',
 'Ironbridge/goalIronbridgeSpan3Path2Before': '5bf7f35849abf3125290be601981deee2aff55cb77d37344fc75c4474f165a16',
 'Ironbridge/goalIronbridgeSpan3Path2On': 'df25cce7c8deae10bb3abdf5dc0bf2928fc2ac4f9b69c31b49750f9ae53b37c2',
 'Ironbridge/goalRivalGateNorth': '491a8539b37c038ee21a0fcd1da0302cbd81f123bc7a45c261f8aad62567967e',
 'Ironbridge/goalRivalGateSouth': '71934f5b7dcda9d497d820d6ba2db14ea78ba5fb384a82e4dc745d676f3b73ea',
 'Ironbridge/goalRivalKeepEntered': 'c8a6a7b846a94322725956a591eaaa6e04106618e610e2b7d5149b0c053050e5',
 'Ironbridge/keepReached': '52168c7a587567af570b8b1ad537643c3516be73a46144e3986c89c26559f148',
 'Ironbridge/rivalFalls': '05b5e72367cf0376729d09cfb8ed22f46c191cc0c0b936db820e8cbf975aa7d5',
 'Ironbridge/rivalHeartDown': '2a99324afebb946debdf455a762b0a3397ab53239aff6aa5c15a82d7ca0af1e6',
 'Ironbridge/yardReached': '9baa954870ffac33927dc1717bf9910ed278b7f82a585cb92216a909f587fba6',
 'Kingsfall/collectWin': '248ae73a6216d7d671e187040748d230b66ca878354543dcd4139d5ead708d53',
 'Kingsfall/courtTaken': '71e02274670b0b0e5cf5cf93d4ec3f43036487e9d3972f95314d7d52de10cb1d',
 'Kingsfall/goalArmyAtHeart': '7c8a4ba84f0d124e4800c8880845249ba6320cf154888bea68bebff9d7718d8e',
 'Lanternhill/goalRingGroundPathOpened': 'aa3d2a4caeaed837e5a0e80db31473d02090c14136c8df98937757f61389f0fc',
 'Lanternhill/rivalHeartDown': '2a99324afebb946debdf455a762b0a3397ab53239aff6aa5c15a82d7ca0af1e6',
 'Mirewatch/goalNineTowerRoomsClaimed': '84658f831313f77f8aac25816f828b1f3a0d8c541d2776dda77ccf139a4c215f',
 'Mirewatch/scoutTowerC': '47716bb4d4cf382a3c8361ed7ea924e83aec55ebe50a73512cced36d5cfa45b6',
 'Mirewatch/watcherSlain': '64247d08b33c60aecb9970aaa61cad974015e18496b97341fb463acf439130ca',
 'Mossgate/firstGold': '4bb67357526e67e6cebc76c5f68d3724d44496fa4fa0af3b661369e6a7822232',
 'Mossgate/goalWatchPostCleared': '43dd83a2b12c44e8787c1d4915efd8d681e068d24c09d2f6481c7997d2ada8eb',
 'Mossgate/hallOpened': '835d13b12817b58bb8a9342ce8e44ecc3fbc12ad6845c34392fefbf960ef93fc',
 'Mossgate/meadowOurs': 'dc24a3c61e63b2361437ecff16031829c4603c9cf3914b514de872c70ddb52e2',
 'Mossgate/reeveFalls': '64247d08b33c60aecb9970aaa61cad974015e18496b97341fb463acf439130ca',
 'Ravensledge/captainDown': '64247d08b33c60aecb9970aaa61cad974015e18496b97341fb463acf439130ca',
 'Ravensledge/goalInnerKeepEntered': '457a1424f47049320d2f987087e3ab9988b1f0b77713ad34334d036446c9556d',
 'Ravensledge/shelfHeld': 'f3b5f12fb09cd6590ec8fe5d13f74cd7956b27e0a3bb9524b0a49ad2782cbef1',
 'Ravensledge/yardEntered': '9f57851e1a5a3e3b77dca2af5a58970e607f4f71b73268c2872e00d8b9eac1db',
 'Saltmere/admiralFalls': 'c96e98ffdf3e6804c1011c44eb491c5380b0511c0097e4208292b74128f88dbd',
 'Saltmere/firstSalt': '44a33805891e8bde4816432f6122e61047c42e6a3206e750e684f084e0a8d625',
 'Saltmere/flatHeld': '013d7fc84c5e7c7c651d50bf9d20ed6116158743113badc7883dbbf4de2e0ce1',
 'Saltmere/goalHeartLost3': '65883912aaee2758fba63138f970f145b041ff9190c5a9b5c207577321079e3e',
 'Saltmere/goalHeartSafe3BeforeAdmiralFalls': '3539faa948aae498e63ded7b84f4a1a1b58d95f29a3f61a48e88eba6161317c2',
 'Saltmere/portGatesTimeout': '461dcfa3ab8be3d790feeaaf6488d364908584efbd2cb9c4a0f0d4bebf5dc769',
 'Silverdeep/goalSilverdeepAllSpans': '416b30b15daccc2cc8efa057efa6a346d0ff60d754b4050adf781186638dc5ec',
 'Silverdeep/goalSilverdeepSpan1Path1After': '694e3bb32a4dad662193dfeb38ade55123d23d1197188182da6280353895482c',
 'Silverdeep/goalSilverdeepSpan1Path1Before': '96b935a0794b0dc5b1ba4c55ca33a73790f881e813b8e9f2b64fe0b2d01a3b19',
 'Silverdeep/goalSilverdeepSpan1Path1On': '4ab50e2d69d4786c084d5716621cffeef8b018ca74e427e6c72e03b7f6c46bcf',
 'Silverdeep/goalSilverdeepSpan2Path1After': '0645a6ec87e0a448b9d2f5e5febea25308d57e5f446587f7717a79f4449017b4',
 'Silverdeep/goalSilverdeepSpan2Path1Before': '56d1c9f369d900a668f10f5fc73772ae42a621cefed12d89940d945705ddc604',
 'Silverdeep/goalSilverdeepSpan2Path1On': '20b9ed0914187570fe2c524aa2c798840159cbb0ae9d9dcf5ff67ef46b0675b7',
 'Silverdeep/goalSilverdeepSpan3Path1After': '90da92e9fa34127a8999e8160db1843651f264d6c8aa28c8fcc78b7a816b4f9e',
 'Silverdeep/goalSilverdeepSpan3Path1Before': 'a43faad731e81bf48223b03c4b0ab98d5b4aa076a9d31e5107ec95393aa1e428',
 'Silverdeep/goalSilverdeepSpan3Path1On': '5e37dc7725947912515515e5458a9a5f55b3ba082d73fa723bdd0bffabbaf885',
 'Silverdeep/silverWonGoal': '2a99324afebb946debdf455a762b0a3397ab53239aff6aa5c15a82d7ca0af1e6',
 'SkittleCavern/laneCleared': '34ca017e8ab755fe28f0a2da2aa7a24fc0fb75e59cad21f923a1aa3631fbf231',
 'Stormhaven/goalPactFormedDockEntered': 'd084ee2784204893d1414846b867a0379d4044c69495f226778d2b376fb970d5',
 'Stormhaven/goalPactFormedQuayEntered': '87230c6693f552c4505a78931b3b8c58a0b780f005e81cc23ffe20ec5655f4ed',
 'Stormhaven/goalPactFormedTideTurns': '991c221aa0af6dfc67b1113a43cf55140d476ecd1334a6ad973f9b5ed1e63258',
 'Stormhaven/goalPactFormedTideTurns2': '347ef5b3961513b41663a471c08ee3acf0f256ad1175b84b112f1cce96751883',
 'Stormhaven/goalPactsPrevented': 'c49c9154600141018a22f986cc085433756316fc81d6618b1efa8d80690df33c',
 'Stormhaven/goalStormhavenBothSpans': '5f8a0e62609158e5664ac6198c877559380765760e138518a29639bfd98fc3a4',
 'Stormhaven/goalStormhavenSpan1After': 'f0a4d6d018914b54708f42588df5157eaab85efba18c4b9e05110101b9188ba9',
 'Stormhaven/goalStormhavenSpan1Before': '7869cf5e406b2a3a9ccaadf3bd98292b4dce4b58c4a506581fccb3205a7e34b3',
 'Stormhaven/goalStormhavenSpan1On': 'db5c986a56c96dbd0ad1a29010db13c624abc5d2f96b839751d857897e22991e',
 'Stormhaven/goalStormhavenSpan2After': '6ab2caa5531335587661040cd481fad096de9254ed2296efcb9b826dffb1c7a1',
 'Stormhaven/goalStormhavenSpan2Before': 'e921a356dfc1d75bf0939271c6eafaaf0b943cb6df262fbcd8fde5be651fc2dc',
 'Stormhaven/goalStormhavenSpan2On': 'd2fdfa00e0cd4c5de50ade73fd7065e6097ed95578b7fde1fab8023ec39e2c4a',
 'Stormhaven/stormEndsGoal': '2a99324afebb946debdf455a762b0a3397ab53239aff6aa5c15a82d7ca0af1e6',
 'SwarmNight/dawn': '2335939255b4fd381eca7f23b78c9b0d57725213bced5336b88fd8d105dfaf0e',
 'SwarmNight/goalHeartLost1': '65883912aaee2758fba63138f970f145b041ff9190c5a9b5c207577321079e3e',
 'SwarmNight/goalHeartLost2': '65883912aaee2758fba63138f970f145b041ff9190c5a9b5c207577321079e3e',
 'SwarmNight/goalHeartSafe1BeforeDawn': 'f6d3c776f37846a45c747396602ae53855e4ec5b9e8b52b8520d3df2aa7941a1',
 'SwarmNight/goalHeartSafe2BeforeDawn': 'f6d3c776f37846a45c747396602ae53855e4ec5b9e8b52b8520d3df2aa7941a1',
 'Thornreach/goalHeartLost3': '65883912aaee2758fba63138f970f145b041ff9190c5a9b5c207577321079e3e',
 'Thornreach/goalHeartSafe3BeforeKeepersGoneGoal': 'f70d95a26645ff48ff76d89a9f3b38d12dd2439cd3004c3c25249ef2caf53e5d',
 'Thornreach/keepersGoneGoal': '2a99324afebb946debdf455a762b0a3397ab53239aff6aa5c15a82d7ca0af1e6',
 'Tinmoor/goalPostACleared': '35637c4d1096ff9d216ab391a899d54ab046ff480f75b4e3f1bda65df5562634',
 'Tinmoor/goalPostBCleared': 'db69c69aa8f511f80d586503f111bea43cd33474fd295d4d2a80f353790f1df1',
 'Tinmoor/postACleared': 'dac3217efa10c0755d25613c88742aee86e44f9f96087c3fe996ec4d3e389fad',
 'Tinmoor/scoutSeams': 'b0046d533f6e970dbd28d295d8c86f436eafb42379ac010acd97b4cbe7587ac2',
 'Tinmoor/seamsFilled': '9cf34e5e445fe8b72c19705c624d55970832076f065272e785544ee60a0d46ea',
 'Tinmoor/wardenDown': '64247d08b33c60aecb9970aaa61cad974015e18496b97341fb463acf439130ca',
 'TwistingHalls/hallReached': '9e2ac4d840cc4b2cd6b37ea9260f16250976ae7ea3e0d5ae9563c5387778f0f9',
 'VarnsCrossing/crossingHeldGoal': '2a99324afebb946debdf455a762b0a3397ab53239aff6aa5c15a82d7ca0af1e6',
 'VarnsCrossing/goalHeartLost2': '65883912aaee2758fba63138f970f145b041ff9190c5a9b5c207577321079e3e',
 'VarnsCrossing/goalHeartSafe2BeforeCrossingHeldGoal': 'f70d95a26645ff48ff76d89a9f3b38d12dd2439cd3004c3c25249ef2caf53e5d',
 'VarnsCrossing/goalQuarterThenFortress': '47574e68327b7f3c0a12d22636d5edf5baa3f388f4fde19dc086d24f648961d2',
 'Wolfscar/goalHeartLost3': '65883912aaee2758fba63138f970f145b041ff9190c5a9b5c207577321079e3e',
 'Wolfscar/goalHeartSafe3BeforeHuntersGoneGoal': 'f70d95a26645ff48ff76d89a9f3b38d12dd2439cd3004c3c25249ef2caf53e5d',
 'Wolfscar/huntersGone': '817957baa91166588c3cf255cb0a0c3b1337b587953edcab61f720389de35680',
 'Wolfscar/huntersGoneGoal': '2a99324afebb946debdf455a762b0a3397ab53239aff6aa5c15a82d7ca0af1e6',
 'Wolfscar/southCrossed': 'a8f87a6499f3f4999683baba47d5c312ff9e89caf3a9af1f46bd762a6510941b',
 'Wraithwood/bothFallGoal': '2a99324afebb946debdf455a762b0a3397ab53239aff6aa5c15a82d7ca0af1e6',
 'Wraithwood/goalHeartLost3': '65883912aaee2758fba63138f970f145b041ff9190c5a9b5c207577321079e3e',
 'Wraithwood/goalHeartSafe3BeforeBothFallGoal': 'f70d95a26645ff48ff76d89a9f3b38d12dd2439cd3004c3c25249ef2caf53e5d',
 'Cinderhollow/goalHeartSafe1BeforeRivalHeartFalls': '72261b6e1a897402c27c01d339a576268aa58939eb72e41de9c88b8e29c2f812',
 'Cinderhollow/goalHeartSafe1BeforeRivalHeartDown': 'b6d7002c1fcffaa18185d177955b81f6f612ba6e22c7356b448792085c34f3ec',
 'Cinderhollow/goalHeartLost1': '65883912aaee2758fba63138f970f145b041ff9190c5a9b5c207577321079e3e',
 'Lanternhill/goalHeartSafe3BeforeRivalHeartDown': 'f70d95a26645ff48ff76d89a9f3b38d12dd2439cd3004c3c25249ef2caf53e5d',
 'Lanternhill/goalHeartLost3': '65883912aaee2758fba63138f970f145b041ff9190c5a9b5c207577321079e3e'}

# Fixed original enemy-room footprints: name, type, original owner, inclusive rectangle, tile count.
MIREWATCH_ROOMS = (('GuardRoom1', 17, 3, (44, 106, 47, 107), 8),
 ('GuardRoom2', 17, 3, (86, 110, 88, 111), 6),
 ('Library1', 7, 3, (24, 78, 26, 79), 6),
 ('Arena1', 14, 3, (74, 82, 78, 84), 15),
 ('Library2', 7, 3, (20, 44, 22, 45), 6),
 ('Prison1', 11, 3, (68, 50, 69, 52), 6),
 ('Workshop1', 5, 3, (44, 24, 47, 25), 8),
 ('GuardRoom3', 17, 2, (94, 22, 97, 23), 8),
 ('Workshop2', 5, 2, (99, 18, 101, 19), 6))
CINDER_SEAMS = ((5, 55, 6, 58), (30, 72, 33, 73))
LANTERN_CONNECTION = (13, 57, 40, 22)


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
        source_rows = [row for _, row in lines(block)]
        seat_fields = ("seatId", "teamId", "player", "faction", "startingX", "startingY",
                       "colorId", "gold", "goldMined", "mana")
        if tuple(row.split()[0] for row in source_rows[:len(seat_fields)]) != seat_fields:
            raise ValueError("seat fields do not follow Seat::importSeatFromStream order")
        if source_rows[len(seat_fields)] != "[SkillDone]":
            raise ValueError("seat lacks initial SkillDone section")
        fields = dict(line.split(None, 1) for _, line in lines(block) if not line.startswith("[") and len(line.split(None, 1)) == 2)
        required = ("seatId", "teamId", "player", "faction", "startingX", "startingY")
        if any(key not in fields for key in required):
            raise ValueError("seat lacks required fields")
        seat = int(fields["seatId"])
        if seat <= 0 or seat in seats or any(int(team) <= 0 for team in fields["teamId"].split("/")):
            raise ValueError("duplicate/invalid seat or team")
        seats[seat] = fields
        for key in ("startingX", "startingY", "colorId", "gold", "goldMined"):
            int(fields[key])
        if not math.isfinite(float(fields["mana"])):
            raise ValueError("non-finite seat mana")
    humans = [seat for seat, fields in seats.items() if fields["player"] == "Human"]
    if len(humans) != 1:
        raise ValueError("expected exactly one human seat")
    tile_lines = lines(section(text, "Tiles"))
    width, height = int(tile_lines[0][1]), int(tile_lines[1][1])
    if width <= 0 or height <= 0:
        raise ValueError("invalid map dimensions")
    if any(not 0 <= int(fields["startingX"]) < width or not 0 <= int(fields["startingY"]) < height
           for seat, fields in seats.items() if seat != 0):
        raise ValueError("seat starting position is outside map")
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
        if parts[:2] in (["Cond", "goal"], ["Cond", "goalmet"]):
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


# Keep the approved finite seams fixed, so moved regions cannot silently weaken completion.
def cinder_seam_errors(level):
    errors = []
    for name, rect in zip(("GoalWestGoldSeam", "GoalSouthGoldSeam"), CINDER_SEAMS):
        fields = tuple(map(str, rect))
        expected = "Region\t" + name + "\t" + "\t".join(fields)
        if expected not in level["text"].splitlines():
            errors.append("approved gold seam region differs: " + name)
        x1, y1, x2, y2 = rect
        for x in range(x1, x2 + 1):
            for y in range(y1, y2 + 1):
                if level["tiles"].get((x, y)) != (2, 100.0, None):
                    errors.append("approved finite full-gold tile differs: %d,%d" % (x, y))
    return errors


# Check every fixed tower room tile for eventual worker access and enemy-room claimability.
def mirewatch_errors(level):
    errors = []
    configured_modes = re.findall(r"^\s*RoomsClaimableByEnemies\s+(\S+)", (ROOT / "config/rooms.cfg").read_text(encoding="utf-8"), re.M)
    if configured_modes != ["1"]:
        errors.append("room takeover configuration differs from reviewed claimable mode 1")
    area, _ = reachable(timed_openings(level))
    human = level["human"]
    actual = {name: (kind, owner, set(tiles)) for kind, name, owner, tiles in level["rooms"]}
    total = 0
    for name, kind, owner, (x1, y1, x2, y2), count in MIREWATCH_ROOMS:
        expected = {(x, y) for x in range(x1, x2 + 1) for y in range(y1, y2 + 1)}
        total += len(expected)
        if len(expected) != count or actual.get(name) != (kind, owner, expected):
            errors.append("tower room original footprint/type/owner differs: " + name)
        room = actual.get(name)
        for tile in sorted(expected):
            if tile not in area:
                errors.append("tower room tile unreachable: %s %s" % (name, tile))
            ground = level["tiles"].get(tile)
            # RoomClaim rejects allied rooms and temples; the reviewed global mode above is claimable.
            if (ground is None or ground[1] != 0 or room is None or room[0] == 1
                    or level["seats"][room[1]]["teamId"] == level["seats"][human]["teamId"]):
                errors.append("tower room tile not worker-claimable: %s %s" % (name, tile))
    if total != 69:
        errors.append("expected exactly 69 fixed tower room tiles")
    return errors


# Validate explicit objective identities and only the individually mapped completion triggers.
def objective_errors(text, level_name):
    declarations, states, errors = {}, {}, []
    trigger_names = set(re.findall(r"^Name\t([^\r\n]+)", section(text, "Triggers"), re.M)) if "[Triggers]" in text else set()
    region_names = set(re.findall(r"^Region\t(\S+)", text, re.M))
    conditions_to_check = []
    for block in re.findall(r"\[Trigger\](.*?)\[/Trigger\]", text, re.S):
        rows = lines(block)
        trigger = next((row.split(None, 1)[1] for _, row in rows if row.startswith("Name\t")), "")
        conditions = "\n".join(row for _, row in rows if row.startswith("Cond\t"))
        fingerprint = hashlib.sha256(conditions.encode("utf-8")).hexdigest()
        has_state = any(row.startswith("Action\tobjectivestate\t") for _, row in rows)
        if (has_state or trigger.startswith("goal")) and OBJECTIVE_CONDITIONS.get(level_name + "/" + trigger) != fingerprint:
            errors.append("objective status trigger conditions differ from reviewed conditions: " + trigger)
        conditions_to_check.extend((trigger, row.split()) for _, row in rows if row.startswith("Cond\t"))
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
    for trigger, fields in conditions_to_check:
        if fields[:2] in (["Cond", "minedregion"], ["Cond", "killedin"]):
            expected_size = 6 if fields[1] == "minedregion" else 4
            if len(fields) != expected_size or fields[3] not in region_names:
                errors.append("unknown presentation region or malformed regional history in " + trigger)
        if fields[:2] == ["Cond", "trigger"]:
            if len(fields) not in (3, 4) or fields[2] not in trigger_names or len(fields) == 4 and fields[3] not in ("0", "1"):
                errors.append("unknown trigger-history reference in " + trigger)
        if fields[:2] == ["Cond", "objective"]:
            if len(fields) != 5 or fields[4] not in ("open", "reached", "failed") or not any(
                    key[1] == fields[3] and key[0] in (fields[2], "-1") for key in declarations):
                errors.append("unknown objective-state reference in " + trigger)
        if fields[:2] == ["Cond", "trapgone"]:
            traps = section(text, "Traps")
            if len(fields) != 3 or not re.search(r"^\d+\t" + re.escape(fields[2]) + r"\t", traps, re.M):
                errors.append("trapgone has no initial named trap in " + trigger)
        if fields[:2] == ["Cond", "spawncleared"]:
            source = next((block for block in re.findall(r"\[Trigger\](.*?)\[/Trigger\]", text, re.S)
                           if re.search(r"^Name\t" + re.escape(fields[2]) + r"$", block, re.M)), None) if len(fields) >= 4 else None
            actions = [row for _, row in lines(source or "") if row.startswith("Action\t")]
            index = int(fields[3]) if len(fields) >= 4 and fields[3].isdigit() else -1
            if index < 0 or index >= len(actions) or not actions[index].startswith("Action\tspawn\t"):
                errors.append("spawncleared does not reference an actual original spawn action in " + trigger)
            if len(fields) not in (4, 6) or len(fields) == 6 and (
                    fields[4] != "atheart" or not fields[5].isdigit() or int(fields[5]) <= 0):
                errors.append("invalid spawncleared death-location restriction in " + trigger)
    for key, description in declarations.items():
        identity = re.fullmatch(re.escape(level_name.lower()) + r"-o(\d{2})", key[1])
        index = int(identity[1]) if identity else 0
        if index < 1 or key[1] in HINT_IDS:
            errors.append("objective ID differs from fixed inventory: " + key[1])
        completions = {trigger for trigger, state in states.get(key, []) if state == "reached"}
        if not completions:
            errors.append("objective has no completing trigger: %s: %s" % (key[1], description))
        expected = OBJECTIVE_TRIGGERS.get(level_name, [])
        if index < 1 or index > len(expected) or completions != set(expected[index - 1] or ()):
            errors.append("objective completion differs from reviewed trigger mapping: " + key[1])
        if key[1] in OBJECTIVE_COVERAGE_PENDING:
            errors.append("objective partially covered: " + key[1] + ": " + OBJECTIVE_COVERAGE_PENDING[key[1]])
    for index in range(1, len(OBJECTIVE_TRIGGERS.get(level_name, ())) + 1):
        identity = level_name.lower() + "-o%02d" % index
        if identity not in HINT_IDS and not any(key[1] == identity for key in declarations):
            errors.append("required stable objective ID is missing: " + identity)
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
            if parts[:2] == ["Action", "hint"]:
                result.append("| Advisory hint line %d | `%s` | not an objective; no state required |" % (number, cell(line)))
            elif len(parts) >= 2 and (parts[:2] in (["Action", "objective"], ["Action", "objectiveid"], ["Action", "win"], ["Cond", "goal"])):
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
                index = int(fields[3].rsplit("-o", 1)[1])
                if OBJECTIVE_TRIGGERS[path.stem][index - 1] and fields[3] not in OBJECTIVE_COVERAGE_PENDING:
                    continue
                candidates, missing, question = review
                candidate_conditions = "; ".join(candidate + ": " + trigger_conditions.get(candidate, "no matching existing trigger")
                                                 for candidate in candidates.split(", "))
                result.append("| Open requirement `%s`: %s | Candidates/conditions: `%s`; missing: %s Proposal/question: %s | open |" %
                              (fields[3], cell(fields[4]), cell(candidate_conditions), cell(missing), cell(question)))
        if entry:
            sentences = re.split(r"(?<=[.!?])\s+", entry.get("Briefing", "").replace("\\n", " "))
            for index, match in BRIEFING_MATCH[path.stem].items():
                result.append("| Briefing | " + cell(sentences[index - 1].strip()) + " | " + match + " |")
        if path.stem == "Cinderhollow":
            result.append("Mario's explicit criterion for cinderhollow-o02: fully mine all 16 original finite gold fields x5..6/y55..58 and x30..33/y72..73. No deadline or first-wave failure applies; this approved criterion overrides the unchanged objective prose mentioning the first wave.")
        elif path.stem == "Lanternhill":
            result.append("lanternhill-o02 requires an actual current dry worker path from (13,57) to (40,22), including legal alternative digging routes; no fixed plug subset is required.")
        elif path.stem == "Mirewatch":
            result.append("mirewatch-o03 requires complete worker claim and current room ownership on the nine original room masks, exactly 69 fields; broad tower regions and visits do not qualify.")
            for room_name, kind, owner, rect, count in MIREWATCH_ROOMS:
                result.append("Original room `%s`: type %d, owner %d, rectangle %s, %d fields." % (room_name, kind, owner, rect, count))
            for error in mirewatch_errors(parse_level(text)):
                result.append("Static tower validation failure: " + error)
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
    wrong_spawn = original.replace("spawncleared\tpostStirs\t1", "spawncleared\tpostStirs\t0")
    assert any("actual original spawn" in error for error in objective_errors(wrong_spawn, "Mossgate"))
    ravensledge = (root / "levels/campaign/Ravensledge.level").read_text(encoding="utf-8")
    wrong_trap = ravensledge.replace("trapgone\tDoorWooden1", "trapgone\tNoSuchDoor")
    assert any("no initial named trap" in error for error in objective_errors(wrong_trap, "Ravensledge"))
    bellwick = (root / "levels/campaign/Bellwick.level").read_text(encoding="utf-8")
    assert not objective_errors(bellwick, "Bellwick"), "advisory hint must not renumber later IDs"
    assert "objectiveid\t1\tbellwick-o03\t" in bellwick
    assert "objectiveid\t1\tbellwick-o02\t" not in bellwick
    assert "Action\thint\t1\tHold the nave against the faithful." in bellwick
    legacy = (root / "levels/campaign/Campaign1.level").read_text(encoding="utf-8")
    assert not objective_errors(legacy, "Campaign1"), "broadcast declarations must keep matching status seats"
    kingsfall = (root / "levels/campaign/Kingsfall.level").read_text(encoding="utf-8")
    assert not objective_errors(kingsfall, "Kingsfall"), "army goal must use its exact temple location"
    assert "Cond\tcreaturesatheart\t1\t>=\t12" in kingsfall
    coldwell = (root / "levels/campaign/Coldwell.level").read_text(encoding="utf-8")
    assert not objective_errors(coldwell, "Coldwell"), "under-plaza mining must have an exact source"
    wrong_source = coldwell.replace("minedregion\t1\tUndercroft", "minedregion\t1\tMissingRegion")
    assert any("unknown presentation region" in error for error in objective_errors(wrong_source, "Coldwell"))
    cinder = parse_level((root / "levels/campaign/Cinderhollow.level").read_text(encoding="utf-8"))
    assert not cinder_seam_errors(cinder)
    cinder["tiles"][5, 55] = (6, 100.0, None)
    assert cinder_seam_errors(cinder), "infinite gem cannot replace the approved finite gold"
    mire_text = (root / "levels/campaign/Mirewatch.level").read_text(encoding="utf-8")
    mire = parse_level(mire_text)
    assert not mirewatch_errors(mire), "all 69 original tower room fields must be reachable and claimable"
    for mutation in ("sealed", "full", "allied", "missing"):
        bad = parse_level(mire_text)
        target = (44, 106)
        if mutation == "sealed":
            for tile in neighbours(target):
                bad["tiles"][tile] = (3, 100.0, None)
        elif mutation == "full":
            bad["tiles"][target] = (1, 100.0, None)
        elif mutation == "allied":
            bad["seats"][3]["teamId"] = bad["seats"][1]["teamId"]
        else:
            bad["rooms"] = [room for room in bad["rooms"] if room[1] != "GuardRoom1"]
        errors = mirewatch_errors(bad)
        assert errors, "required room mutation must fail: " + mutation
        if mutation == "sealed":
            assert any("tile unreachable: GuardRoom1 (44, 106)" in error for error in errors)
        if mutation == "full":
            assert any("not worker-claimable: GuardRoom1 (44, 106)" in error for error in errors)
    for filename, needle, replacement in (
            ("Cinderhollow", "slabs\t-1\tGoalWestGoldSeam\tgold\t==\t0", "slabs\t-1\tGoalWestGoldSeam\tgold\t>=\t0"),
            ("Lanternhill", "groundconnected\t1\t13\t57\t40\t22", "groundconnected\t1\t13\t57\t39\t22"),
            ("Mirewatch", "roomclaimed\t1\t44\t106\t47\t107", "roomclaimed\t1\t44\t106\t46\t107")):
        original_map = (root / ("levels/campaign/" + filename + ".level")).read_text(encoding="utf-8")
        assert needle in original_map
        assert any("conditions differ" in error for error in objective_errors(original_map.replace(needle, replacement), filename))
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
            if path_level.stem == "Cinderhollow":
                failures.extend(name + ": " + error for error in cinder_seam_errors(level))
            if path_level.stem == "Mirewatch":
                failures.extend(name + ": " + error for error in mirewatch_errors(level))
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
