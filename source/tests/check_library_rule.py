"""Checks that a seat cannot cast spells while its library is lost. It does not start a game.

The rule is checked on the sources: the seat remembers that it once had a library, the server
refuses spells without one, the flag reaches the clients with the other changing seat data, and
the spell buttons are locked on the client.
"""
from pathlib import Path
import sys

repo = Path(__file__).resolve().parents[2]


def read(relative):
    return (repo / relative).read_text(encoding='utf-8')


def function(text, signature):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


failures = []


def check(condition, message):
    if not condition:
        failures.append(message)


seat_data_header = read('source/game/SeatData.h')
seat_data = read('source/game/SeatData.cpp')
seat = read('source/game/Seat.cpp')
skill_manager = read('source/game/SkillManager.cpp')
game_mode = read('source/modes/GameMode.cpp')

check('mHadLibrary && (getNbRooms(RoomType::library) == 0)' in function(seat_data, 'bool SeatData::isLibraryLost('),
      'a library is lost when the seat had one and owns none now')
check('mHadLibrary(false)' in seat_data, 'a new seat has not had a library')
check(function(seat_data, 'bool SeatData::importFromPacketForUpdate(').count('mHadLibrary') == 1
      and function(seat_data, 'void SeatData::exportToPacketForUpdate(').count('mHadLibrary') == 1,
      'the flag travels with the changing seat data, in the same position on both sides')
begin_turn = function(seat, 'void Seat::computeSeatBeginTurn(')
check('getNbRooms(RoomType::library) > 0' in begin_turn and 'mHadLibrary = true;' in begin_turn,
      'the server notes that the seat owns a library')

available = function(skill_manager, 'bool SkillManager::isSpellAvailable(')
check('isLibraryLost()' in available and 'return false;' in available[available.index('isLibraryLost()'):],
      'the server refuses spells while the library is lost')
check('SpellType::summonWorker' in available, 'summoning a worker needs no research and stays')
locked = function(skill_manager, 'bool SkillManager::isLockedByLostLibrary(')
check('SkillType::spellSummonWorker' in locked and 'SkillFamily::spells' in locked,
      'the client lock covers the same spells as the server')
check('Skills::isRewardSkill(resType)' in available and 'Skills::isRewardSkill(type)' in locked,
      'reward skills (Summon champion) come from the Heartstone, not from research, and are not locked')
check('isLockedByLostLibrary(resType, localPlayerSeat)' in game_mode
      and 'castButtonName)->setEnabled(!isLockedByLibrary)' in game_mode, 'the cast button is locked on the client')
check('mIsLibraryLostShown != localPlayerSeat->isLibraryLost()' in function(game_mode, 'void GameMode::refreshGuiSkill('),
      'the spell buttons refresh when the library is lost or regained')

# Server side spell casting goes through isSpellAvailable
check('SkillManager::isSpellAvailable(spellType, player->getSeat())' in read('source/network/ODServer.cpp'),
      'casting on the server checks isSpellAvailable')

if failures:
    for failure in failures:
        print('FAIL: ' + failure)
    sys.exit(1)
print('check_library_rule: OK')
