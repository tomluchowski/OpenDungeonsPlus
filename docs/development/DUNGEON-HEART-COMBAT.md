# Dungeon heart combat

September 22 acceptance update: after the rejected-click follow-up was deployed,
the user reported successful in-game retests. This supersedes the pending user
retest status in the historical entries below; Windows-blocked automated probes
remain explicitly unverified, and no agent-run game test is claimed.

The existing room exposes every floor tile as a combat target and stores damage
per tile. Its persistent central object has no health or attackability; the room
can also be sold. The requested behaviour is an enemy-only attackable heart,
with floor tiles protected from demolition and direct damage.

Reuse the existing persistent object and room lifecycle: a server-side heart
object delegates to one room-owned health pool and participates in visible enemy
target discovery. The ordinary room target/damage and sale paths reject the
heart's floor. Actual heart death releases the room through the existing cleanup
and last-temple defeat paths. Editor tile removal remains available.

The heart's health is its own pool with a fixed maximum of 10000
(`RoomDungeonTemple::HEART_MAX_HP`), whatever the number of tiles of the room. It
no longer comes from the floor tiles' durability (9 tiles of 10, so 90 in all,
which one strong blow destroyed while the ring barely moved). The top-left ring
shows the remaining health divided by this maximum. Only fighters damage an
enemy heart: `takeHeartDamage` ignores worker creatures, and a worker given a
non-creature target drops its fight action. A tagged optional room record saves
the remaining heart health as `HeartHealth10000`; files without it load with an
undamaged heart. Older saves are converted by share, so a full heart stays full
and a ruin stays a ruin: the record `HeartHealth` (10000 per room tile, 90000
for a 3 by 3 heart) is divided by that old maximum, and the record `HeartHP`,
which measured the health against the floor durability, by the durability
(`HeartHP 90` of 90 becomes 10000, `HeartHP 0` stays a ruin).
A living heart heals 2.5 per second (`HEART_HEAL_PER_SECOND`, added as
`2.5 / turnsPerSecond` in every `doUpkeep`) up to the maximum; a destroyed heart
(0) never heals. Unrelated resource rewards are not part of it.

## Verification

### Construction footprint follow-up

The September 22 user log records a treasury built at (56,102), while the
heart room covers (57..59,101..103), centred at (58,102). The shipped heart
mesh extends 1.73308 units from its centre, overlapping the supposedly free
adjacent tile. The existing building flag correctly protects the nine room
tiles but does not represent this visible overhang. Reuse the measured bounds
in `RoomObjectBounds.h` in the shared tile construction validator, on both
client and server, without changing editor placement or creature navigation.
Treasury hover preview also needs to use that existing validator instead of
unconditionally offering construction. This completes the unmerged heart
protection feature on its existing branch; no numerical rebalance is involved.

The extracted production construction validator and treasury hover pass 250
checks (76 failures before the fix), including the logged overlapping tile,
all surrounding footprint edges, ordinary land, existing buildings, editor
placement and portal isolation. All 51 demolition-selection regressions pass.
The shared validator also gates trap placement and server room packets, so the
protection is not limited to the treasury preview. No header layout changed;
an incremental Release rebuild is sufficient for this follow-up.

The next user retest exposed treasury click feedback: its empty-selection path
reports a white zero-cost build message, which the game dispatch interprets as
success and uses to trigger the hammer. It returns without sending a build
packet; the latest user log contains no new room after loading. Replace this
success feedback with the existing failure path and extend the production
dispatch fixture to cover treasury clicks, not just hover and ordinary rooms.
All 745 input/dispatch checks now pass (five hammer failures before), together
with the 250 footprint checks and 32 resource checks. Release compilation and
hash-verified deployment pass; user click/animation retest remains pending.

The production-method heart fixture passes 46 checks, including the actual
server heart object, owner/allied/unknown rejection, independent damage,
single death notification, retained floor, editor removal and save/legacy-load
handling. The production sale-selection fixture passes 51 checks, including
preview, dragging and validated commands over protected heart tiles. All 361
temple-duplication regression checks also pass with the new object constructor.

The production enemy-discovery fixture compiles, but Windows application control
blocks execution with error 4551, both in the sandbox and in the approved
external run; it is not reported as passed. No security policy was changed.
The inherited movable-object footprint is its position tile, so normal combat
selection targets the heart's centre rather than its surrounding room floor.
The existing seat upkeep excludes rooms with zero health from temple counts,
feeding the existing last-temple defeat notification.

Clean Release compilation, hash-verified backup/deployment, runtime preparation
and all 32 resource checks pass; see [the build record](BUILDING.md).
User gameplay acceptance must
cover blocked demolition, enemy attacks on the heart itself, and defeat after
heart destruction; no manual game test is run by the agent.

This is a local feature without a release-version change; the development index
and build record contain its documentation, with no top-level README or release
changelog update needed. No remote publication is authorized.
