#ifndef ROOMOBJECTBOUNDS_H
#define ROOMOBJECTBOUNDS_H

#include <OgreVector2.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

namespace RoomObjectPath
{
// Factor that converts degrees to radians.
constexpr float degreesToRadians = 0.01745329252f;

// A creature grows by this fraction of its base size with every level.
constexpr float creatureScalePerLevel = 0.02f;

// Returns the size factor of a creature of the given level.
inline float creatureScale(float level)
{
    return 1.0f + creatureScalePerLevel * level;
}

struct MeshBounds
{
    const char* name;
    float minX, minY, maxX, maxY;
    float maxZ = std::numeric_limits<float>::infinity();
};

// Mesh-local XY bounds measured from the shipped room furniture, not tile
// centers. Keep these synchronized with assets using check_room_object_bounds.py.
static const MeshBounds meshBounds[] = {
    {"AdventurerBed", -.406797f, -.679228f, .406797f, .679228f, .087917f},
    {"Anvil", -.335981f, -.280187f, .341827f, .231204f},
    {"Bed", -.337495f, -.681989f, .344794f, .702338f, .686024f},
    {"Bookcase", -.1694f, -.1694f, .1694f, .1694f},
    {"Bookshelf", -.52309f, -.0069419f, .530155f, .428917f},
    {"CasinoPokerTable", -.287885f, -.249315f, .287885f, .249315f},
    {"CasinoWallBeer", -.502785f, .325887f, .502785f, .493482f},
    {"CelticCross", -.391612f, -.28f, .391607f, .28f},
    {"ChickenCoop", -.203275f, -.4f, .796725f, .4f},
    {"Chimney", -.63f, .0721364f, .63f, .583136f},
    {"DragonBed", -.941213f, -.96306f, .886655f, .969738f, .306282f},
    {"DungeonTempleObject", -1.73308f, -1.73308f, 1.73308f, 1.73308f},
    {"FenceCorner", -.502486f, -.500121f, .521221f, .514487f},
    {"FenceStraight", -.493929f, -.492016f, .516277f, -.462466f},
    {"GoblinBed", -.450071f, -.436337f, .444606f, .44576f, .073802f},
    {"GoldstackLv1", -.197478f, -.175391f, .186379f, .185685f},
    {"GoldstackLv2", -.185184f, -.182552f, .185553f, .182552f},
    {"GoldstackLv3", -.331379f, -.301391f, .290679f, .182552f},
    {"GoldstackLv4", -.331379f, -.301391f, .290679f, .282221f},
    {"Grindstone", -.329974f, -.215496f, .329974f, .371096f},
    {"ImpBed", -.42f, -.42f, .42f, .42f},
    {"KnightCoffin", -.254848f, -.627449f, .517051f, .632155f, .841464f},
    {"KnightStatue", -.293943f, -.32853f, .295722f, .325329f},
    {"KnightStatue2", -.323145f, -.358743f, .332038f, .367767f},
    {"LizardmanBed", -.32835f, -.77042f, .328337f, .769587f, .952407f},
    {"OrcBed", -.533745f, -.997136f, .529433f, .987434f, .560469f},
    {"Podium", -.193612f, -.148315f, .193891f, .148315f},
    {"PortalObject", -1.66302f, -1.66302f, 1.66302f, 1.66302f},
    {"RangerBed", -.53459f, -.777014f, .534418f, .775852f, .805918f},
    {"Roulette", -.292486f, -.296162f, .292326f, .296652f},
    {"Skull", -.205408f, -.216924f, .205408f, .460773f},
    {"SpiderBed", -.342277f, -.52519f, .458527f, .328312f},
    {"StoneCoffin", -.425477f, -.693722f, .425477f, .693722f, .767922f},
    {"TentacleBed", -.448981f, -.476366f, .461978f, .436628f, .276672f},
    {"TortureObject", -.7007f, -.606824f, .7007f, .606824f},
    {"TrainingDummy1", -.248242f, -.128432f, .248296f, .330221f},
    {"TrainingDummy2", -.455f, -.170625f, .455f, .4095f},
    {"TrainingDummy3", -.263341f, -.228943f, .30858f, .121115f},
    {"TrainingDummy4", -.462458f, -.248657f, .462458f, .248657f},
    {"TrollBed", -.787227f, -.794584f, .954702f, .813162f, 1.28522f},
    {"WorkshopMachine1", -.5376f, -.506479f, .761215f, .337512f},
    {"WorkshopMachine2", -.730786f, -.336f, .5376f, .338177f}
};

// Narrow the visible furniture and its navigation bounds together. Leave Z
// unchanged so authored working heights and bed support surfaces stay valid.
// Large beds keep their existing multi-tile allocation; this is not a capacity
// or placement rule. Already compact meshes are never enlarged.
struct FurnitureScale
{
    float x, y;
};

// Footprint (in tiles) that furniture is narrowed to. Beds that are not assigned
// to a creature use the smaller one, large beds the larger one.
constexpr float furnitureFootprint = 0.6f;
constexpr float unassignedBedFootprint = 0.4f;
constexpr float largeBedFootprint = 1.2f;
// Longest diagonal (in tiles) of a treasury pile.
constexpr float treasuryPileDiagonal = 0.4f;

// Returns how much the mesh is narrowed so that it fits the footprint of its kind.
inline FurnitureScale furnitureScale(const MeshBounds& bounds)
{
    const std::string name(bounds.name);
    if(name == "FenceCorner" || name == "FenceStraight" ||
       name == "PortalObject" || name == "DungeonTempleObject")
        return {1.0f, 1.0f};
    // Treasury piles can fill adjacent tiles at arbitrary angles. Bound their
    // diagonal, not just their unrotated width, so rotation retains the margin.
    if(name.compare(0, 9, "Goldstack") == 0)
    {
        const float scale = std::min(1.0f, treasuryPileDiagonal / std::hypot(
            bounds.maxX - bounds.minX, bounds.maxY - bounds.minY));
        return {scale, scale};
    }
    float width = furnitureFootprint, depth = furnitureFootprint;
    const bool bed = name == "Bed" || name == "KnightCoffin" || name == "StoneCoffin" ||
        (name.size() >= 3 && name.compare(name.size() - 3, 3, "Bed") == 0);
    // This fallback also covers unassigned decorative instances. Actual beds
    // override it with the owning creature's allocated dimensions below.
    if(bed)
        width = depth = unassignedBedFootprint;
    if(name == "DragonBed" || name == "TrollBed")
        width = depth = largeBedFootprint;
    else if(name == "Bed" || name == "KnightCoffin" || name == "LizardmanBed" ||
            name == "OrcBed" || name == "RangerBed" || name == "StoneCoffin")
        depth = largeBedFootprint;
    const float x = std::min(1.0f, width / (bounds.maxX - bounds.minX));
    const float y = std::min(1.0f, depth / (bounds.maxY - bounds.minY));
    if(bed)
        return {x, y};
    const float scale = std::min(x, y);
    return {scale, scale};
}

// Returns the scale at which a piece of furniture is shown and navigated: the one
// stored with the placed object, or the default of its mesh if none was stored (zero).
inline FurnitureScale placedFurnitureScale(const MeshBounds& bounds, const Ogre::Vector2& stored)
{
    if(stored == Ogre::Vector2::ZERO)
        return furnitureScale(bounds);
    return {stored.x, stored.y};
}

// Position of the mesh origin, angle in degrees and scale of a bed.
struct BedPlacement
{
    float x, y, angle;
    FurnitureScale scale;
};

// FNV-1a hash parameters, used to derive a stable angle from the creature name.
constexpr std::uint32_t bedHashBasis = 2166136261u;
constexpr std::uint32_t bedHashPrime = 16777619u;
// The angle of a bed differs from the allocated angle by up to bedAngleJitter degrees,
// in bedAngleSteps equal steps.
constexpr std::uint32_t bedAngleSteps = 8001u;
constexpr float bedAngleStep = 0.001f;
constexpr float bedAngleJitter = 4.0f;
// Part of the allocated width and depth that the rotated bed covers; the rest stays free.
constexpr float bedFootprintShare = 0.70f;
// Start values for the search of the corner of the rotated bed.
constexpr float bedCornerSentinel = 1.0e10f;

// Returns where a bed for the given creature is placed in the allocated area of
// width x height tiles whose lower left tile is (x, y); the angle is the allocated one.

inline BedPlacement bedPlacement(const MeshBounds& bounds, int x, int y,
    int width, int height, float allocationAngle, const std::string& creatureName)
{
    // Stable across save/load and platforms, without consuming gameplay RNG.
    std::uint32_t hash = bedHashBasis;
    for(unsigned char character : creatureName)
        hash = (hash ^ character) * bedHashPrime;
    const float angle = allocationAngle + float(hash % bedAngleSteps) * bedAngleStep - bedAngleJitter;
    const float radians = angle * degreesToRadians;
    const float cosine = std::cos(radians), sine = std::sin(radians);
    // Fit the rotated footprint, keeping the right and bottom 30% lanes clear.
    const float c = std::abs(cosine), s = std::abs(sine);
    const float determinant = c * c - s * s;
    const FurnitureScale scale{bedFootprintShare * (width * c - height * s) /
            (determinant * (bounds.maxX - bounds.minX)),
        bedFootprintShare * (height * c - width * s) /
            (determinant * (bounds.maxY - bounds.minY))};
    float left = bedCornerSentinel, top = -bedCornerSentinel;
    for(float px : {bounds.minX * scale.x, bounds.maxX * scale.x})
        for(float py : {bounds.minY * scale.y, bounds.maxY * scale.y})
        {
            left = std::min(left, cosine * px - sine * py);
            top = std::max(top, sine * px + cosine * py);
        }
    return {float(x) - 0.5f - left, float(y + height) - 0.5f - top, angle, scale};
}

// Walking body of a creature mesh: radius of the circle around it and its XY bounds.
struct WalkingRadius
{
    const char* name;
    float radius;
    float minX, minY, maxX, maxY;
};

// Maximum XY distance of skinned vertices over the authored Walk cycle,
// rounded upward with a small interpolation margin; never use bind-pose arms.
static const WalkingRadius walkingRadii[] = {
    {"Adventurer.mesh", .32f, -.223013f, -.303778f, .219754f, .241574f},
    {"CaveHornet.mesh", .34f, -.336557f, -.204048f, .336531f, .223639f},
    {"Cultist.mesh", .36f, -.290157f, -.345677f, .290157f, .285324f},
    {"DarkElf.mesh", .30f, -.193203f, -.17552f, .139327f, .305f},
    {"Defender.mesh", 1.08f, -.257927f, -1.0724f, .213f, .375236f},
    {"Dragon.mesh", .94f, -.742643f, -.907037f, .757226f, .858256f},
    {"Dwarf1.mesh", .27f, -.258449f, -.249296f, .25823f, .198673f},
    {"Dwarf2.mesh", .27f, -.253703f, -.248959f, .253375f, .198673f},
    {"Elf.mesh", .30f, -.193203f, -.17552f, .139327f, .305f},
    {"Gnome.mesh", .30f, -.257412f, -.196075f, .257413f, .202186f},
    {"Goblin.mesh", .39f, -.143098f, -.359099f, .26421f, .255691f},
    {"Knight.mesh", .42f, -.271715f, -.368724f, .276299f, .395971f},
    {"Kobold.mesh", .30f, -.215426f, -.202178f, .146432f, .277816f},
    {"Kreatur.mesh", .95f, -.72982f, -.685657f, .73037f, .600837f},
    {"LavaSpawn.mesh", 1.13f, -.803072f, -.770489f, .801402f, 1.12894f},
    {"Lizardman.mesh", .72f, -.327972f, -.372646f, .319483f, .717053f},
    {"Monk.mesh", .29f, -.195454f, -.275317f, .193061f, .190716f},
    {"NatureMonster.mesh", .59f, -.48582f, -.437184f, .48582f, .446317f},
    {"Orc.mesh", .37f, -.301586f, -.279845f, .300999f, .289291f},
    {"PitDemon.mesh", 1.33f, -1.32384f, -.477834f, 1.32384f, .627703f},
    {"Rat.mesh", .59f, -.137487f, -.47173f, .130219f, .580713f},
    {"Roach.mesh", .53f, -.289007f, -.445967f, .29347f, .453444f},
    {"RunelordDwarf.mesh", .42f, -.2958f, -.403943f, .304318f, .219047f},
    {"Scarab.mesh", .63f, -.580074f, -.289168f, .583679f, .382623f},
    {"Slime.mesh", .57f, -.139284f, -.564724f, .139272f, .205f},
    {"Spider.mesh", .46f, -.395196f, -.425697f, .395635f, .395744f},
    {"TentacleAlbine.mesh", .32f, -.240472f, -.304768f, .239782f, .294454f},
    {"TentacleGreen.mesh", .32f, -.240472f, -.304768f, .239782f, .294454f},
    {"Troll.mesh", .64f, -.626231f, -.488335f, .635458f, .318153f},
    {"Wizard.mesh", .41f, -.238856f, -.314751f, .242062f, .378029f},
    {"Wyvern.mesh", .62f, -.56663f, -.318587f, .566629f, .619002f},
    {"lich.mesh", .65f, -.515405f, -.621f, .42628f, .647588f},
    {"skeleton.mesh", .44f, -.248051f, -.429675f, .279937f, .306748f}
};

// Walking envelope of the skinned Walk poses clipped below the top of each bed:
// only the body under the bed top counts for passing it, arms and torso above
// do not. The clip lies one level-one scale step below the bed top (mesh units),
// so higher levels keep this conservative band, not a narrower guessed footprint.
// XY interpolation clearance matches the full walking catalog above. An empty
// band means no animated part reaches that low (flying creatures).
constexpr float lowWalkingMargin = .010001f;
struct BodyBand
{
    const char* name;
    float height;
    float minX, minY, maxX, maxY;
    bool empty;
    float minZ;
};
static const BodyBand bodyBands[] = {
    {"Adventurer.mesh", .073802f, -.11224f, -.294089f, .109446f, .231575f, false, -.00231f},
    {"Adventurer.mesh", .087917f, -.11224f, -.294089f, .109446f, .231575f, false, -.00231f},
    {"Adventurer.mesh", .276672f, -.126903f, -.294089f, .149075f, .231575f, false, -.00231f},
    {"Adventurer.mesh", .306282f, -.139089f, -.294089f, .149286f, .231575f, false, -.00231f},
    {"Adventurer.mesh", .560469f, -.213135f, -.294089f, .209982f, .231575f, false, -.00231f},
    {"Adventurer.mesh", .59f, -.213135f, -.294089f, .209982f, .231575f, false, -.00231f},
    {"Adventurer.mesh", .686024f, -.213135f, -.294089f, .209982f, .231575f, false, -.00231f},
    {"Adventurer.mesh", .71f, -.213135f, -.294089f, .209982f, .231575f, false, -.00231f},
    {"Adventurer.mesh", .767922f, -.213135f, -.294089f, .209982f, .231575f, false, -.00231f},
    {"Adventurer.mesh", .805918f, -.213135f, -.294089f, .209982f, .231575f, false, -.00231f},
    {"Adventurer.mesh", .841464f, -.213135f, -.294089f, .209982f, .231575f, false, -.00231f},
    {"Adventurer.mesh", .952407f, -.213135f, -.294089f, .209982f, .231575f, false, -.00231f},
    {"Adventurer.mesh", 1.28522f, -.213135f, -.294089f, .209982f, .231575f, false, -.00231f},
    {"CaveHornet.mesh", .073802f, 0.0f, 0.0f, 0.0f, 0.0f, true, 0.0f},
    {"CaveHornet.mesh", .087917f, 0.0f, 0.0f, 0.0f, 0.0f, true, 0.0f},
    {"CaveHornet.mesh", .276672f, -.326506f, -.194049f, .326482f, .213642f, false, .128645f},
    {"CaveHornet.mesh", .306282f, -.326605f, -.194049f, .326581f, .213642f, false, .128645f},
    {"CaveHornet.mesh", .560469f, -.326605f, -.194049f, .326581f, .213642f, false, .128645f},
    {"CaveHornet.mesh", .59f, -.326605f, -.194049f, .326581f, .213642f, false, .128645f},
    {"CaveHornet.mesh", .686024f, -.326605f, -.194049f, .326581f, .213642f, false, .128645f},
    {"CaveHornet.mesh", .71f, -.326605f, -.194049f, .326581f, .213642f, false, .128645f},
    {"CaveHornet.mesh", .767922f, -.326605f, -.194049f, .326581f, .213642f, false, .128645f},
    {"CaveHornet.mesh", .805918f, -.326605f, -.194049f, .326581f, .213642f, false, .128645f},
    {"CaveHornet.mesh", .841464f, -.326605f, -.194049f, .326581f, .213642f, false, .128645f},
    {"CaveHornet.mesh", .952407f, -.326605f, -.194049f, .326581f, .213642f, false, .128645f},
    {"CaveHornet.mesh", 1.28522f, -.326605f, -.194049f, .326581f, .213642f, false, .128645f},
    {"Cultist.mesh", .073802f, -.12125f, -.335677f, .12125f, .246237f, false, -.027495f},
    {"Cultist.mesh", .087917f, -.12125f, -.335677f, .12125f, .246237f, false, -.027495f},
    {"Cultist.mesh", .276672f, -.153164f, -.335677f, .155892f, .275325f, false, -.027495f},
    {"Cultist.mesh", .306282f, -.153164f, -.335677f, .155892f, .275325f, false, -.027495f},
    {"Cultist.mesh", .560469f, -.22619f, -.335677f, .231711f, .275325f, false, -.027495f},
    {"Cultist.mesh", .59f, -.229131f, -.335677f, .231711f, .275325f, false, -.027495f},
    {"Cultist.mesh", .686024f, -.231367f, -.335677f, .231711f, .275325f, false, -.027495f},
    {"Cultist.mesh", .71f, -.231367f, -.335677f, .231711f, .275325f, false, -.027495f},
    {"Cultist.mesh", .767922f, -.231367f, -.335677f, .231711f, .275325f, false, -.027495f},
    {"Cultist.mesh", .805918f, -.231367f, -.335677f, .231711f, .275325f, false, -.027495f},
    {"Cultist.mesh", .841464f, -.232195f, -.335677f, .232195f, .275325f, false, -.027495f},
    {"Cultist.mesh", .952407f, -.279145f, -.335677f, .279145f, .275325f, false, -.027495f},
    {"Cultist.mesh", 1.28522f, -.280156f, -.335677f, .280156f, .275325f, false, -.027495f},
    {"DarkElf.mesh", .073802f, -.076123f, -.168468f, .116131f, .282346f, false, -.039261f},
    {"DarkElf.mesh", .087917f, -.076123f, -.168468f, .116131f, .290376f, false, -.039261f},
    {"DarkElf.mesh", .276672f, -.085169f, -.168468f, .116131f, .294587f, false, -.039261f},
    {"DarkElf.mesh", .306282f, -.085169f, -.168468f, .116131f, .294587f, false, -.039261f},
    {"DarkElf.mesh", .560469f, -.165293f, -.168468f, .130259f, .294587f, false, -.039261f},
    {"DarkElf.mesh", .59f, -.165293f, -.168468f, .130259f, .294587f, false, -.039261f},
    {"DarkElf.mesh", .686024f, -.185216f, -.168468f, .130259f, .294587f, false, -.039261f},
    {"DarkElf.mesh", .71f, -.185216f, -.168468f, .130259f, .294587f, false, -.039261f},
    {"DarkElf.mesh", .767922f, -.185216f, -.168468f, .130259f, .294587f, false, -.039261f},
    {"DarkElf.mesh", .805918f, -.185216f, -.168468f, .130259f, .294587f, false, -.039261f},
    {"DarkElf.mesh", .841464f, -.185216f, -.168468f, .130259f, .294587f, false, -.039261f},
    {"DarkElf.mesh", .952407f, -.185216f, -.168468f, .130259f, .294587f, false, -.039261f},
    {"DarkElf.mesh", 1.28522f, -.185216f, -.168468f, .130259f, .294587f, false, -.039261f},
    {"Defender.mesh", .073802f, -.247534f, -.85444f, .130207f, .255929f, false, -.025755f},
    {"Defender.mesh", .087917f, -.247596f, -.862685f, .130207f, .264717f, false, -.025755f},
    {"Defender.mesh", .276672f, -.251305f, -.962843f, .130207f, .27361f, false, -.025755f},
    {"Defender.mesh", .306282f, -.251305f, -.988388f, .130207f, .27361f, false, -.025755f},
    {"Defender.mesh", .560469f, -.251305f, -1.04913f, .161523f, .363148f, false, -.025755f},
    {"Defender.mesh", .59f, -.251305f, -1.04913f, .161523f, .365243f, false, -.025755f},
    {"Defender.mesh", .686024f, -.251305f, -1.05854f, .174574f, .365243f, false, -.025755f},
    {"Defender.mesh", .71f, -.251305f, -1.0624f, .174574f, .365243f, false, -.025755f},
    {"Defender.mesh", .767922f, -.251305f, -1.0624f, .174574f, .365243f, false, -.025755f},
    {"Defender.mesh", .805918f, -.251305f, -1.0624f, .174574f, .365243f, false, -.025755f},
    {"Defender.mesh", .841464f, -.251305f, -1.0624f, .174574f, .365243f, false, -.025755f},
    {"Defender.mesh", .952407f, -.251305f, -1.0624f, .174574f, .365243f, false, -.025755f},
    {"Defender.mesh", 1.28522f, -.251305f, -1.0624f, .191936f, .365243f, false, -.025755f},
    {"Dragon.mesh", .073802f, -.132556f, -.308052f, .138703f, .244352f, false, -.004297f},
    {"Dragon.mesh", .087917f, -.24494f, -.350987f, .192072f, .244352f, false, -.004297f},
    {"Dragon.mesh", .276672f, -.342277f, -.355221f, .338384f, .707872f, false, -.004297f},
    {"Dragon.mesh", .306282f, -.342277f, -.355221f, .338384f, .756761f, false, -.004297f},
    {"Dragon.mesh", .560469f, -.732641f, -.897033f, .750112f, .848264f, false, -.004297f},
    {"Dragon.mesh", .59f, -.732641f, -.897033f, .750112f, .848264f, false, -.004297f},
    {"Dragon.mesh", .686024f, -.732641f, -.897033f, .750112f, .848264f, false, -.004297f},
    {"Dragon.mesh", .71f, -.732641f, -.897033f, .750112f, .848264f, false, -.004297f},
    {"Dragon.mesh", .767922f, -.732641f, -.897033f, .750112f, .848264f, false, -.004297f},
    {"Dragon.mesh", .805918f, -.732641f, -.897033f, .750112f, .848264f, false, -.004297f},
    {"Dragon.mesh", .841464f, -.732641f, -.897033f, .750112f, .848264f, false, -.004297f},
    {"Dragon.mesh", .952407f, -.732641f, -.897033f, .750112f, .848264f, false, -.004297f},
    {"Dragon.mesh", 1.28522f, -.732641f, -.897033f, .750112f, .848264f, false, -.004297f},
    {"Dwarf1.mesh", .073802f, -.122935f, -.239296f, .122842f, .173019f, false, -.037031f},
    {"Dwarf1.mesh", .087917f, -.122935f, -.239296f, .122842f, .174218f, false, -.037031f},
    {"Dwarf1.mesh", .276672f, -.240157f, -.239296f, .241007f, .1889f, false, -.037031f},
    {"Dwarf1.mesh", .306282f, -.248399f, -.239296f, .248014f, .1889f, false, -.037031f},
    {"Dwarf1.mesh", .560469f, -.24859f, -.239296f, .248231f, .1889f, false, -.037031f},
    {"Dwarf1.mesh", .59f, -.24859f, -.239296f, .248231f, .1889f, false, -.037031f},
    {"Dwarf1.mesh", .686024f, -.24859f, -.239296f, .248231f, .1889f, false, -.037031f},
    {"Dwarf1.mesh", .71f, -.24859f, -.239296f, .248231f, .1889f, false, -.037031f},
    {"Dwarf1.mesh", .767922f, -.24859f, -.239296f, .248231f, .1889f, false, -.037031f},
    {"Dwarf1.mesh", .805918f, -.24859f, -.239296f, .248231f, .1889f, false, -.037031f},
    {"Dwarf1.mesh", .841464f, -.24859f, -.239296f, .248231f, .1889f, false, -.037031f},
    {"Dwarf1.mesh", .952407f, -.24859f, -.239296f, .248231f, .1889f, false, -.037031f},
    {"Dwarf1.mesh", 1.28522f, -.24859f, -.239296f, .248231f, .1889f, false, -.037031f},
    {"Dwarf2.mesh", .073802f, -.12418f, -.238959f, .124005f, .170386f, false, -.027467f},
    {"Dwarf2.mesh", .087917f, -.12418f, -.238959f, .124005f, .172825f, false, -.027467f},
    {"Dwarf2.mesh", .276672f, -.237981f, -.238959f, .23799f, .188901f, false, -.027467f},
    {"Dwarf2.mesh", .306282f, -.241995f, -.238959f, .241552f, .188901f, false, -.027467f},
    {"Dwarf2.mesh", .560469f, -.243824f, -.238959f, .243376f, .188901f, false, -.027467f},
    {"Dwarf2.mesh", .59f, -.243824f, -.238959f, .243376f, .188901f, false, -.027467f},
    {"Dwarf2.mesh", .686024f, -.243824f, -.238959f, .243376f, .188901f, false, -.027467f},
    {"Dwarf2.mesh", .71f, -.243824f, -.238959f, .243376f, .188901f, false, -.027467f},
    {"Dwarf2.mesh", .767922f, -.243824f, -.238959f, .243376f, .188901f, false, -.027467f},
    {"Dwarf2.mesh", .805918f, -.243824f, -.238959f, .243376f, .188901f, false, -.027467f},
    {"Dwarf2.mesh", .841464f, -.243824f, -.238959f, .243376f, .188901f, false, -.027467f},
    {"Dwarf2.mesh", .952407f, -.243824f, -.238959f, .243376f, .188901f, false, -.027467f},
    {"Dwarf2.mesh", 1.28522f, -.243824f, -.238959f, .243376f, .188901f, false, -.027467f},
    {"Elf.mesh", .073802f, -.076123f, -.168468f, .116131f, .282346f, false, -.039261f},
    {"Elf.mesh", .087917f, -.076123f, -.168468f, .116131f, .290376f, false, -.039261f},
    {"Elf.mesh", .276672f, -.085169f, -.168468f, .116131f, .294587f, false, -.039261f},
    {"Elf.mesh", .306282f, -.085169f, -.168468f, .116131f, .294587f, false, -.039261f},
    {"Elf.mesh", .560469f, -.165293f, -.168468f, .130259f, .294587f, false, -.039261f},
    {"Elf.mesh", .59f, -.165293f, -.168468f, .130259f, .294587f, false, -.039261f},
    {"Elf.mesh", .686024f, -.185216f, -.168468f, .130259f, .294587f, false, -.039261f},
    {"Elf.mesh", .71f, -.185216f, -.168468f, .130259f, .294587f, false, -.039261f},
    {"Elf.mesh", .767922f, -.185216f, -.168468f, .130259f, .294587f, false, -.039261f},
    {"Elf.mesh", .805918f, -.185216f, -.168468f, .130259f, .294587f, false, -.039261f},
    {"Elf.mesh", .841464f, -.185216f, -.168468f, .130259f, .294587f, false, -.039261f},
    {"Elf.mesh", .952407f, -.185216f, -.168468f, .130259f, .294587f, false, -.039261f},
    {"Elf.mesh", 1.28522f, -.185216f, -.168468f, .130259f, .294587f, false, -.039261f},
    {"Gnome.mesh", .073802f, -.070794f, -.174931f, .070779f, .182404f, false, -.006618f},
    {"Gnome.mesh", .087917f, -.070794f, -.174931f, .070779f, .192727f, false, -.006618f},
    {"Gnome.mesh", .276672f, -.244296f, -.174931f, .244296f, .192727f, false, -.006618f},
    {"Gnome.mesh", .306282f, -.247507f, -.186157f, .247507f, .192727f, false, -.006618f},
    {"Gnome.mesh", .560469f, -.247507f, -.186157f, .247507f, .192727f, false, -.006618f},
    {"Gnome.mesh", .59f, -.247507f, -.186157f, .247507f, .192727f, false, -.006618f},
    {"Gnome.mesh", .686024f, -.247507f, -.186157f, .247507f, .192727f, false, -.006618f},
    {"Gnome.mesh", .71f, -.247507f, -.186157f, .247507f, .192727f, false, -.006618f},
    {"Gnome.mesh", .767922f, -.247507f, -.186157f, .247507f, .192727f, false, -.006618f},
    {"Gnome.mesh", .805918f, -.247507f, -.186157f, .247507f, .192727f, false, -.006618f},
    {"Gnome.mesh", .841464f, -.247507f, -.186157f, .247507f, .192727f, false, -.006618f},
    {"Gnome.mesh", .952407f, -.247507f, -.186157f, .247507f, .192727f, false, -.006618f},
    {"Gnome.mesh", 1.28522f, -.247507f, -.186157f, .247507f, .192727f, false, -.006618f},
    {"Goblin.mesh", .073802f, -.09128f, -.198021f, .106435f, .242259f, false, -.026127f},
    {"Goblin.mesh", .087917f, -.09128f, -.198021f, .106435f, .245692f, false, -.026127f},
    {"Goblin.mesh", .276672f, -.122437f, -.298201f, .255044f, .245692f, false, -.026127f},
    {"Goblin.mesh", .306282f, -.122437f, -.31675f, .255044f, .245692f, false, -.026127f},
    {"Goblin.mesh", .560469f, -.13325f, -.349861f, .255044f, .245692f, false, -.026127f},
    {"Goblin.mesh", .59f, -.13325f, -.349861f, .255044f, .245692f, false, -.026127f},
    {"Goblin.mesh", .686024f, -.13325f, -.349861f, .255044f, .245692f, false, -.026127f},
    {"Goblin.mesh", .71f, -.13325f, -.349861f, .255044f, .245692f, false, -.026127f},
    {"Goblin.mesh", .767922f, -.13325f, -.349861f, .255044f, .245692f, false, -.026127f},
    {"Goblin.mesh", .805918f, -.13325f, -.349861f, .255044f, .245692f, false, -.026127f},
    {"Goblin.mesh", .841464f, -.13325f, -.349861f, .255044f, .245692f, false, -.026127f},
    {"Goblin.mesh", .952407f, -.13325f, -.349861f, .255044f, .245692f, false, -.026127f},
    {"Goblin.mesh", 1.28522f, -.13325f, -.349861f, .255044f, .245692f, false, -.026127f},
    {"Knight.mesh", .073802f, -.196323f, -.358722f, .185533f, .223521f, false, -.022251f},
    {"Knight.mesh", .087917f, -.196323f, -.358722f, .20325f, .239972f, false, -.022251f},
    {"Knight.mesh", .276672f, -.218926f, -.358722f, .262491f, .385971f, false, -.022251f},
    {"Knight.mesh", .306282f, -.218926f, -.358722f, .262491f, .385971f, false, -.022251f},
    {"Knight.mesh", .560469f, -.261789f, -.358722f, .266867f, .385971f, false, -.022251f},
    {"Knight.mesh", .59f, -.261789f, -.358722f, .266867f, .385971f, false, -.022251f},
    {"Knight.mesh", .686024f, -.261789f, -.358722f, .266867f, .385971f, false, -.022251f},
    {"Knight.mesh", .71f, -.261789f, -.358722f, .266867f, .385971f, false, -.022251f},
    {"Knight.mesh", .767922f, -.261789f, -.358722f, .266867f, .385971f, false, -.022251f},
    {"Knight.mesh", .805918f, -.261789f, -.358722f, .266867f, .385971f, false, -.022251f},
    {"Knight.mesh", .841464f, -.261789f, -.358722f, .266867f, .385971f, false, -.022251f},
    {"Knight.mesh", .952407f, -.261789f, -.358722f, .266867f, .385971f, false, -.022251f},
    {"Knight.mesh", 1.28522f, -.261789f, -.358722f, .266867f, .385971f, false, -.022251f},
    {"Kobold.mesh", .073802f, -.137907f, -.153292f, .084618f, .141324f, false, -.011227f},
    {"Kobold.mesh", .087917f, -.203394f, -.166429f, .084618f, .141447f, false, -.011227f},
    {"Kobold.mesh", .276672f, -.205426f, -.192767f, .136456f, .267816f, false, -.011227f},
    {"Kobold.mesh", .306282f, -.205426f, -.192767f, .136456f, .267816f, false, -.011227f},
    {"Kobold.mesh", .560469f, -.205426f, -.192767f, .136456f, .267816f, false, -.011227f},
    {"Kobold.mesh", .59f, -.205426f, -.192767f, .136456f, .267816f, false, -.011227f},
    {"Kobold.mesh", .686024f, -.205426f, -.192767f, .136456f, .267816f, false, -.011227f},
    {"Kobold.mesh", .71f, -.205426f, -.192767f, .136456f, .267816f, false, -.011227f},
    {"Kobold.mesh", .767922f, -.205426f, -.192767f, .136456f, .267816f, false, -.011227f},
    {"Kobold.mesh", .805918f, -.205426f, -.192767f, .136456f, .267816f, false, -.011227f},
    {"Kobold.mesh", .841464f, -.205426f, -.192767f, .136456f, .267816f, false, -.011227f},
    {"Kobold.mesh", .952407f, -.205426f, -.192767f, .136456f, .267816f, false, -.011227f},
    {"Kobold.mesh", 1.28522f, -.205426f, -.192767f, .136456f, .267816f, false, -.011227f},
    {"Kreatur.mesh", .073802f, -.467869f, -.623358f, .475f, .537126f, false, -.020371f},
    {"Kreatur.mesh", .087917f, -.46877f, -.623358f, .475f, .537126f, false, -.020371f},
    {"Kreatur.mesh", .276672f, -.713778f, -.663678f, .713792f, .537126f, false, -.020371f},
    {"Kreatur.mesh", .306282f, -.720491f, -.668393f, .720511f, .537126f, false, -.020371f},
    {"Kreatur.mesh", .560469f, -.720491f, -.676316f, .720511f, .59084f, false, -.020371f},
    {"Kreatur.mesh", .59f, -.720491f, -.676316f, .720511f, .59084f, false, -.020371f},
    {"Kreatur.mesh", .686024f, -.720491f, -.676316f, .720511f, .59084f, false, -.020371f},
    {"Kreatur.mesh", .71f, -.720491f, -.676316f, .720511f, .59084f, false, -.020371f},
    {"Kreatur.mesh", .767922f, -.720491f, -.676316f, .720511f, .59084f, false, -.020371f},
    {"Kreatur.mesh", .805918f, -.720491f, -.676316f, .720511f, .59084f, false, -.020371f},
    {"Kreatur.mesh", .841464f, -.720491f, -.676316f, .720511f, .59084f, false, -.020371f},
    {"Kreatur.mesh", .952407f, -.720491f, -.676316f, .720511f, .59084f, false, -.020371f},
    {"Kreatur.mesh", 1.28522f, -.720491f, -.676316f, .720511f, .59084f, false, -.020371f},
    {"LavaSpawn.mesh", .073802f, -.794634f, -.627325f, .789449f, 1.11894f, false, -.052282f},
    {"LavaSpawn.mesh", .087917f, -.794634f, -.632651f, .789449f, 1.11894f, false, -.052282f},
    {"LavaSpawn.mesh", .276672f, -.794634f, -.702337f, .789449f, 1.11894f, false, -.052282f},
    {"LavaSpawn.mesh", .306282f, -.794634f, -.702396f, .789449f, 1.11894f, false, -.052282f},
    {"LavaSpawn.mesh", .560469f, -.794634f, -.762326f, .791635f, 1.11894f, false, -.052282f},
    {"LavaSpawn.mesh", .59f, -.794634f, -.762326f, .791635f, 1.11894f, false, -.052282f},
    {"LavaSpawn.mesh", .686024f, -.794634f, -.763218f, .791635f, 1.11894f, false, -.052282f},
    {"LavaSpawn.mesh", .71f, -.794634f, -.763218f, .791635f, 1.11894f, false, -.052282f},
    {"LavaSpawn.mesh", .767922f, -.794634f, -.763218f, .791635f, 1.11894f, false, -.052282f},
    {"LavaSpawn.mesh", .805918f, -.794634f, -.763218f, .791635f, 1.11894f, false, -.052282f},
    {"LavaSpawn.mesh", .841464f, -.794634f, -.763218f, .791635f, 1.11894f, false, -.052282f},
    {"LavaSpawn.mesh", .952407f, -.794634f, -.763218f, .791635f, 1.11894f, false, -.052282f},
    {"LavaSpawn.mesh", 1.28522f, -.794634f, -.763218f, .791635f, 1.11894f, false, -.052282f},
    {"Lizardman.mesh", .073802f, -.108072f, -.365149f, .108144f, .293085f, false, -.003575f},
    {"Lizardman.mesh", .087917f, -.108073f, -.365149f, .108144f, .297639f, false, -.003575f},
    {"Lizardman.mesh", .276672f, -.111313f, -.365149f, .111263f, .701479f, false, -.003575f},
    {"Lizardman.mesh", .306282f, -.111313f, -.365149f, .111263f, .705455f, false, -.003575f},
    {"Lizardman.mesh", .560469f, -.318192f, -.365149f, .309732f, .707061f, false, -.003575f},
    {"Lizardman.mesh", .59f, -.318192f, -.365149f, .309732f, .707061f, false, -.003575f},
    {"Lizardman.mesh", .686024f, -.318192f, -.365149f, .309732f, .707061f, false, -.003575f},
    {"Lizardman.mesh", .71f, -.318192f, -.365149f, .309732f, .707061f, false, -.003575f},
    {"Lizardman.mesh", .767922f, -.318192f, -.365149f, .309732f, .707061f, false, -.003575f},
    {"Lizardman.mesh", .805918f, -.318192f, -.365149f, .309732f, .707061f, false, -.003575f},
    {"Lizardman.mesh", .841464f, -.318192f, -.365149f, .309732f, .707061f, false, -.003575f},
    {"Lizardman.mesh", .952407f, -.318192f, -.365149f, .309732f, .707061f, false, -.003575f},
    {"Lizardman.mesh", 1.28522f, -.318192f, -.365149f, .309732f, .707061f, false, -.003575f},
    {"Monk.mesh", .073802f, -.146848f, -.265317f, .146356f, .121055f, false, -.02499f},
    {"Monk.mesh", .087917f, -.146848f, -.265317f, .146356f, .144862f, false, -.02499f},
    {"Monk.mesh", .276672f, -.146848f, -.265317f, .146356f, .180716f, false, -.02499f},
    {"Monk.mesh", .306282f, -.146848f, -.265317f, .146356f, .180716f, false, -.02499f},
    {"Monk.mesh", .560469f, -.185454f, -.265317f, .183061f, .180716f, false, -.02499f},
    {"Monk.mesh", .59f, -.185454f, -.265317f, .183061f, .180716f, false, -.02499f},
    {"Monk.mesh", .686024f, -.185454f, -.265317f, .183061f, .180716f, false, -.02499f},
    {"Monk.mesh", .71f, -.185454f, -.265317f, .183061f, .180716f, false, -.02499f},
    {"Monk.mesh", .767922f, -.185454f, -.265317f, .183061f, .180716f, false, -.02499f},
    {"Monk.mesh", .805918f, -.185454f, -.265317f, .183061f, .180716f, false, -.02499f},
    {"Monk.mesh", .841464f, -.185454f, -.265317f, .183061f, .180716f, false, -.02499f},
    {"Monk.mesh", .952407f, -.185454f, -.265317f, .183061f, .180716f, false, -.02499f},
    {"Monk.mesh", 1.28522f, -.185454f, -.265317f, .183061f, .180716f, false, -.02499f},
    {"NatureMonster.mesh", .073802f, -.423013f, -.402614f, .423011f, .31668f, false, -.032417f},
    {"NatureMonster.mesh", .087917f, -.42452f, -.415187f, .424519f, .327784f, false, -.032417f},
    {"NatureMonster.mesh", .276672f, -.458655f, -.418676f, .458654f, .436317f, false, -.032417f},
    {"NatureMonster.mesh", .306282f, -.470545f, -.418676f, .470544f, .436317f, false, -.032417f},
    {"NatureMonster.mesh", .560469f, -.475821f, -.427183f, .475819f, .436317f, false, -.032417f},
    {"NatureMonster.mesh", .59f, -.475821f, -.427183f, .475819f, .436317f, false, -.032417f},
    {"NatureMonster.mesh", .686024f, -.475821f, -.427183f, .475819f, .436317f, false, -.032417f},
    {"NatureMonster.mesh", .71f, -.475821f, -.427183f, .475819f, .436317f, false, -.032417f},
    {"NatureMonster.mesh", .767922f, -.475821f, -.427183f, .475819f, .436317f, false, -.032417f},
    {"NatureMonster.mesh", .805918f, -.475821f, -.427183f, .475819f, .436317f, false, -.032417f},
    {"NatureMonster.mesh", .841464f, -.475821f, -.427183f, .475819f, .436317f, false, -.032417f},
    {"NatureMonster.mesh", .952407f, -.475821f, -.427183f, .475819f, .436317f, false, -.032417f},
    {"NatureMonster.mesh", 1.28522f, -.475821f, -.427183f, .475819f, .436317f, false, -.032417f},
    {"Orc.mesh", .073802f, -.134865f, -.251628f, .13519f, .271754f, false, -.005486f},
    {"Orc.mesh", .087917f, -.134865f, -.251628f, .13519f, .279291f, false, -.005486f},
    {"Orc.mesh", .276672f, -.22272f, -.251628f, .222719f, .279291f, false, -.005486f},
    {"Orc.mesh", .306282f, -.259066f, -.269845f, .259066f, .279291f, false, -.005486f},
    {"Orc.mesh", .560469f, -.291688f, -.269845f, .291117f, .279291f, false, -.005486f},
    {"Orc.mesh", .59f, -.291688f, -.269845f, .291117f, .279291f, false, -.005486f},
    {"Orc.mesh", .686024f, -.291688f, -.269845f, .291117f, .279291f, false, -.005486f},
    {"Orc.mesh", .71f, -.291688f, -.269845f, .291117f, .279291f, false, -.005486f},
    {"Orc.mesh", .767922f, -.291688f, -.269845f, .291117f, .279291f, false, -.005486f},
    {"Orc.mesh", .805918f, -.291688f, -.269845f, .291117f, .279291f, false, -.005486f},
    {"Orc.mesh", .841464f, -.291688f, -.269845f, .291117f, .279291f, false, -.005486f},
    {"Orc.mesh", .952407f, -.291688f, -.269845f, .291117f, .279291f, false, -.005486f},
    {"Orc.mesh", 1.28522f, -.291688f, -.269845f, .291117f, .279291f, false, -.005486f},
    {"PitDemon.mesh", .073802f, -.192091f, -.468384f, .1921f, .466082f, false, -.049681f},
    {"PitDemon.mesh", .087917f, -.192091f, -.468384f, .1921f, .472145f, false, -.049681f},
    {"PitDemon.mesh", .276672f, -.192091f, -.468384f, .1921f, .602836f, false, -.049681f},
    {"PitDemon.mesh", .306282f, -.192091f, -.468384f, .1921f, .610731f, false, -.049681f},
    {"PitDemon.mesh", .560469f, -.300642f, -.468384f, .302622f, .617701f, false, -.049681f},
    {"PitDemon.mesh", .59f, -.306971f, -.468384f, .308149f, .617701f, false, -.049681f},
    {"PitDemon.mesh", .686024f, -1.315f, -.468384f, 1.315f, .617701f, false, -.049681f},
    {"PitDemon.mesh", .71f, -1.315f, -.468384f, 1.315f, .617701f, false, -.049681f},
    {"PitDemon.mesh", .767922f, -1.315f, -.468384f, 1.315f, .617701f, false, -.049681f},
    {"PitDemon.mesh", .805918f, -1.315f, -.468384f, 1.315f, .617701f, false, -.049681f},
    {"PitDemon.mesh", .841464f, -1.315f, -.468384f, 1.315f, .617701f, false, -.049681f},
    {"PitDemon.mesh", .952407f, -1.315f, -.468384f, 1.315f, .617701f, false, -.049681f},
    {"PitDemon.mesh", 1.28522f, -1.315f, -.468384f, 1.315f, .617701f, false, -.049681f},
    {"Rat.mesh", .073802f, -.11897f, -.444537f, .118309f, .570713f, false, -.030562f},
    {"Rat.mesh", .087917f, -.126345f, -.444537f, .118309f, .570713f, false, -.030562f},
    {"Rat.mesh", .276672f, -.127487f, -.46173f, .12022f, .570713f, false, -.030562f},
    {"Rat.mesh", .306282f, -.127487f, -.46173f, .12022f, .570713f, false, -.030562f},
    {"Rat.mesh", .560469f, -.127487f, -.46173f, .12022f, .570713f, false, -.030562f},
    {"Rat.mesh", .59f, -.127487f, -.46173f, .12022f, .570713f, false, -.030562f},
    {"Rat.mesh", .686024f, -.127487f, -.46173f, .12022f, .570713f, false, -.030562f},
    {"Rat.mesh", .71f, -.127487f, -.46173f, .12022f, .570713f, false, -.030562f},
    {"Rat.mesh", .767922f, -.127487f, -.46173f, .12022f, .570713f, false, -.030562f},
    {"Rat.mesh", .805918f, -.127487f, -.46173f, .12022f, .570713f, false, -.030562f},
    {"Rat.mesh", .841464f, -.127487f, -.46173f, .12022f, .570713f, false, -.030562f},
    {"Rat.mesh", .952407f, -.127487f, -.46173f, .12022f, .570713f, false, -.030562f},
    {"Rat.mesh", 1.28522f, -.127487f, -.46173f, .12022f, .570713f, false, -.030562f},
    {"Roach.mesh", .073802f, -.279383f, -.436215f, .279686f, .389623f, false, -.003397f},
    {"Roach.mesh", .087917f, -.279548f, -.436215f, .279686f, .389689f, false, -.003397f},
    {"Roach.mesh", .276672f, -.280405f, -.436215f, .28347f, .416947f, false, -.003397f},
    {"Roach.mesh", .306282f, -.280405f, -.436215f, .28347f, .425847f, false, -.003397f},
    {"Roach.mesh", .560469f, -.280405f, -.436215f, .28347f, .446461f, false, -.003397f},
    {"Roach.mesh", .59f, -.280405f, -.436215f, .28347f, .446461f, false, -.003397f},
    {"Roach.mesh", .686024f, -.280405f, -.436215f, .28347f, .446461f, false, -.003397f},
    {"Roach.mesh", .71f, -.280405f, -.436215f, .28347f, .446461f, false, -.003397f},
    {"Roach.mesh", .767922f, -.280405f, -.436215f, .28347f, .446461f, false, -.003397f},
    {"Roach.mesh", .805918f, -.280405f, -.436215f, .28347f, .446461f, false, -.003397f},
    {"Roach.mesh", .841464f, -.280405f, -.436215f, .28347f, .446461f, false, -.003397f},
    {"Roach.mesh", .952407f, -.280405f, -.436215f, .28347f, .446461f, false, -.003397f},
    {"Roach.mesh", 1.28522f, -.280405f, -.436215f, .28347f, .446461f, false, -.003397f},
    {"RunelordDwarf.mesh", .073802f, -.144518f, -.373078f, .144375f, .207465f, false, -.008892f},
    {"RunelordDwarf.mesh", .087917f, -.144518f, -.38003f, .144375f, .209046f, false, -.008892f},
    {"RunelordDwarf.mesh", .276672f, -.157714f, -.393943f, .157713f, .209046f, false, -.008892f},
    {"RunelordDwarf.mesh", .306282f, -.16562f, -.393943f, .165619f, .209046f, false, -.008892f},
    {"RunelordDwarf.mesh", .560469f, -.281414f, -.393943f, .272544f, .209046f, false, -.008892f},
    {"RunelordDwarf.mesh", .59f, -.285801f, -.393943f, .288866f, .209046f, false, -.008892f},
    {"RunelordDwarf.mesh", .686024f, -.285801f, -.393943f, .294319f, .209046f, false, -.008892f},
    {"RunelordDwarf.mesh", .71f, -.285801f, -.393943f, .294319f, .209046f, false, -.008892f},
    {"RunelordDwarf.mesh", .767922f, -.285801f, -.393943f, .294319f, .209046f, false, -.008892f},
    {"RunelordDwarf.mesh", .805918f, -.285801f, -.393943f, .294319f, .209046f, false, -.008892f},
    {"RunelordDwarf.mesh", .841464f, -.285801f, -.393943f, .294319f, .209046f, false, -.008892f},
    {"RunelordDwarf.mesh", .952407f, -.285801f, -.393943f, .294319f, .209046f, false, -.008892f},
    {"RunelordDwarf.mesh", 1.28522f, -.285801f, -.393943f, .294319f, .209046f, false, -.008892f},
    {"Scarab.mesh", .073802f, -.49026f, -.214609f, .496994f, .365987f, false, -.036621f},
    {"Scarab.mesh", .087917f, -.502353f, -.214609f, .505099f, .365987f, false, -.036621f},
    {"Scarab.mesh", .276672f, -.544043f, -.214609f, .547189f, .365987f, false, -.036621f},
    {"Scarab.mesh", .306282f, -.548049f, -.214609f, .551956f, .365987f, false, -.036621f},
    {"Scarab.mesh", .560469f, -.570193f, -.214609f, .573705f, .365987f, false, -.036621f},
    {"Scarab.mesh", .59f, -.570193f, -.214609f, .573705f, .365987f, false, -.036621f},
    {"Scarab.mesh", .686024f, -.570193f, -.214609f, .573705f, .365987f, false, -.036621f},
    {"Scarab.mesh", .71f, -.570193f, -.214609f, .573705f, .365987f, false, -.036621f},
    {"Scarab.mesh", .767922f, -.570193f, -.214609f, .573705f, .365987f, false, -.036621f},
    {"Scarab.mesh", .805918f, -.570193f, -.214609f, .573705f, .372892f, false, -.036621f},
    {"Scarab.mesh", .841464f, -.570193f, -.214609f, .573705f, .372892f, false, -.036621f},
    {"Scarab.mesh", .952407f, -.570193f, -.241534f, .573705f, .372892f, false, -.036621f},
    {"Scarab.mesh", 1.28522f, -.570193f, -.279468f, .573705f, .372892f, false, -.036621f},
    {"Slime.mesh", .073802f, -.127922f, -.401151f, .12793f, .195001f, false, -.016886f},
    {"Slime.mesh", .087917f, -.129409f, -.409826f, .129396f, .195001f, false, -.016886f},
    {"Slime.mesh", .276672f, -.129409f, -.503814f, .129396f, .195001f, false, -.016886f},
    {"Slime.mesh", .306282f, -.129409f, -.518025f, .129396f, .195001f, false, -.016886f},
    {"Slime.mesh", .560469f, -.129409f, -.554726f, .129396f, .195001f, false, -.016886f},
    {"Slime.mesh", .59f, -.129409f, -.554726f, .129396f, .195001f, false, -.016886f},
    {"Slime.mesh", .686024f, -.129409f, -.554726f, .129396f, .195001f, false, -.016886f},
    {"Slime.mesh", .71f, -.129409f, -.554726f, .129396f, .195001f, false, -.016886f},
    {"Slime.mesh", .767922f, -.129409f, -.554726f, .129396f, .195001f, false, -.016886f},
    {"Slime.mesh", .805918f, -.129409f, -.554726f, .129396f, .195001f, false, -.016886f},
    {"Slime.mesh", .841464f, -.129409f, -.554726f, .129396f, .195001f, false, -.016886f},
    {"Slime.mesh", .952407f, -.129409f, -.554726f, .129396f, .195001f, false, -.016886f},
    {"Slime.mesh", 1.28522f, -.129409f, -.554726f, .129396f, .195001f, false, -.016886f},
    {"Spider.mesh", .073802f, -.385553f, -.415698f, .385636f, .385696f, false, -.08359f},
    {"Spider.mesh", .087917f, -.385553f, -.415698f, .385636f, .385696f, false, -.08359f},
    {"Spider.mesh", .276672f, -.385553f, -.415698f, .385636f, .385696f, false, -.08359f},
    {"Spider.mesh", .306282f, -.385553f, -.415698f, .385636f, .385696f, false, -.08359f},
    {"Spider.mesh", .560469f, -.385553f, -.415698f, .385636f, .385696f, false, -.08359f},
    {"Spider.mesh", .59f, -.385553f, -.415698f, .385636f, .385696f, false, -.08359f},
    {"Spider.mesh", .686024f, -.385553f, -.415698f, .385636f, .385696f, false, -.08359f},
    {"Spider.mesh", .71f, -.385553f, -.415698f, .385636f, .385696f, false, -.08359f},
    {"Spider.mesh", .767922f, -.385553f, -.415698f, .385636f, .385696f, false, -.08359f},
    {"Spider.mesh", .805918f, -.385553f, -.415698f, .385636f, .385696f, false, -.08359f},
    {"Spider.mesh", .841464f, -.385553f, -.415698f, .385636f, .385696f, false, -.08359f},
    {"Spider.mesh", .952407f, -.385553f, -.415698f, .385636f, .385696f, false, -.08359f},
    {"Spider.mesh", 1.28522f, -.385553f, -.415698f, .385636f, .385696f, false, -.08359f},
    {"TentacleAlbine.mesh", .073802f, -.232142f, -.294767f, .231845f, .284453f, false, -.000321f},
    {"TentacleAlbine.mesh", .087917f, -.232142f, -.294767f, .231845f, .284453f, false, -.000321f},
    {"TentacleAlbine.mesh", .276672f, -.232142f, -.294767f, .231845f, .284453f, false, -.000321f},
    {"TentacleAlbine.mesh", .306282f, -.232142f, -.294767f, .231845f, .284453f, false, -.000321f},
    {"TentacleAlbine.mesh", .560469f, -.232142f, -.294767f, .231845f, .284453f, false, -.000321f},
    {"TentacleAlbine.mesh", .59f, -.232142f, -.294767f, .231845f, .284453f, false, -.000321f},
    {"TentacleAlbine.mesh", .686024f, -.232142f, -.294767f, .231845f, .284453f, false, -.000321f},
    {"TentacleAlbine.mesh", .71f, -.232142f, -.294767f, .231845f, .284453f, false, -.000321f},
    {"TentacleAlbine.mesh", .767922f, -.232142f, -.294767f, .231845f, .284453f, false, -.000321f},
    {"TentacleAlbine.mesh", .805918f, -.232142f, -.294767f, .231845f, .284453f, false, -.000321f},
    {"TentacleAlbine.mesh", .841464f, -.232142f, -.294767f, .231845f, .284453f, false, -.000321f},
    {"TentacleAlbine.mesh", .952407f, -.232142f, -.294767f, .231845f, .284453f, false, -.000321f},
    {"TentacleAlbine.mesh", 1.28522f, -.232142f, -.294767f, .231845f, .284453f, false, -.000321f},
    {"TentacleGreen.mesh", .073802f, -.232142f, -.294767f, .231845f, .284453f, false, -.000321f},
    {"TentacleGreen.mesh", .087917f, -.232142f, -.294767f, .231845f, .284453f, false, -.000321f},
    {"TentacleGreen.mesh", .276672f, -.232142f, -.294767f, .231845f, .284453f, false, -.000321f},
    {"TentacleGreen.mesh", .306282f, -.232142f, -.294767f, .231845f, .284453f, false, -.000321f},
    {"TentacleGreen.mesh", .560469f, -.232142f, -.294767f, .231845f, .284453f, false, -.000321f},
    {"TentacleGreen.mesh", .59f, -.232142f, -.294767f, .231845f, .284453f, false, -.000321f},
    {"TentacleGreen.mesh", .686024f, -.232142f, -.294767f, .231845f, .284453f, false, -.000321f},
    {"TentacleGreen.mesh", .71f, -.232142f, -.294767f, .231845f, .284453f, false, -.000321f},
    {"TentacleGreen.mesh", .767922f, -.232142f, -.294767f, .231845f, .284453f, false, -.000321f},
    {"TentacleGreen.mesh", .805918f, -.232142f, -.294767f, .231845f, .284453f, false, -.000321f},
    {"TentacleGreen.mesh", .841464f, -.232142f, -.294767f, .231845f, .284453f, false, -.000321f},
    {"TentacleGreen.mesh", .952407f, -.232142f, -.294767f, .231845f, .284453f, false, -.000321f},
    {"TentacleGreen.mesh", 1.28522f, -.232142f, -.294767f, .231845f, .284453f, false, -.000321f},
    {"Troll.mesh", .073802f, -.525683f, -.390119f, .532351f, .272789f, false, -.003583f},
    {"Troll.mesh", .087917f, -.531766f, -.390119f, .538331f, .272789f, false, -.003583f},
    {"Troll.mesh", .276672f, -.581986f, -.390119f, .594954f, .308156f, false, -.003583f},
    {"Troll.mesh", .306282f, -.588111f, -.390119f, .602679f, .308156f, false, -.003583f},
    {"Troll.mesh", .560469f, -.616233f, -.390119f, .625831f, .308156f, false, -.003583f},
    {"Troll.mesh", .59f, -.616233f, -.390119f, .625831f, .308156f, false, -.003583f},
    {"Troll.mesh", .686024f, -.616233f, -.390119f, .625831f, .308156f, false, -.003583f},
    {"Troll.mesh", .71f, -.616233f, -.407975f, .625831f, .308156f, false, -.003583f},
    {"Troll.mesh", .767922f, -.616233f, -.468787f, .625831f, .308156f, false, -.003583f},
    {"Troll.mesh", .805918f, -.616233f, -.47839f, .625831f, .308156f, false, -.003583f},
    {"Troll.mesh", .841464f, -.616233f, -.478443f, .625831f, .308156f, false, -.003583f},
    {"Troll.mesh", .952407f, -.616233f, -.478443f, .625831f, .308156f, false, -.003583f},
    {"Troll.mesh", 1.28522f, -.616233f, -.478443f, .625831f, .308156f, false, -.003583f},
    {"Wizard.mesh", .073802f, -.14674f, -.305334f, .13172f, .366649f, false, -.021898f},
    {"Wizard.mesh", .087917f, -.147413f, -.305334f, .13172f, .368027f, false, -.021898f},
    {"Wizard.mesh", .276672f, -.147413f, -.305334f, .13172f, .368027f, false, -.021898f},
    {"Wizard.mesh", .306282f, -.147413f, -.305334f, .13172f, .368027f, false, -.021898f},
    {"Wizard.mesh", .560469f, -.228866f, -.305334f, .232062f, .368027f, false, -.021898f},
    {"Wizard.mesh", .59f, -.228866f, -.305334f, .232062f, .368027f, false, -.021898f},
    {"Wizard.mesh", .686024f, -.228866f, -.305334f, .232062f, .368027f, false, -.021898f},
    {"Wizard.mesh", .71f, -.228866f, -.305334f, .232062f, .368027f, false, -.021898f},
    {"Wizard.mesh", .767922f, -.228866f, -.305334f, .232062f, .368027f, false, -.021898f},
    {"Wizard.mesh", .805918f, -.228866f, -.305334f, .232062f, .368027f, false, -.021898f},
    {"Wizard.mesh", .841464f, -.228866f, -.305334f, .232062f, .368027f, false, -.021898f},
    {"Wizard.mesh", .952407f, -.228866f, -.305334f, .232062f, .368027f, false, -.021898f},
    {"Wizard.mesh", 1.28522f, -.228866f, -.305334f, .232062f, .368027f, false, -.021898f},
    {"Wyvern.mesh", .073802f, 0.0f, 0.0f, 0.0f, 0.0f, true, 0.0f},
    {"Wyvern.mesh", .087917f, 0.0f, 0.0f, 0.0f, 0.0f, true, 0.0f},
    {"Wyvern.mesh", .276672f, -.298396f, -.0707f, .298396f, .4966f, false, .095168f},
    {"Wyvern.mesh", .306282f, -.306659f, -.0707f, .306661f, .523086f, false, .095168f},
    {"Wyvern.mesh", .560469f, -.539701f, -.308589f, .539702f, .60904f, false, .095168f},
    {"Wyvern.mesh", .59f, -.546477f, -.308589f, .546478f, .60904f, false, .095168f},
    {"Wyvern.mesh", .686024f, -.55849f, -.308589f, .55849f, .60904f, false, .095168f},
    {"Wyvern.mesh", .71f, -.55849f, -.308589f, .55849f, .60904f, false, .095168f},
    {"Wyvern.mesh", .767922f, -.55849f, -.308589f, .55849f, .60904f, false, .095168f},
    {"Wyvern.mesh", .805918f, -.55849f, -.308589f, .55849f, .60904f, false, .095168f},
    {"Wyvern.mesh", .841464f, -.55849f, -.308589f, .55849f, .60904f, false, .095168f},
    {"Wyvern.mesh", .952407f, -.55849f, -.308589f, .55849f, .60904f, false, .095168f},
    {"Wyvern.mesh", 1.28522f, -.55849f, -.308589f, .55849f, .60904f, false, .095168f},
    {"lich.mesh", .073802f, -.329928f, -.610156f, .335793f, .640511f, false, -.070756f},
    {"lich.mesh", .087917f, -.332446f, -.610156f, .335793f, .640511f, false, -.070756f},
    {"lich.mesh", .276672f, -.332446f, -.610156f, .335793f, .640511f, false, -.070756f},
    {"lich.mesh", .306282f, -.332446f, -.610156f, .335793f, .640511f, false, -.070756f},
    {"lich.mesh", .560469f, -.332446f, -.610156f, .417297f, .640511f, false, -.070756f},
    {"lich.mesh", .59f, -.332446f, -.610156f, .417297f, .640511f, false, -.070756f},
    {"lich.mesh", .686024f, -.332446f, -.610156f, .417297f, .640511f, false, -.070756f},
    {"lich.mesh", .71f, -.332446f, -.610156f, .417297f, .640511f, false, -.070756f},
    {"lich.mesh", .767922f, -.332446f, -.610156f, .417297f, .640511f, false, -.070756f},
    {"lich.mesh", .805918f, -.332446f, -.610156f, .417297f, .640511f, false, -.070756f},
    {"lich.mesh", .841464f, -.332446f, -.610156f, .417297f, .640511f, false, -.070756f},
    {"lich.mesh", .952407f, -.460053f, -.610156f, .417297f, .640511f, false, -.070756f},
    {"lich.mesh", 1.28522f, -.505414f, -.610156f, .417297f, .640511f, false, -.070756f},
    {"skeleton.mesh", .073802f, -.16422f, -.423196f, .166207f, .293669f, false, -.023324f},
    {"skeleton.mesh", .087917f, -.16422f, -.423196f, .166207f, .29671f, false, -.023324f},
    {"skeleton.mesh", .276672f, -.16422f, -.423196f, .166207f, .296805f, false, -.023324f},
    {"skeleton.mesh", .306282f, -.16422f, -.423196f, .166207f, .296805f, false, -.023324f},
    {"skeleton.mesh", .560469f, -.23887f, -.423196f, .221794f, .296805f, false, -.023324f},
    {"skeleton.mesh", .59f, -.23887f, -.423196f, .234667f, .296805f, false, -.023324f},
    {"skeleton.mesh", .686024f, -.23887f, -.423196f, .247509f, .296805f, false, -.023324f},
    {"skeleton.mesh", .71f, -.23887f, -.423196f, .247509f, .296805f, false, -.023324f},
    {"skeleton.mesh", .767922f, -.23887f, -.423196f, .248781f, .296805f, false, -.023324f},
    {"skeleton.mesh", .805918f, -.23887f, -.423196f, .256017f, .296805f, false, -.023324f},
    {"skeleton.mesh", .841464f, -.23887f, -.423196f, .262852f, .296805f, false, -.023324f},
    {"skeleton.mesh", .952407f, -.23887f, -.423196f, .269937f, .296805f, false, -.023324f},
    {"skeleton.mesh", 1.28522f, -.23887f, -.423196f, .269937f, .296805f, false, -.023324f}
};

// The band measured for this creature below a furniture top, or nullptr when
// that height is not a measured bed top (the full walking envelope applies).
inline const BodyBand* bodyBand(const std::string& mesh, float height)
{
    for(const BodyBand& band : bodyBands)
        if(mesh == band.name && std::abs(band.height - height) < .0001f)
            return &band;
    return nullptr;
}
}

#endif
