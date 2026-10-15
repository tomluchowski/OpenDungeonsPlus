/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "game/CreaturePanelData.h"
#include "network/ODPacket.h"

bool matchesCreaturePanelCriterion(CreaturePanelCriterion criterion, const CreatureActivity& activity,
    CreatureMoodLevel mood, bool worker)
{
    // The goal behind the current action (see CreatureActivity) decides what the creature is doing.
    const bool fighting = activity.task == CreatureActionType::fight ||
        activity.task == CreatureActionType::fightFriendly;
    // Only workers search for tiles and entities; for other creatures such a task is not idling.
    const bool workerSearching = worker &&
        (activity.task == CreatureActionType::searchTileToDig ||
         activity.task == CreatureActionType::searchGroundTileToClaim ||
         activity.task == CreatureActionType::searchWallTileToClaim ||
         activity.task == CreatureActionType::searchEntityToCarry);
    const bool idle = activity.task == CreatureActionType::nb ||
        activity.task == CreatureActionType::searchJob || workerSearching;
    const bool working = worker && (activity.task == CreatureActionType::digTile ||
        activity.task == CreatureActionType::claimGroundTile ||
        activity.task == CreatureActionType::claimWallTile ||
        activity.task == CreatureActionType::grabEntity ||
        activity.task == CreatureActionType::carryEntity);
    const bool usingRoom = activity.action == CreatureActionType::useRoom && activity.inAssignedRoom;
    const bool manufacturing = usingRoom && (activity.assignedRoom == RoomType::workshop ||
        activity.assignedRoom == RoomType::library);
    const bool inTrainingRoom = usingRoom && (activity.assignedRoom == RoomType::trainingHall ||
        activity.assignedRoom == RoomType::arena);
    // Friendly fights in the arena are training as well.
    const bool sparringInArena = activity.inAssignedRoom && activity.assignedRoom == RoomType::arena &&
        activity.task == CreatureActionType::fightFriendly;
    const bool training = inTrainingRoom || sparringInArena;
    // Mood criteria do not need a known activity; every activity criterion does.
    switch(criterion)
    {
        case CreaturePanelCriterion::Total: return true;
        case CreaturePanelCriterion::Happy:
            return mood == CreatureMoodLevel::Happy || mood == CreatureMoodLevel::Neutral;
        case CreaturePanelCriterion::Unhappy:
            return mood == CreatureMoodLevel::Upset || mood == CreatureMoodLevel::Angry;
        case CreaturePanelCriterion::Angry: return mood == CreatureMoodLevel::Furious;
        case CreaturePanelCriterion::Idle: return activity.known && idle;
        case CreaturePanelCriterion::Working: return activity.known && working;
        case CreaturePanelCriterion::Fighting: return activity.known && fighting;
        case CreaturePanelCriterion::Manufacturing: return activity.known && manufacturing;
        case CreaturePanelCriterion::Training: return activity.known && training;
        case CreaturePanelCriterion::OtherJobs: return activity.known && !idle && !manufacturing && !training;
        // The creature AI has no guarding action, so this criterion never matches.
        case CreaturePanelCriterion::Guarding: return false;
        case CreaturePanelCriterion::OtherFighting: return activity.known && !fighting;
        default: return false;
    }
}

void addCreaturePanelCounts(CreaturePanelCounts& counts, const CreatureActivity& activity,
    CreatureMoodLevel mood, bool worker)
{
    for(size_t i = 0; i < counts.size(); ++i)
        if(matchesCreaturePanelCriterion(static_cast<CreaturePanelCriterion>(i), activity, mood, worker))
            ++counts[i];
}

void exportCreaturePanelData(ODPacket& packet, const CreaturePanelData& data)
{
    packet << static_cast<uint32_t>(data.size());
    for(const std::pair<const std::string, CreaturePanelCounts>& entry : data)
    {
        packet << entry.first;
        for(uint32_t count : entry.second)
            packet << count;
    }
}

bool importCreaturePanelData(ODPacket& packet, CreaturePanelData& data)
{
    uint32_t size;
    if(!(packet >> size))
        return false;
    CreaturePanelData received;
    while(size-- > 0)
    {
        std::string name;
        CreaturePanelCounts counts{};
        if(!(packet >> name) || name.empty() || received.count(name) != 0)
            return false;
        for(uint32_t& count : counts)
            if(!(packet >> count))
                return false;
        for(uint32_t count : counts)
            if(count > counts[static_cast<size_t>(CreaturePanelCriterion::Total)])
                return false;
        received.emplace(name, counts);
    }
    data = std::move(received);
    return true;
}
