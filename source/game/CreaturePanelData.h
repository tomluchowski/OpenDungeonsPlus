/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CREATUREPANELDATA_H
#define CREATUREPANELDATA_H

#include "entities/CreatureActivity.h"
#include "creaturemood/CreatureMood.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

class ODPacket;

// Wire order for the negotiated creature-panel snapshot. Keep existing values.
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

//! Number of creatures per criterion, indexed by CreaturePanelCriterion.
using CreaturePanelCounts = std::array<uint32_t, static_cast<size_t>(CreaturePanelCriterion::Count)>;
//! Counts per creature class name.
using CreaturePanelData = std::map<std::string, CreaturePanelCounts>;

//! Whether a creature in the given state is counted for the criterion.
bool matchesCreaturePanelCriterion(CreaturePanelCriterion criterion, const CreatureActivity& activity,
    CreatureMoodLevel mood, bool worker);
//! Adds one creature to every criterion it matches.
void addCreaturePanelCounts(CreaturePanelCounts& counts, const CreatureActivity& activity,
    CreatureMoodLevel mood, bool worker);
//! Writes the snapshot to the packet.
void exportCreaturePanelData(ODPacket& packet, const CreaturePanelData& data);
//! Reads a snapshot from the packet. Returns false and leaves data untouched when the packet is malformed.
bool importCreaturePanelData(ODPacket& packet, CreaturePanelData& data);

#endif
