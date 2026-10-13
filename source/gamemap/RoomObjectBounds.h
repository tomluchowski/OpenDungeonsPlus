#ifndef ROOMOBJECTBOUNDS_H
#define ROOMOBJECTBOUNDS_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

namespace RoomObjectPath
{
struct MeshBounds
{
    const char* name;
    float minX, minY, maxX, maxY;
    float maxZ = std::numeric_limits<float>::infinity();
};

// Mesh-local XY bounds measured from the shipped room furniture, not tile
// centers. Keep these synchronized with assets using check_room_object_bounds.py.
static const MeshBounds meshBounds[] = {
    {"Anvil", -.335981f, -.280187f, .341827f, .231204f},
    {"Bed", -.337495f, -.681989f, .344794f, .702338f},
    {"Bookcase", -.1694f, -.1694f, .1694f, .1694f},
    {"Bookshelf", -.52309f, -.0069419f, .530155f, .428917f},
    {"CasinoPokerTable", -.287885f, -.249315f, .287885f, .249315f},
    {"CasinoWallBeer", -.502785f, .325887f, .502785f, .493482f},
    {"CelticCross", -.391612f, -.28f, .391607f, .28f},
    {"ChickenCoop", -.203275f, -.4f, .796725f, .4f},
    {"Chimney", -.63f, .0721364f, .63f, .583136f},
    {"DragonBed", -.941213f, -.96306f, .886655f, .969738f},
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
    {"KnightCoffin", -.254848f, -.627449f, .517051f, .632155f},
    {"KnightStatue", -.293943f, -.32853f, .295722f, .325329f},
    {"KnightStatue2", -.323145f, -.358743f, .332038f, .367767f},
    {"LizardmanBed", -.32835f, -.77042f, .328337f, .769587f},
    {"OrcBed", -.533745f, -.997136f, .529433f, .987434f},
    {"Podium", -.193612f, -.148315f, .193891f, .148315f},
    {"PortalObject", -1.66302f, -1.66302f, 1.66302f, 1.66302f},
    {"RangerBed", -.53459f, -.777014f, .534418f, .775852f},
    {"Roulette", -.292486f, -.296162f, .292326f, .296652f},
    {"Skull", -.205408f, -.216924f, .205408f, .460773f},
    {"SpiderBed", -.342277f, -.52519f, .458527f, .328312f},
    {"StoneCoffin", -.425477f, -.693722f, .425477f, .693722f},
    {"TentacleBed", -.448981f, -.476366f, .461978f, .436628f},
    {"TortureObject", -.7007f, -.606824f, .7007f, .606824f},
    {"TrainingDummy1", -.248242f, -.128432f, .248296f, .330221f},
    {"TrainingDummy2", -.455f, -.170625f, .455f, .4095f},
    {"TrainingDummy3", -.263341f, -.228943f, .30858f, .121115f},
    {"TrainingDummy4", -.462458f, -.248657f, .462458f, .248657f},
    {"TrollBed", -.787227f, -.794584f, .954702f, .813162f},
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
        const float scale = std::min(1.0f, 0.4f / std::hypot(
            bounds.maxX - bounds.minX, bounds.maxY - bounds.minY));
        return {scale, scale};
    }
    float width = 0.6f, depth = 0.6f;
    const bool bed = name == "Bed" || name == "KnightCoffin" || name == "StoneCoffin" ||
        (name.size() >= 3 && name.compare(name.size() - 3, 3, "Bed") == 0);
    // This fallback also covers unassigned decorative instances. Actual beds
    // override it with the owning creature's allocated dimensions below.
    if(bed)
        width = depth = 0.4f;
    if(name == "DragonBed" || name == "TrollBed")
        width = depth = 1.2f;
    else if(name == "Bed" || name == "KnightCoffin" || name == "LizardmanBed" ||
            name == "OrcBed" || name == "RangerBed" || name == "StoneCoffin")
        depth = 1.2f;
    const float x = std::min(1.0f, width / (bounds.maxX - bounds.minX));
    const float y = std::min(1.0f, depth / (bounds.maxY - bounds.minY));
    if(bed)
        return {x, y};
    const float scale = std::min(x, y);
    return {scale, scale};
}

struct BedPlacement
{
    float x, y, angle;
    FurnitureScale scale;
};

