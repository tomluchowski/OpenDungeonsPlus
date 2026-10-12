"""Check that the right Alt key (AltGr) toggles the creature indicators and never counts as Ctrl.

Needs no compiler: it inspects the sources of the key bridge, the keyboard wrapper and the game mode.
"""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
bridge = (root / 'source/modes/SFMLToOISListener.cpp').read_text()
keyboard = (root / 'source/modes/Keyboard.cpp').read_text()
game_mode = (root / 'source/modes/GameMode.cpp').read_text()


def function(text, signature):
    """Return the body of the function that starts with the signature."""
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


# The window bridge reports the right Alt key as the code the game mode listens to (KC_RMENU)
assert re.search(r'sfk::RAlt\)\]\s*=\s*cgk::RightAlt;', bridge), 'RAlt must map to RightAlt'

# Both Alt keys switch the indicators, and a release missed while unfocused is recovered for both
update = function(game_mode, 'void GameMode::updateCreatureIndicatorAlt(')
assert 'OIS::KC_LMENU' in update and 'OIS::KC_RMENU' in update
frame = function(game_mode, 'void GameMode::onFrameStarted(')
assert 'isKeyDown(OIS::KC_LMENU)' in frame and 'isKeyDown(OIS::KC_RMENU)' in frame
assert 'updateCreatureIndicatorAlt(arg.key, true);' in function(game_mode, 'bool GameMode::keyPressed(')
assert 'updateCreatureIndicatorAlt(arg.key, false);' in function(game_mode, 'bool GameMode::keyReleased(')

# The fake left Control that Windows adds to AltGr must not count as Ctrl in the SFML branch
modifier = function(keyboard, 'bool Keyboard::isModifierDown(')
ctrl = modifier[modifier.index('case OIS::Keyboard::Ctrl:'):modifier.index('case OIS::Keyboard::Shift:')]
assert 'sf::Keyboard::RAlt' in ctrl, 'Ctrl must look at the right Alt key'
assert re.search(r'LControl\)\s*&&\s*!rightAltDown', ctrl), 'left Control must be ignored while right Alt is down'
assert 'sf::Keyboard::RControl' in ctrl, 'a real right Control must still count'
print('AltGr modifier check passed')
