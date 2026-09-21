/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "game/CreaturePanelData.h"
#include "network/ODPacket.h"

bool matchesCreaturePanelCriterion(CreaturePanelCriterion criterion, const CreatureActivity& activity,
    CreatureMoodLevel mood, bool worker)
{
    if(criterion == CreaturePanelCriterion::Total)
        return true;
    if(criterion == CreaturePanelCriterion::Happy)
        return mood == CreatureMoodLevel::Happy || mood == CreatureMoodLevel::Neutral;
    if(criterion == CreaturePanelCriterion::Unhappy)
        return mood == CreatureMoodLevel::Upset || mood == CreatureMoodLevel::Angry;
    if(criterion == CreaturePanelCriterion::Angry)
        return mood == CreatureMoodLevel::Furious;
    if(!activity.known)
        return false;

    const bool fighting = activity.task == CreatureActionType::fight ||
        activity.task == CreatureActionType::fightFriendly;
    const bool idle = activity.task == CreatureActionType::nb ||
        activity.task == CreatureActionType::searchJob || (worker &&
        (activity.task == CreatureActionType::searchTileToDig ||
         activity.task == CreatureActionType::searchGroundTileToClaim ||
         activity.task == CreatureActionType::searchWallTileToClaim ||
         activity.task == CreatureActionType::searchEntityToCarry));
    const bool usingRoom = activity.action == CreatureActionType::useRoom && activity.inAssignedRoom;
    const bool manufacturing = usingRoom && (activity.assignedRoom == RoomType::workshop ||
        activity.assignedRoom == RoomType::library);
    const bool training = (usingRoom && (activity.assignedRoom == RoomType::trainingHall ||
        activity.assignedRoom == RoomType::arena)) || (activity.inAssignedRoom &&
        activity.assignedRoom == RoomType::arena && activity.task == CreatureActionType::fightFriendly);
    switch(criterion)
    {
        case CreaturePanelCriterion::Idle: return idle;
        case CreaturePanelCriterion::Working:
            return worker && (activity.task == CreatureActionType::digTile ||
                activity.task == CreatureActionType::claimGroundTile ||
                activity.task == CreatureActionType::claimWallTile ||
                activity.task == CreatureActionType::grabEntity ||
                activity.task == CreatureActionType::carryEntity);
        case CreaturePanelCriterion::Fighting: return fighting;
        case CreaturePanelCriterion::Manufacturing: return manufacturing;
        case CreaturePanelCriterion::Training: return training;
        case CreaturePanelCriterion::OtherJobs: return !idle && !manufacturing && !training;
        case CreaturePanelCriterion::Guarding: return false;
        case CreaturePanelCriterion::OtherFighting: return !fighting;
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
    for(const auto& entry : data)
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
