"""Run the actual session reset snippets repeatedly in one Python process; no build."""
from pathlib import Path
import builtins
import re
import sys
import types

ROOT = Path(__file__).resolve().parents[2]
console = (ROOT / "source/modes/GameEditorModeConsole.cpp").read_text()
application = (ROOT / "source/ODApplication.cpp").read_text()
header = (ROOT / "source/modes/GameEditorModeConsole.h").read_text()
loader = (ROOT / "source/gamemap/MapHandler.cpp").read_text()


def snippet(name):
    match = re.search(r"const char\* const " + name + r' = R"python\((.*?)\)python";', console, re.S)
    assert match, name
    return match.group(1)


assert application.count("pybind11::scoped_interpreter") == 1
assert application.index("scoped_interpreter pythonInterpreter") < application.index("ODFrameListener frameListener")
assert application.index("gil_scoped_release pythonMainThreadRelease") < application.index("ODFrameListener frameListener")
assert "scoped_interpreter" not in header
assert "mMainThreadGilRelease" not in console
assert 'PyModule_New("__main__")' in console
assert console.count("pybind11::object scope = mScriptScope;") == 2
assert "CreatureMoved::alreadyVisited.assign(mapSizeX*mapSizeY, false)" in loader
assert "GameEditorModeConsole::scriptRegister.clear()" in loader
assert loader.index("scriptRegister.clear()") < loader.index("scriptRegister[actionName]")
assert console.index("stopInterpreterThread();", console.index("::~GameEditorModeConsole")) < console.index("exec(END_SCRIPT_SESSION")

begin, end = snippet("BEGIN_SCRIPT_SESSION"), snippet("END_SCRIPT_SESSION")
original_modules = dict(sys.modules)
original_main = sys.modules["__main__"]
original_path = list(sys.path)
original_argv = list(sys.argv)
original_streams = (sys.stdin, sys.stdout, sys.stderr, sys.displayhook)
for name in ("cheats", "my_sys"):
    module = types.ModuleType(name)
    module.command = "original"
    sys.modules[name] = module

for load in range(12):
    temporary = {}
    exec(begin, temporary)
    state = temporary.pop("_session_state")
    game_module = types.ModuleType("__main__")
    scope = vars(game_module)
    sys.modules["__main__"] = game_module
    assert "old_game" not in scope
    assert "old_level_module" not in sys.modules
    assert not hasattr(builtins, "old_game")
    assert sys.modules["cheats"].command == "original"
    if load % 2 == 0:  # alternate scripted and script-free saves/levels
        exec("old_game = 42\nimport __main__\nassert __main__.old_game == 42", scope)
        sys.modules["old_level_module"] = types.ModuleType("old_level_module")
        builtins.old_game = 42
        builtins._ = 42
        sys.modules["cheats"].command = "previous game"
        sys.path.append("previous-game-path")
        sys.argv.append("previous-game-argument")
        sys.displayhook = lambda obj: None
    scope.clear()
    scope["_session_state"] = state
    exec(end, scope)
    assert sys.modules["__main__"] is original_main
    assert sys.path == original_path and sys.argv == original_argv
    assert (sys.stdin, sys.stdout, sys.stderr, sys.displayhook) == original_streams
    assert "old_level_module" not in sys.modules
    assert not hasattr(builtins, "old_game")
    assert sys.modules["cheats"].command == "original"
    scope.clear()
sys.modules.clear()
sys.modules.update(original_modules)
print("script reload: 12 same-process sessions, scripted and script-free state isolation, lifecycle wiring OK")
