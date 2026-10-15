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

#include "network/CosmeticEvent.h"

bool CosmeticEvent::isKnownType() const
{
    return (mType >= static_cast<int32_t>(CosmeticEventType::moodStage)) &&
           (mType <= static_cast<int32_t>(CosmeticEventType::portalArrival));
}

std::string CosmeticEvent::typeString() const
{
    switch(mType)
    {
        case static_cast<int32_t>(CosmeticEventType::moodStage):
            return "moodStage";
        case static_cast<int32_t>(CosmeticEventType::scared):
            return "scared";
        case static_cast<int32_t>(CosmeticEventType::impatient):
            return "impatient";
        case static_cast<int32_t>(CosmeticEventType::treasuryFull):
            return "treasuryFull";
        case static_cast<int32_t>(CosmeticEventType::carriedGold):
            return "carriedGold";
        case static_cast<int32_t>(CosmeticEventType::meleeResult):
            return "meleeResult";
        case static_cast<int32_t>(CosmeticEventType::missileLaunch):
            return "missileLaunch";
        case static_cast<int32_t>(CosmeticEventType::digFinished):
            return "digFinished";
        case static_cast<int32_t>(CosmeticEventType::portalArrival):
            return "portalArrival";
        default:
            break;
    }
    return "unknown";
}

ODPacket& operator<<(ODPacket& os, const CosmeticEvent& event)
{
    os << event.mType << event.mSubject << event.mObject << event.mText
       << event.mValue << event.mValue2 << event.mPosition;
    return os;
}

ODPacket& operator>>(ODPacket& is, CosmeticEvent& event)
{
    is >> event.mType >> event.mSubject >> event.mObject >> event.mText
       >> event.mValue >> event.mValue2 >> event.mPosition;
    return is;
}
