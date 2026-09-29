/*!
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

#include "network/ChatEventMessage.h"

#include "game/Seat.h"

#include "utils/ConfigManager.h"
#include "utils/Helper.h"

namespace
{
// Colours of the event notices: warm bone for information, amber for creatures,
// gold for skills, ember red for major events
const std::string EVENT_COLOUR_INFO = "[colour='FFE8DCC0']";
const std::string EVENT_COLOUR_MAJOR = "[colour='FFE8583A']";
const std::string EVENT_COLOUR_CREATURES = "[colour='FFE8A850']";
const std::string EVENT_COLOUR_SKILLS = "[colour='FFF2C860']";
const std::string EVENT_COLOUR_OBJECTIVES = "[colour='FFF6E0A0']";
}

ChatMessage::ChatMessage(const std::string& playerNick, const std::string& message, Seat* seat) :
    mMessage(message),
    mPlayerNick(playerNick),
    mSeat(seat)
{
}

std::string ChatMessage::getMessageAsString() const
{
    const Ogre::ColourValue& colorValue = mSeat ? mSeat->getColorValue() : ConfigManager::getSingleton().getColorFromId("");
    const std::string formatSeatColor = "[colour='" + Helper::getCEGUIColorFromOgreColourValue(colorValue) + "']";
    const std::string formatWhiteColor = "[colour='FFE8DCC0']";
    std::string messageStr = formatSeatColor + mPlayerNick + formatWhiteColor + ": " + getMessage()  + "\n";
    return messageStr;
}

EventMessage::EventMessage(const std::string& message, EventShortNoticeType type):
    mMessage(message),
    mType(type)
{
}

bool EventMessage::isMessageTooOld(float maxTimeDisplay) const
{
    return mClockCreation.getElapsedTime().asSeconds() > maxTimeDisplay;
}

std::string EventMessage::getMessageAsString()
{
    std::string eventType;
    const std::string formatWhiteColor = "[colour='FFE8DCC0']";
    switch(mType)
    {
        case EventShortNoticeType::genericGameInfo:
            eventType = "[image-size='w:16 h:16'][image='OpenDungeonsIcons/HelpIcon'] " + EVENT_COLOUR_INFO;
            break;
        case EventShortNoticeType::majorGameEvent:
            eventType = "[image-size='w:16 h:16'][image='OpenDungeonsIcons/SeatIcon'] " + EVENT_COLOUR_MAJOR;
            break;
        case EventShortNoticeType::aboutCreatures:
            eventType = "[image-size='w:16 h:16'][image='OpenDungeonsIcons/CreaturesIcon'] " + EVENT_COLOUR_CREATURES;
            break;
        default:
        case EventShortNoticeType::aboutSkills:
            eventType = "[image-size='w:16 h:16'][image='OpenDungeonsIcons/SkillIcon'] " + EVENT_COLOUR_SKILLS;
            break;
        case EventShortNoticeType::aboutObjectives:
            eventType = "[image-size='w:16 h:16'][image='OpenDungeonsIcons/ObjectivesIcon'] " + EVENT_COLOUR_OBJECTIVES;
            break;
    }
    return eventType + mMessage + formatWhiteColor + "\n";
}
