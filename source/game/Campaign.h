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

#ifndef CAMPAIGN_H
#define CAMPAIGN_H

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <mutex>
#include <string>
#include <vector>

//! \brief One level of the campaign sequence.
//! The level file name is relative to the "levels" directory.
struct CampaignLevel
{
    CampaignLevel():
        mBonus(false),
        mHeartstone(true),
        mDifficulty(1)
    {
    }

    std::string mFile;
    std::string mTitle;
    std::string mBriefing;
    std::string mDebriefing;
    //! A bonus (secret) level does not block the main sequence. Finding one
    //! gives a piece of the Heartstone.
    bool mBonus;
    //! Whether a bonus level counts as a piece of the Heartstone. A bonus level
    //! with its own reward sets this to false.
    bool mHeartstone;
    //! Id of the province (or of the bonus site) of the level on the world map
    std::string mProvince;
    //! Province id of the branch sister: of two sisters completing one unlocks
    //! the levels after them, the other one stays playable. Empty if none.
    std::string mBranch;
    //! Ruler of the province, shown in its tooltip. Empty if it has none.
    std::string mWarden;
    //! Difficulty of the level from 1 (easy) to 5, shown in the tooltip
    uint32_t mDifficulty;
};

//! \brief The campaign: an ordered list of levels plus the saved progress.
//!
//! Definition file (levels/campaign/Campaign.cfg), one setting per line:
//!   # comment
//!   [Level]
//!   File=campaign/Level1.level
//!   Title=Some title
//!   Briefing=Text shown before the level. "\n" starts a new line.
//!   Debriefing=Text shown after the level was won.
//!   Bonus=1 (optional) marks a bonus level.
//!   Heartstone=0 (optional) a bonus level that is not a piece of the Heartstone.
//!   Province=T01 (optional) the province or bonus site of the level on the
//!   world map (gui/campaign/campaign-world.json). Levels without it are not
//!   shown on the map.
//!   Branch=T09B (optional) the province of the branch sister. Both sisters
//!   follow each other in the file and name each other.
//!   Warden=Name (optional) the ruler of the province.
//!   Difficulty=1 to 5 (optional) the difficulty shown on the map.
//!
//! A bonus level is hidden until a level script finds it (action "discover",
//! the level file name as argument). It is not needed to finish the campaign,
//! and each discovered bonus level is one piece of the Heartstone.
//!
//! Progress file (written by the game):
//!   Completed <index> [<index> ...]
//!   Discovered <index> [<index> ...]
//!   Difficulty <n>   (index of the AI level for the whole campaign, 0 = easy)
//! A level is unlocked when all main levels before it are completed
//! (of two branch sisters one is enough); a bonus
//! level is unlocked when it was discovered. The current
//! level is the first level that is not completed yet.
//!
//! The campaign is a process wide singleton because the single player server
//! (which detects the victory) and the menu (which shows the result) live in
//! the same process.
class Campaign
{
public:
    static Campaign& getSingleton();

    //! \brief Reads the level sequence. Returns false if no level was found.
    bool importDefinition(std::istream& is);
    //! \brief Reads the saved progress. Unknown or out of range indices are ignored.
    bool importProgress(std::istream& is);
    void exportProgress(std::ostream& os) const;

    //! \brief Sets the file the progress is written to by saveProgress() and
    //! onLevelWon(). Without a path nothing is written.
    void setProgressPath(const std::string& path);
    bool saveProgress() const;

    size_t getNumLevels() const;
    const CampaignLevel& getLevel(size_t index) const;
    //! \brief Index of the level on the province or bonus site with that id,
    //! or getNumLevels() if there is none.
    size_t findLevelByProvince(const std::string& province) const;
    bool isCompleted(size_t index) const;
    bool isUnlocked(size_t index) const;
    //! \brief First main (non bonus) level not completed. Equals the number of levels if all are done.
    size_t getCurrentLevel() const;
    bool isFinished() const;
    //! \brief A level script found the bonus level with that file name. Only
    //! counts while a campaign level is played. Returns true if it is newly
    //! discovered (progress is saved).
    bool discoverBonusLevel(const std::string& file);
    bool isDiscovered(size_t index) const;
    //! \brief Number of discovered bonus levels (found Heartstone pieces).
    size_t getHeartstonePieces() const;
    //! \brief Number of bonus levels (Heartstone pieces to find).
    size_t getHeartstoneTotal() const;
    //! \brief True if the campaign has bonus levels and all were discovered.
    bool isHeartstoneComplete() const;
    //! \brief The kept minion special was used in the level that is played: the creature (class and level)
    //! comes along to the next level, if this level is won.
    void setKeptMinion(const std::string& className, uint32_t level);
    //! \brief The creature that came along from the last level won. Returns false if there is none; the
    //! creature is taken (it comes to one level only).
    bool takeKeptMinion(std::string& className, uint32_t& level);
    //! \brief True if any level was completed (a campaign can be continued).
    bool hasProgress() const;
    //! \brief Forgets all progress (New Campaign).
    void resetProgress();

    //! \brief AI difficulty of the whole campaign, chosen when a new campaign
    //! is begun. The value is an index into KeeperAIType.
    uint32_t getDifficulty() const;
    //! \brief Sets the difficulty and saves the progress. Values above the
    //! highest AI level are ignored.
    void setDifficulty(uint32_t difficulty);
    //! \brief Default difficulty (normal).
    static uint32_t getDefaultDifficulty();

    //! \brief A campaign level was started from the campaign menu.
    void startLevel(size_t index);
    //! \brief Leaving the campaign (the main menu is shown).
    void stopCampaign();
    bool isActive() const;
    //! \brief The level that was started last, or getNumLevels() if none.
    size_t getPlayedLevel() const;
    bool getPlayedLevelWon() const;
    //! \brief Clears the "last played" result once the menu has shown it.
    void clearPlayedLevel();

    //! \brief The statistics of the level that was just won, as text lines. Set by the client when the
    //! server sends them at the victory, shown by the campaign menu. Empty if there are none.
    void setLevelSummary(const std::string& summary);
    std::string getLevelSummary() const;

    //! \brief Called by the server when a human seat completed all its goals.
    //! Marks the played level as completed and saves the progress.
    //! Returns true if a campaign level was won.
    bool onLevelWon();

    //! \brief Replaces "\n" (backslash, n) by a line feed.
    static std::string unescapeText(const std::string& text);

private:
    Campaign();

    mutable std::mutex mMutex;
    std::vector<CampaignLevel> mLevels;
    std::vector<bool> mCompleted;
    std::vector<bool> mDiscovered;
    std::string mProgressPath;
    bool mActive;
    size_t mPlayedLevel;
    bool mPlayedLevelWon;
    std::string mLevelSummary;
    uint32_t mDifficulty;
    //! The creature chosen by the kept minion special in the level that is played, and the one that came
    //! from the last level won (saved)
    std::string mKeptPendingClass;
    uint32_t mKeptPendingLevel;
    std::string mKeptClass;
    uint32_t mKeptLevel;

    size_t getCurrentLevelNoLock() const;
    bool isUnlockedNoLock(size_t index) const;
    size_t findBranchSisterNoLock(size_t index) const;
};

#endif // CAMPAIGN_H
