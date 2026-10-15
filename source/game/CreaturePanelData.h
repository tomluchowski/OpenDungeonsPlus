/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CREATUREPANELDATA_H
#define CREATUREPANELDATA_H

#include "entities/CreatureActivity.h"
#include "creaturemood/CreatureMood.h"

#include <array>
#include <cstdint>
#include <map>
#include <string>

class ODPacket;

// Wire order for the negotiated creature-panel snapshot. Keep existing values.
// Each value is one counter per creature type. All counters are subsets of Total.
enum class CreaturePanelCriterion
{
    //! All creatures of the type that are on the map
    Total,
    //! Without a task, or (workers) searching for something to do
    Idle,
    //! (workers only) Digging, claiming or carrying
    Working,
    //! Fighting an enemy or a friendly creature
    Fighting,
    //! Using a workshop or a library
    Manufacturing,
    //! Using a training hall or an arena, or sparring in the arena
    Training,
    //! Everything that is neither idle, manufacturing nor training
    OtherJobs,
    //! Always zero: there is no guarding action
    Guarding,
    //! Everything that is not fighting
    OtherFighting,
    //! Mood happy or neutral
    Happy,
    //! Mood upset or angry
    Unhappy,
    //! Mood furious
    Angry,
    //! Number of criteria, not a criterion
    Count
};

//! One counter per CreaturePanelCriterion, indexed by the criterion value
using CreaturePanelCounts = std::array<uint32_t, static_cast<size_t>(CreaturePanelCriterion::Count)>;
//! The counters of one player's creatures, by creature class name
using CreaturePanelData = std::map<std::string, CreaturePanelCounts>;

//! \brief Tells if a creature with the given activity and mood is counted for the criterion.
//! Mood criteria also work with an unknown activity; all other criteria need CreatureActivity::known.
bool matchesCreaturePanelCriterion(CreaturePanelCriterion criterion, const CreatureActivity& activity,
    CreatureMoodLevel mood, bool worker);
//! \brief Increments every counter of counts that matches the creature
void addCreaturePanelCounts(CreaturePanelCounts& counts, const CreatureActivity& activity,
    CreatureMoodLevel mood, bool worker);
//! \brief Writes the snapshot to the packet (server side)
void exportCreaturePanelData(ODPacket& packet, const CreaturePanelData& data);
//! \brief Reads a snapshot from the packet (client side). Returns false and leaves data untouched
//! if the packet is truncated or inconsistent (duplicate or empty type, counter above Total).
bool importCreaturePanelData(ODPacket& packet, CreaturePanelData& data);

#endif