inline BedPlacement bedPlacement(const MeshBounds& bounds, int x, int y,
    int width, int height, float allocationAngle, const std::string& creatureName)
{
    // Stable across save/load and platforms, without consuming gameplay RNG.
    std::uint32_t hash = 2166136261u;
    for(unsigned char character : creatureName)
        hash = (hash ^ character) * 16777619u;
    const float angle = allocationAngle + float(hash % 8001u) * 0.001f - 4.0f;
    const float radians = angle * 0.01745329252f;
    const float cosine = std::cos(radians), sine = std::sin(radians);
    // Fit the rotated footprint, keeping the right and bottom 30% lanes clear.
    const float c = std::abs(cosine), s = std::abs(sine);
    const float determinant = c * c - s * s;
    const FurnitureScale scale{0.70f * (width * c - height * s) /
            (determinant * (bounds.maxX - bounds.minX)),
        0.70f * (height * c - width * s) /
            (determinant * (bounds.maxY - bounds.minY))};
    float left = 1.0e10f, top = -1.0e10f;
    for(float px : {bounds.minX * scale.x, bounds.maxX * scale.x})
        for(float py : {bounds.minY * scale.y, bounds.maxY * scale.y})
        {
            left = std::min(left, cosine * px - sine * py);
            top = std::max(top, sine * px + cosine * py);
        }
    return {float(x) - 0.5f - left, float(y + height) - 0.5f - top, angle, scale};
}

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

// Skinned Walk triangles clipped below the low nest's top at level-one scale.
// Higher levels keep this conservative band, not a narrower guessed footprint.
// XY interpolation clearance matches the full walking catalog above.
constexpr float lowWalkingHeight = .073802f / 1.02f;
constexpr float lowWalkingMargin = .010001f;
struct LowWalkingBounds
{
    const char* name;
    float minX, minY, maxX, maxY;
    bool empty = false;
    float minZ = 0.0f;
};
static const LowWalkingBounds lowWalkingBounds[] = {
    {"Adventurer.mesh", -.112240f, -.294087f, .109449f, .231574f, false, -.002311f},
    {"CaveHornet.mesh", 0, 0, 0, 0, true},
    {"Cultist.mesh", -.121249f, -.335677f, .121249f, .246237f, false, -.027495f},
    {"DarkElf.mesh", -.0761223f, -.168468f, .116131f, .282344f, false, -.039261f},
    {"Defender.mesh", -.248247f, -.880927f, .130284f, .258729f, false, -.025758f},
    {"Dragon.mesh", -.132556f, -.308054f, .138703f, .244352f, false, -.004299f},
    {"Dwarf1.mesh", -.122944f, -.239296f, .122842f, .173020f, false, -.037032f},
    {"Dwarf2.mesh", -.124180f, -.238959f, .124005f, .170387f, false, -.027468f},
    {"Elf.mesh", -.0761223f, -.168468f, .116131f, .282344f, false, -.039261f},
    {"Gnome.mesh", -.0707926f, -.174931f, .0707784f, .182404f, false, -.006618f},
    {"Goblin.mesh", -.0912782f, -.198021f, .106434f, .242258f, false, -.026126f},
    {"Knight.mesh", -.196324f, -.358724f, .185533f, .223521f, false, -.022253f},
    {"Kobold.mesh", -.137907f, -.153292f, .0846171f, .141323f, false, -.011227f},
    {"Kreatur.mesh", -.467870f, -.623356f, .475000f, .537126f, false, -.020375f},
    {"LavaSpawn.mesh", -.794633f, -.627326f, .789448f, 1.118940f, false, -.052282f},
    {"Lizardman.mesh", -.108072f, -.365148f, .108144f, .293084f, false, -.003576f},
    {"Monk.mesh", -.146848f, -.265317f, .146355f, .121055f, false, -.02499f},
    {"NatureMonster.mesh", -.423010f, -.402615f, .423010f, .316681f, false, -.032419f},
    {"Orc.mesh", -.134866f, -.251628f, .135191f, .271754f, false, -.005487f},
    {"PitDemon.mesh", -.192091f, -.468387f, .192100f, .466103f, false, -.049686f},
    {"Rat.mesh", -.118970f, -.444536f, .118308f, .570713f, false, -.030563f},
    {"Roach.mesh", -.279384f, -.436215f, .279687f, .389623f, false, -.003398f},
    {"RunelordDwarf.mesh", -.144517f, -.373079f, .144374f, .207467f, false, -.008894f},
    {"Scarab.mesh", -.490259f, -.214609f, .496993f, .365987f, false, -.036622f},
    {"Slime.mesh", -.127921f, -.401148f, .127929f, .195000f, false, -.016886f},
    {"Spider.mesh", -.385552f, -.415697f, .385635f, .385744f, false, -.083595f},
    {"TentacleAlbine.mesh", -.232143f, -.294768f, .231846f, .284454f, false, -.000322f},
    {"TentacleGreen.mesh", -.232143f, -.294768f, .231846f, .284454f, false, -.000322f},
    {"Troll.mesh", -.525680f, -.390118f, .532348f, .272789f, false, -.003584f},
    {"Wizard.mesh", -.146740f, -.305335f, .131721f, .366651f, false, -.021902f},
    {"Wyvern.mesh", 0, 0, 0, 0, true},
    {"lich.mesh", -.329927f, -.610155f, .335792f, .640511f, false, -.070757f},
    {"skeleton.mesh", -.164219f, -.423195f, .166207f, .293669f, false, -.023325f}
};
}

#endif
