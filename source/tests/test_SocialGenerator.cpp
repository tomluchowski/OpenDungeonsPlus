/*
 *  Copyright (C) 2011-2016  OpenDungeons Team
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "social/SocialData.h"
#include "social/SocialGenerator.h"
#include "social/SocialRng.h"

#include <fstream>
#include <sstream>

#define BOOST_TEST_MODULE SocialGenerator
#include "BoostTestTargetConfig.h"

namespace
{

//! The data files live in ../../config relative to this source file
std::string getConfigDirectory()
{
    std::string path = __FILE__;
    std::string::size_type slash = path.find_last_of("/\\");
    std::string testsDirectory = (slash == std::string::npos) ? std::string(".") : path.substr(0, slash);
    return testsDirectory + "/../../config/";
}

std::string getGoldenFile()
{
    std::string path = __FILE__;
    std::string::size_type slash = path.find_last_of("/\\");
    std::string testsDirectory = (slash == std::string::npos) ? std::string(".") : path.substr(0, slash);
    return testsDirectory + "/social-golden.txt";
}

bool isWorkerClass(const std::string& className)
{
    return (className == "Kobold") || (className == "DwarfWorker");
}

std::string goldenLine(const social::SocialData& data, const std::string& name, const std::string& className)
{
    social::CreatureProfile profile = social::SocialGenerator::makeProfile(data, name, className,
        isWorkerClass(className));
    std::ostringstream stream;
    stream << profile.mFirstName << "|" << profile.mAge << "|" << profile.mHometown << "|" << profile.mJob;
    return stream.str();
}

void checkNoEmptyField(const social::CreatureProfile& profile, bool distinctPairs)
{
    BOOST_CHECK_MESSAGE(!profile.mFirstName.empty(), profile.mCreatureName);
    BOOST_CHECK_MESSAGE(!profile.mAgeText.empty(), profile.mCreatureName);
    BOOST_CHECK_MESSAGE((profile.mGender == "Female") || (profile.mGender == "Male") || profile.mGender.empty(), profile.mCreatureName);
    BOOST_CHECK_MESSAGE(!profile.mRelationship.empty(), profile.mCreatureName);
    BOOST_CHECK_MESSAGE(!profile.mHometown.empty(), profile.mCreatureName);
    BOOST_CHECK_MESSAGE(!profile.mJob.empty(), profile.mCreatureName);
    BOOST_CHECK_MESSAGE(!profile.mLikes[0].empty(), profile.mCreatureName);
    BOOST_CHECK_MESSAGE(!profile.mLikes[1].empty(), profile.mCreatureName);
    BOOST_CHECK_MESSAGE(!profile.mDislikes[0].empty(), profile.mCreatureName);
    BOOST_CHECK_MESSAGE(!profile.mDislikes[1].empty(), profile.mCreatureName);
    BOOST_CHECK_MESSAGE(!profile.mQuirk.empty(), profile.mCreatureName);
    BOOST_CHECK_MESSAGE(!profile.mBio.empty(), profile.mCreatureName);
    BOOST_CHECK_MESSAGE(profile.mBio.size() <= social::SocialGenerator::MAX_BIO_LENGTH, profile.mCreatureName);
    BOOST_CHECK_MESSAGE(profile.mBio.find('{') == std::string::npos, profile.mCreatureName);
    BOOST_CHECK_MESSAGE(profile.mBio.find('}') == std::string::npos, profile.mCreatureName);
    if(distinctPairs)
    {
        BOOST_CHECK_MESSAGE(profile.mLikes[0] != profile.mLikes[1], profile.mCreatureName);
        BOOST_CHECK_MESSAGE(profile.mDislikes[0] != profile.mDislikes[1], profile.mCreatureName);
    }
}

}

BOOST_AUTO_TEST_CASE(test_SocialRng)
{
    // Reference values of FNV-1a 64 and splitmix64 (seed 0)
    BOOST_CHECK(social::fnv1a64("") == 0xcbf29ce484222325ULL);
    BOOST_CHECK(social::fnv1a64("a") == 0xaf63dc4c8601ec8cULL);
    social::Rng rng(0);
    BOOST_CHECK(rng.next() == 0xE220A8397B1DCDAFULL);
    BOOST_CHECK(rng.next() == 0x6E789E6AA1B965F4ULL);
    BOOST_CHECK(rng.next() == 0x06C45D188009454FULL);
    BOOST_CHECK(rng.below(0) == 0);
    for(uint32_t i = 0; i < 100; ++i)
        BOOST_CHECK(rng.below(7) < 7);
}

BOOST_AUTO_TEST_CASE(test_SocialDataLoads)
{
    social::SocialData data;
    BOOST_CHECK(data.loadFromDirectory(getConfigDirectory()));
    for(std::size_t i = 0; i < data.getErrors().size(); ++i)
        BOOST_TEST_MESSAGE(data.getErrors()[i]);
    BOOST_CHECK(data.getErrors().empty());
    BOOST_CHECK_EQUAL(data.getGroups().size(), 11u);
    BOOST_CHECK(data.hasGroupForClass("Kobold"));
    BOOST_CHECK(data.hasGroupForClass("Kreatur"));
    BOOST_CHECK(!data.hasGroupForClass("NoSuchClass"));
    BOOST_CHECK_EQUAL(social::SocialGenerator::displayClassName(data, "CaveHornet"), "Cave Hornet");
    BOOST_CHECK_EQUAL(social::SocialGenerator::displayClassName(data, "NoSuchClass"), "NoSuchClass");
}

BOOST_AUTO_TEST_CASE(test_SocialDeterminism)
{
    social::SocialData data;
    data.loadFromDirectory(getConfigDirectory());
    std::string first = social::SocialGenerator::serialize(
        social::SocialGenerator::makeProfile(data, "Orc17", "Orc", false));
    std::string second = social::SocialGenerator::serialize(
        social::SocialGenerator::makeProfile(data, "Orc17", "Orc", false));
    BOOST_CHECK_EQUAL(first, second);

    // After a reload of the data, and with a second instance
    data.loadFromDirectory(getConfigDirectory());
    std::string afterReload = social::SocialGenerator::serialize(
        social::SocialGenerator::makeProfile(data, "Orc17", "Orc", false));
    BOOST_CHECK_EQUAL(first, afterReload);
    social::SocialData otherData;
    otherData.loadFromDirectory(getConfigDirectory());
    std::string otherInstance = social::SocialGenerator::serialize(
        social::SocialGenerator::makeProfile(otherData, "Orc17", "Orc", false));
    BOOST_CHECK_EQUAL(first, otherInstance);

    // Another name gives another profile
    std::string other = social::SocialGenerator::serialize(
        social::SocialGenerator::makeProfile(data, "Orc18", "Orc", false));
    BOOST_CHECK(first != other);
}

BOOST_AUTO_TEST_CASE(test_SocialNoEmptyField)
{
    social::SocialData data;
    data.loadFromDirectory(getConfigDirectory());

    // All classes of all groups plus a class that is not in any group
    std::vector<std::string> classes;
    for(std::size_t i = 0; i < data.getGroups().size(); ++i)
    {
        for(std::size_t j = 0; j < data.getGroups()[i].mClasses.size(); ++j)
            classes.push_back(data.getGroups()[i].mClasses[j]);
    }
    classes.push_back("FutureCreature");
    BOOST_REQUIRE(classes.size() > 30);

    uint32_t count = 0;
    while(count < 10000)
    {
        for(std::size_t i = 0; (i < classes.size()) && (count < 10000); ++i)
        {
            std::ostringstream name;
            name << classes[i] << (count / classes.size() + 1);
            social::CreatureProfile profile = social::SocialGenerator::makeProfile(data, name.str(),
                classes[i], isWorkerClass(classes[i]));
            checkNoEmptyField(profile, true);
            BOOST_CHECK(profile.mAge > 0);
            ++count;
        }
    }
    BOOST_CHECK(data.getUnmappedClasses().size() == 1);
}

BOOST_AUTO_TEST_CASE(test_SocialGolden)
{
    social::SocialData data;
    data.loadFromDirectory(getConfigDirectory());

    // Each line: name TAB class TAB firstName|age|hometown|job
    std::ifstream file(getGoldenFile().c_str());
    BOOST_REQUIRE(file.is_open());
    std::string line;
    uint32_t count = 0;
    while(std::getline(file, line))
    {
        if((!line.empty()) && (line[line.size() - 1] == '\r'))
            line.erase(line.size() - 1);
        if(line.empty() || (line[0] == '#'))
            continue;
        std::string::size_type firstTab = line.find('\t');
        std::string::size_type secondTab = line.find('\t', firstTab + 1);
        BOOST_REQUIRE(secondTab != std::string::npos);
        std::string name = line.substr(0, firstTab);
        std::string className = line.substr(firstTab + 1, secondTab - firstTab - 1);
        std::string expected = line.substr(secondTab + 1);
        BOOST_CHECK_EQUAL(goldenLine(data, name, className), expected);
        ++count;
    }
    BOOST_CHECK_EQUAL(count, 50u);
}

BOOST_AUTO_TEST_CASE(test_SocialSlots)
{
    std::map<std::string, std::string> slots;
    slots["hometown"] = "Mudcrest";
    slots["like"] = "warm soup";
    slots["empty"] = "";
    BOOST_CHECK_EQUAL(social::SocialGenerator::renderText("From {hometown}, loves {like}.", slots),
        "From Mudcrest, loves warm soup.");
    // Unknown and empty slots are removed, spaces cleaned up
    BOOST_CHECK_EQUAL(social::SocialGenerator::renderText("Loves {unknown} and {empty} things .", slots),
        "Loves and things.");
    BOOST_CHECK_EQUAL(social::SocialGenerator::renderText("  {empty}  ", slots), "");
    // Unterminated slot and stray closing brace do not crash
    BOOST_CHECK_EQUAL(social::SocialGenerator::renderText("Open {hometown", slots), "Open {hometown");
    BOOST_CHECK_EQUAL(social::SocialGenerator::renderText("Closed }", slots), "Closed }");
    BOOST_CHECK_EQUAL(social::SocialGenerator::renderText("", slots), "");
    // Sentences start with a capital letter
    BOOST_CHECK_EQUAL(social::SocialGenerator::renderText("{like} is nice. {like}! Yes? #tag", slots),
        "Warm soup is nice. Warm soup! Yes? #tag");
}

BOOST_AUTO_TEST_CASE(test_SocialAffinity)
{
    BOOST_CHECK_EQUAL(social::SocialGenerator::affinity("Orc1", "Goblin3"),
        social::SocialGenerator::affinity("Goblin3", "Orc1"));
    BOOST_CHECK(social::SocialGenerator::affinity("Orc1", "Goblin3") < 1000);
}

BOOST_AUTO_TEST_CASE(test_SocialMoodAndPosts)
{
    social::SocialData data;
    data.loadFromDirectory(getConfigDirectory());
    const char* states[] = {"Hungry", "Tired", "GetFee", "LeaveDungeon", "KoTemp", "InJail", "Happy",
        "Neutral", "Upset", "Angry", "Furious", "Unknown"};
    for(std::size_t i = 0; i < sizeof(states) / sizeof(states[0]); ++i)
    {
        std::string line = social::SocialGenerator::moodLine(data, "Orc17", "Orc", false, states[i]);
        BOOST_CHECK_MESSAGE(!line.empty(), states[i]);
        BOOST_CHECK_EQUAL(line, social::SocialGenerator::moodLine(data, "Orc17", "Orc", false, states[i]));
    }
    BOOST_CHECK(social::SocialGenerator::moodLine(data, "Orc17", "Orc", false, "NoSuchState").empty());
    BOOST_CHECK(!social::SocialGenerator::postTemplate(data, "Orc17", "Orc", false, "eat", 3).empty());
    BOOST_CHECK_EQUAL(social::SocialGenerator::postTemplate(data, "Orc17", "Orc", false, "eat", 3),
        social::SocialGenerator::postTemplate(data, "Orc17", "Orc", false, "eat", 3));
}

BOOST_AUTO_TEST_CASE(test_SocialFallbacks)
{
    // Missing directory: errors are collected, profiles still work
    social::SocialData missing;
    BOOST_CHECK(!missing.loadFromDirectory("no-such-directory"));
    BOOST_CHECK(missing.getErrors().size() >= 2);
    checkNoEmptyField(social::SocialGenerator::makeProfile(missing, "Orc17", "Orc", false), false);

    // A names file without any group
    social::SocialData noGroup;
    std::istringstream emptyNames("# only a comment\n");
    BOOST_CHECK(!noGroup.loadNames(emptyNames, "empty-names"));
    std::istringstream emptyTexts("# only a comment\n");
    BOOST_CHECK(!noGroup.loadTexts(emptyTexts, "empty-texts"));
    checkNoEmptyField(social::SocialGenerator::makeProfile(noGroup, "Kobold4", "Kobold", true), false);

    // Broken lines are reported and skipped, the rest is used
    social::SocialData broken;
    std::istringstream brokenNames(
        "Given\tF\tOutside\n"
        "[NameGroup]\n"
        "Group\ttest\n"
        "Classes\tTestClass\n"
        "AgeRange\tten\t20\n"
        "Given\tQ\tWrong\n"
        "Given\tX\tOnly\n"
        "Bogus\tvalue\n"
        "[/NameGroup]\n"
        "[NameGroup]\n"
        "Group\tunfinished\n");
    BOOST_CHECK(!broken.loadNames(brokenNames, "broken-names"));
    BOOST_CHECK(broken.getErrors().size() >= 5);
    social::CreatureProfile profile = social::SocialGenerator::makeProfile(broken, "Test1", "TestClass", false);
    BOOST_CHECK_EQUAL(profile.mFirstName, "Only");
    BOOST_CHECK_EQUAL(profile.mGroupName, "test");
    BOOST_CHECK_EQUAL(profile.mHometown, "Somewhere");
    BOOST_CHECK(!broken.hasGroupForClass("unfinished"));
    std::istringstream brokenTexts(
        "Like\t*\n"
        "Unknown\t*\ttext\n"
        "Like\t*\tcake\n"
        "Post\teat\t*\n");
    BOOST_CHECK(!broken.loadTexts(brokenTexts, "broken-texts"));
    BOOST_CHECK_EQUAL(broken.getTextCount("Like"), 1u);
    BOOST_CHECK_EQUAL(social::SocialGenerator::makeProfile(broken, "Test1", "TestClass", false).mLikes[0], "cake");
}

BOOST_AUTO_TEST_CASE(test_SocialNameVariants)
{
    social::SocialData data;
    data.loadFromDirectory(getConfigDirectory());
    social::CreatureProfile base = social::SocialGenerator::makeProfile(data, "Orc17", "Orc", false);
    social::CreatureProfile variantZero = social::SocialGenerator::makeProfile(data, "Orc17", "Orc", false, 0);
    BOOST_CHECK_EQUAL(social::SocialGenerator::serialize(base), social::SocialGenerator::serialize(variantZero));

    // A variant is deterministic and keeps everything except the name
    social::CreatureProfile variant = social::SocialGenerator::makeProfile(data, "Orc17", "Orc", false, 3);
    BOOST_CHECK_EQUAL(social::SocialGenerator::serialize(variant),
        social::SocialGenerator::serialize(social::SocialGenerator::makeProfile(data, "Orc17", "Orc", false, 3)));
    BOOST_CHECK_EQUAL(variant.mGender, base.mGender);
    BOOST_CHECK_EQUAL(variant.mAge, base.mAge);
    BOOST_CHECK_EQUAL(variant.mHometown, base.mHometown);

    // Beyond the table variants the creature name makes the full name unique
    social::CreatureProfile fallback = social::SocialGenerator::makeProfile(data, "Orc17", "Orc", false,
        social::SocialGenerator::MAX_NAME_VARIANT + 1);
    BOOST_CHECK_EQUAL(fallback.mSurname, "Orc17");
    BOOST_CHECK(fallback.mTitle.empty());
}
