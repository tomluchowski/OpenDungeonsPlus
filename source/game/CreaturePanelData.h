/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CREATUREPANELDATA_H
#define CREATUREPANELDATA_H

#include "entities/CreatureActivity.h"
#include "creaturemood/CreatureMood.h"

#include <array>
#include <map>
#include <string>

class ODPacket;

// Wire order for the negotiated creature-panel snapshot. Keep existing values.
enum class CreaturePanelCriterion
{
    Total, Idle, Working, Fighting, Manufacturing, Training, OtherJobs,
    Guarding, OtherFighting, Happy, Unhappy, Angry, Count
};

using CreaturePanelCounts = std::array<uint32_t, static_cast<size_t>(CreaturePanelCriterion::Count)>;
using CreaturePanelData = std::map<std::string, CreaturePanelCounts>;

bool matchesCreaturePanelCriterion(CreaturePanelCriterion criterion, const CreatureActivity& activity,
    CreatureMoodLevel mood, bool worker);
void addCreaturePanelCounts(CreaturePanelCounts& counts, const CreatureActivity& activity,
    CreatureMoodLevel mood, bool worker);
void exportCreaturePanelData(ODPacket& packet, const CreaturePanelData& data);
bool importCreaturePanelData(ODPacket& packet, CreaturePanelData& data);

#endif
