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

#include "social/SocialGenerator.h"

#include "social/SocialRng.h"

#include <sstream>

namespace social
{

namespace
{

const uint32_t SURNAME_PERCENT = 40;
const uint32_t TITLE_PERCENT = 25;
const uint32_t AGE_JOKE_PERCENT = 30;
const uint32_t SPECIFIC_LIKE_PERCENT = 60;

const std::string& pickFrom(Rng& rng, const std::vector<std::string>& pool, const std::string& fallback)
{
    if(pool.empty())
        return fallback;
    return pool[rng.below(static_cast<uint32_t>(pool.size()))];
}

//! Scopes applying to a creature. With withGeneric false, "*" is left out.
void buildScopes(const std::string& groupName, const std::string& className, bool isWorker,
    bool withGeneric, std::vector<std::string>& scopes)
{
    scopes.clear();
    if(withGeneric)
        scopes.push_back("*");
    scopes.push_back(groupName);
    scopes.push_back(className);
    scopes.push_back(isWorker ? "worker" : "fighter");
}

//! Picks a text of the pool that differs from the excluded one (a few tries, then gives up)
const std::string& pickDifferent(Rng& rng, const std::vector<std::string>& pool,
    const std::string& excluded, const std::string& fallback)
{
    const std::string* result = &pickFrom(rng, pool, fallback);
    for(uint32_t tries = 0; (tries < 8) && (*result == excluded); ++tries)
        result = &pickFrom(rng, pool, fallback);
    return *result;
}

void pickPair(Rng& rng, const std::vector<std::string>& specific, const std::vector<std::string>& all,
    const std::string& fallback, std::string* result)
{
    const std::vector<std::string>* firstPool = &all;
    if((!specific.empty()) && (rng.below(100) < SPECIFIC_LIKE_PERCENT))
        firstPool = &specific;
    result[0] = pickFrom(rng, *firstPool, fallback);
    result[1] = pickDifferent(rng, all, result[0], fallback);
}

//! Gender index of a creature: 0 female, 1 male, 2 none. Only the creature name and the weights of the group count
uint32_t makeGenderIndex(const NameGroup& group, const std::string& creatureName)
{
    Rng genderRng = makeFieldRng(creatureName, "gender");
    uint32_t totalWeight = group.mGenderWeights[0] + group.mGenderWeights[1] + group.mGenderWeights[2];
    uint32_t genderIndex = 2;
    if(totalWeight > 0)
    {
        uint32_t roll = genderRng.below(totalWeight);
        if(roll < group.mGenderWeights[0])
            genderIndex = 0;
        else if(roll < group.mGenderWeights[0] + group.mGenderWeights[1])
            genderIndex = 1;
    }
    return genderIndex;
}

std::string numberToString(int32_t value)
{
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

}

std::string SocialGenerator::makeGender(const SocialData& data, const std::string& creatureName,
    const std::string& className)
{
    const char* genderNames[3] = {"Female", "Male", ""};
    return genderNames[makeGenderIndex(data.getGroupForClass(className), creatureName)];
}

CreatureProfile SocialGenerator::makeProfile(const SocialData& data, const std::string& creatureName,
    const std::string& className, bool isWorker, uint32_t nameVariant)
{
    const NameGroup& group = data.getGroupForClass(className);
    CreatureProfile profile;
    profile.mCreatureName = creatureName;
    profile.mClassName = className;
    profile.mGroupName = group.mName;

    // Gender
    uint32_t genderIndex = makeGenderIndex(group, creatureName);
    const char* genderNames[3] = {"Female", "Male", ""};
    profile.mGender = genderNames[genderIndex];

    // First name: the pool of the gender, or the first non-empty pool
    static const std::string nameFallback = "Nameless";
    std::string variantSuffix;
    if(nameVariant > 0)
        variantSuffix = "#" + numberToString(static_cast<int32_t>(nameVariant));
    Rng givenRng = makeFieldRng(creatureName, "given" + variantSuffix);
    const std::vector<std::string>* givenPool = &group.mGiven[genderIndex];
    for(uint32_t i = 0; (givenPool->empty()) && (i < 3); ++i)
        givenPool = &group.mGiven[(genderIndex + 1 + i) % 3];
    profile.mFirstName = pickFrom(givenRng, *givenPool, nameFallback);

    // Surname or title
    Rng suffixRng = makeFieldRng(creatureName, "suffix" + variantSuffix);
    uint32_t suffixRoll = suffixRng.below(100);
    static const std::string emptyText;
    if((suffixRoll < SURNAME_PERCENT) && (!group.mSurnames.empty()))
    {
        Rng surnameRng = makeFieldRng(creatureName, "surname" + variantSuffix);
        profile.mSurname = pickFrom(surnameRng, group.mSurnames, emptyText);
    }
    else if((suffixRoll >= SURNAME_PERCENT) && (suffixRoll < SURNAME_PERCENT + TITLE_PERCENT) &&
            (!group.mTitles.empty()))
    {
        Rng titleRng = makeFieldRng(creatureName, "title" + variantSuffix);
        profile.mTitle = pickFrom(titleRng, group.mTitles, emptyText);
    }
    if(nameVariant > MAX_NAME_VARIANT)
    {
        profile.mTitle.clear();
        profile.mSurname = creatureName;
    }

    // Age
    Rng ageRng = makeFieldRng(creatureName, "age");
    uint32_t ageSpan = static_cast<uint32_t>(group.mAgeMax - group.mAgeMin) + 1;
    profile.mAge = group.mAgeMin + static_cast<int32_t>(ageRng.below(ageSpan));
    profile.mAgeText = numberToString(profile.mAge);
    Rng ageJokeRng = makeFieldRng(creatureName, "agejoke");
    if((!group.mAgeJokes.empty()) && (ageJokeRng.below(100) < AGE_JOKE_PERCENT))
        profile.mAgeText = pickFrom(ageJokeRng, group.mAgeJokes, profile.mAgeText);

    // Texts: everything below only reads the tables
    std::vector<std::string> scopesAll;
    std::vector<std::string> scopesSpecific;
    buildScopes(group.mName, className, isWorker, true, scopesAll);
    buildScopes(group.mName, className, isWorker, false, scopesSpecific);
    std::vector<std::string> candidates;
    std::vector<std::string> specificCandidates;

    data.getTexts("Relation", scopesAll, candidates);
    static const std::string relationFallback = "Single";
    Rng relationRng = makeFieldRng(creatureName, "relationship");
    profile.mRelationship = pickFrom(relationRng, candidates, relationFallback);

    static const std::string hometownFallback = "Somewhere";
    Rng hometownRng = makeFieldRng(creatureName, "hometown");
    profile.mHometown = pickFrom(hometownRng, group.mHometowns, hometownFallback);

    // The class job first, then the generic job of workers or fighters
    std::vector<std::string> classScope(1, className);
    data.getTexts("Job", classScope, candidates);
    if(candidates.empty())
    {
        std::vector<std::string> typeScope(1, isWorker ? "worker" : "fighter");
        data.getTexts("Job", typeScope, candidates);
    }
    static const std::string jobFallback = "Dungeon Resident";
    Rng jobRng = makeFieldRng(creatureName, "job");
    profile.mJob = pickFrom(jobRng, candidates, jobFallback);

    static const std::string likeFallback = "quiet days";
    data.getTexts("Like", scopesAll, candidates);
    data.getTexts("Like", scopesSpecific, specificCandidates);
    Rng likeRng = makeFieldRng(creatureName, "like");
    pickPair(likeRng, specificCandidates, candidates, likeFallback, profile.mLikes);

    static const std::string dislikeFallback = "loud noises";
    data.getTexts("Dislike", scopesAll, candidates);
    data.getTexts("Dislike", scopesSpecific, specificCandidates);
    Rng dislikeRng = makeFieldRng(creatureName, "dislike");
    pickPair(dislikeRng, specificCandidates, candidates, dislikeFallback, profile.mDislikes);

    static const std::string quirkFallback = "daydreaming";
    data.getTexts("Quirk", scopesAll, candidates);
    Rng quirkRng = makeFieldRng(creatureName, "quirk");
    profile.mQuirk = pickFrom(quirkRng, candidates, quirkFallback);

    // Bio: first template that fits the length limit, starting at the picked one
    std::map<std::string, std::string> slots;
    slots["name"] = profile.getFullName();
    slots["hometown"] = profile.mHometown;
    slots["job"] = profile.mJob;
    slots["like"] = profile.mLikes[0];
    slots["dislike"] = profile.mDislikes[0];
    slots["quirk"] = profile.mQuirk;
    data.getTexts("Bio", scopesAll, candidates);
    if(candidates.empty())
        candidates.push_back("A mysterious minion from {hometown}.");
    Rng bioRng = makeFieldRng(creatureName, "bio");
    uint32_t bioStart = bioRng.below(static_cast<uint32_t>(candidates.size()));
    std::string bio;
    for(std::size_t i = 0; i < candidates.size(); ++i)
    {
        bio = renderText(candidates[(bioStart + i) % candidates.size()], slots);
        if(bio.size() <= MAX_BIO_LENGTH)
            break;
    }
    if(bio.size() > MAX_BIO_LENGTH)
        bio.erase(MAX_BIO_LENGTH);
    profile.mBio = bio;

    return profile;
}

std::string SocialGenerator::displayClassName(const SocialData& data, const std::string& className)
{
    std::vector<std::string> scope(1, className);
    std::vector<std::string> texts;
    data.getTexts("ClassName", scope, texts);
    if(texts.empty())
        return className;
    return texts[0];
}

std::string SocialGenerator::renderText(const std::string& text, const std::map<std::string, std::string>& slots)
{
    std::string expanded;
    std::size_t position = 0;
    while(position < text.size())
    {
        char c = text[position];
        if(c == '{')
        {
            std::string::size_type close = text.find('}', position + 1);
            if(close != std::string::npos)
            {
                std::string slotName = text.substr(position + 1, close - position - 1);
                std::map<std::string, std::string>::const_iterator it = slots.find(slotName);
                if(it != slots.end())
                    expanded += it->second;
                position = close + 1;
                continue;
            }
        }
        expanded += c;
        ++position;
    }

    // Collapse double spaces, remove spaces before punctuation, trim
    std::string result;
    for(std::size_t i = 0; i < expanded.size(); ++i)
    {
        char c = expanded[i];
        if(c == ' ')
        {
            if(result.empty() || (result[result.size() - 1] == ' '))
                continue;
            result += c;
            continue;
        }
        if(((c == '.') || (c == ',') || (c == '!') || (c == '?') || (c == ';')) &&
           (!result.empty()) && (result[result.size() - 1] == ' '))
            result.erase(result.size() - 1);
        result += c;
    }
    if((!result.empty()) && (result[result.size() - 1] == ' '))
        result.erase(result.size() - 1);

    // A sentence starts with a capital letter, also when a lower case slot value begins it
    bool sentenceStart = true;
    for(std::size_t i = 0; i < result.size(); ++i)
    {
        char c = result[i];
        if(sentenceStart && (c >= 'a') && (c <= 'z'))
            result[i] = static_cast<char>(c - 'a' + 'A');
        if((c == '.') || (c == '!') || (c == '?'))
            sentenceStart = true;
        else if(c != ' ')
            sentenceStart = false;
    }
    return result;
}

uint32_t SocialGenerator::affinity(const std::string& creatureNameA, const std::string& creatureNameB)
{
    const std::string& first = (creatureNameB < creatureNameA) ? creatureNameB : creatureNameA;
    const std::string& second = (creatureNameB < creatureNameA) ? creatureNameA : creatureNameB;
    return static_cast<uint32_t>(fnv1a64(first + "|" + second) % 1000);
}

std::string SocialGenerator::moodLine(const SocialData& data, const std::string& creatureName,
    const std::string& className, bool isWorker, const std::string& state)
{
    const NameGroup& group = data.getGroupForClass(className);
    std::vector<std::string> scopes;
    buildScopes(group.mName, className, isWorker, true, scopes);
    std::vector<std::string> candidates;
    data.getTexts("MoodLine:" + state, scopes, candidates);
    static const std::string emptyText;
    Rng rng = makeFieldRng(creatureName, "mood:" + state);
    return pickFrom(rng, candidates, emptyText);
}

std::string SocialGenerator::postTemplate(const SocialData& data, const std::string& creatureName,
    const std::string& className, bool isWorker, const std::string& category, uint32_t variant)
{
    const NameGroup& group = data.getGroupForClass(className);
    std::vector<std::string> scopes;
    buildScopes(group.mName, className, isWorker, true, scopes);
    std::vector<std::string> candidates;
    data.getTexts("Post:" + category, scopes, candidates);
    static const std::string emptyText;
    Rng rng(fnv1a64(creatureName + "|post:" + category) ^ static_cast<uint64_t>(variant));
    return pickFrom(rng, candidates, emptyText);
}

std::string SocialGenerator::renderPost(const SocialData& data, const CreatureProfile& profile, bool isWorker,
    const std::string& category, uint32_t variant, int32_t level, const std::string& room)
{
    std::string text = postTemplate(data, profile.mCreatureName, profile.mClassName, isWorker, category, variant);
    if(text.empty())
        return text;

    std::map<std::string, std::string> slots;
    slots["name"] = profile.getFullName();
    slots["hometown"] = profile.mHometown;
    slots["job"] = profile.mJob;
    slots["like"] = profile.mLikes[0];
    slots["dislike"] = profile.mDislikes[0];
    slots["quirk"] = profile.mQuirk;
    if(level > 0)
    {
        std::ostringstream levelStream;
        levelStream << level;
        slots["level"] = levelStream.str();
    }
    slots["room"] = room;
    return renderText(text, slots);
}

std::string SocialGenerator::serialize(const CreatureProfile& profile)
{
    std::ostringstream stream;
    stream << profile.mCreatureName << "|" << profile.mClassName << "|" << profile.mGroupName << "|"
        << profile.mFirstName << "|" << profile.mSurname << "|" << profile.mTitle << "|"
        << profile.mAge << "|" << profile.mAgeText << "|" << profile.mGender << "|"
        << profile.mRelationship << "|" << profile.mHometown << "|" << profile.mJob << "|"
        << profile.mLikes[0] << "|" << profile.mLikes[1] << "|"
        << profile.mDislikes[0] << "|" << profile.mDislikes[1] << "|"
        << profile.mQuirk << "|" << profile.mBio;
    return stream.str();
}

}
