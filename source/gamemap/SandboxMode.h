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

#ifndef SANDBOXMODE_H
#define SANDBOXMODE_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

class GameMap;
class Player;
class Seat;

//! \brief Server side logic of the sandbox mode: a level without a rival keeper where the
//! player builds freely, takes heroes of a chosen level in the hand (hero toolbox) and calls
//! hero invasions on demand through the hero gate of the map (the wave portal).
//! Nothing here is saved: a loaded game starts again with wave 1.
class SandboxMode
{
public:
    //! \brief Number of waves of a hero invasion
    static const uint32_t NB_WAVES;
    //! \brief Number of heroes in a wave
    static const uint32_t NB_HEROES_PER_WAVE;
    //! \brief Lowest level of a hero (the level selector of the toolbox starts here)
    static const uint32_t MIN_HERO_LEVEL;
    //! \brief Highest level of a hero (the level selector of the toolbox goes up to this)
    static const uint32_t MAX_HERO_LEVEL;

    //! \brief Creates the sandbox logic of the given map. No wave is launched yet.
    SandboxMode(GameMap& gameMap);

    //! \brief Forgets the waves and the toolbox heroes
    void reset();

    //! \brief Puts a hero of the given class and level in the hand of the player. Only one
    //! hero per class can be alive at the same time.
    void takeHero(Player* player, const std::string& className, uint32_t level);

    //! \brief Starts a single wave, or all the waves one after the other if continual is true.
    //! Nothing happens while a wave is still fighting.
    void startInvasion(Player* player, bool continual);

    //! \brief Checks if the current wave is beaten and launches the next one of a continual invasion
    void doTurn();

    //! \brief Tells if the given seat belongs to the hero faction
    static bool isHeroSeat(const Seat* seat);

private:
    //! \brief The map this sandbox logic belongs to
    GameMap& mGameMap;
    //! \brief Number of waves already launched. The next one is mNbWavesLaunched + 1.
    uint32_t mNbWavesLaunched;
    //! \brief True while a continual invasion goes through its waves
    bool mIsContinual;
    //! \brief True from the launch of a wave until its last hero is dead
    bool mIsWaveActive;
    //! \brief Turns before the next wave of a continual invasion is launched
    int32_t mTurnsBeforeNextWave;
    //! \brief Turns before the heroes of the current wave are checked again
    int32_t mTurnsBeforeCheck;
    //! \brief Turns before the next room of the unlock order becomes available, -1 until the first turn
    int32_t mTurnsBeforeRoomUnlock;
    //! \brief Number of rooms of the unlock order already available
    uint32_t mNbRoomsUnlocked;
    //! \brief Names of the heroes of the current wave that are still alive
    std::vector<std::string> mWaveHeroes;
    //! \brief Name of the living toolbox hero for each class
    std::map<std::string, std::string> mToolboxHeroes;

    //! \brief Returns the first seat of the hero faction, or nullptr if the level has none
    Seat* getHeroSeat() const;
    //! \brief Makes the next room of the unlock order available once the delay is over. A level that
    //! already has every room available at the start has nothing to unlock.
    void updateRoomUnlocks();
    //! \brief Launches the given wave. Returns false if it could not be launched.
    bool launchWave(uint32_t waveNumber);
    //! \brief Sends a message to every human player
    void sendMessage(const std::string& message) const;
    //! \brief Sends a message to the given player only
    void sendMessage(Player* player, const std::string& message) const;
};

#endif // SANDBOXMODE_H
