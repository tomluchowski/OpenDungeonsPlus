"""Exercise actual CMake resource generation without starting the game."""
import argparse
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--resources', type=Path, help='Also validate a real generated build configuration')
args = parser.parse_args()
source = (repo / 'CMakeLists.txt').read_text()
start = source.index('set(OD_OGRE_MEDIA_DIR ')
end = source.index('\n', source.index('configure_file(${CMAKE_CONFIG_DIR}/resources.cfg.in', start))
generation = source[start:end]
checks = 0


def check(condition, message):
    global checks
    checks += 1
    assert condition, message


def paths(config):
    result = {}
    group = ''
    for line in config.read_text().splitlines():
        if line.startswith('['):
            group = line.strip('[]')
        elif line.startswith('FileSystem='):
            result.setdefault(group, []).append(Path(line.split('=', 1)[1]))
    return result


with tempfile.TemporaryDirectory(prefix='odp-resource-generation-') as directory:
    work = Path(directory)
    media = work / 'SDK with spaces/Media'
    prefix = work / 'not-installed'
    for name in ('RTShaderLib/GLSL', 'Main'):
        (media / name).mkdir(parents=True)
    script = work / 'check.cmake'

    def configure(windows):
        script.write_text(f'set(WIN32 {"TRUE" if windows else "FALSE"})\n'
                          f'set(OGRE_MEDIA_DIR "{media.as_posix()}")\n'
                          f'set(CMAKE_INSTALL_PREFIX "{prefix.as_posix()}")\n'
                          f'set(CMAKE_CONFIG_DIR "{(repo / "cmake/config").as_posix()}")\n'
                          f'set(CMAKE_BINARY_DIR "{work.as_posix()}")\n' + generation)
        return subprocess.run(['cmake', '-P', str(script)], capture_output=True, text=True)

    for repeat in range(2):
        check(configure(True).returncode == 0, 'Windows configuration succeeds repeatedly')
        generated = paths(work / 'resources.cfg')
        graphics = [path.resolve() for path in generated['Graphics'] if path.is_absolute()]
        check(graphics == [media / 'RTShaderLib', media / 'RTShaderLib/GLSL', media / 'Main'],
              'Windows uses installed media, excluding absent optional shader folders')
        check([path.resolve() for path in generated['OgreInternal']] == [media / 'Main'],
              'Internal shadow programs use the same installed Main directory')
        check(Path('models') in generated['Graphics'] and Path('gui') in generated['GUI'],
              'Existing relative game resources are preserved')
    for name in ('HLSL', 'HLSL_Cg', 'materials'):
        (media / 'RTShaderLib' / name).mkdir()
    check(configure(True).returncode == 0, 'Additional installed shader directories are supported')
    check(len([path for path in paths(work / 'resources.cfg')['Graphics'] if path.is_absolute()]) == 6,
          'All installed shader directories remain available')
    check(configure(False).returncode == 0, 'Non-Windows generation does not require an existing install prefix')
    generated = paths(work / 'resources.cfg')
    check(all(str(path).startswith(str(prefix)) for path in generated['Graphics'] if path.is_absolute()),
          'Non-Windows retains its install-prefix resource layout')
    check(generated['OgreInternal'][0].resolve() == prefix / 'share/OGRE/Media/Main',
          'Non-Windows internal shadow location is retained')
    (media / 'Main').rmdir()
    failed = configure(True)
    check(failed.returncode != 0 and 'OGRE Main media is missing' in failed.stderr,
          'Missing required Windows media fails configuration instead of game startup')

if args.resources:
    config = args.resources.resolve()
    generated = paths(config)
    for entries in generated.values():
        for path in entries:
            check((path if path.is_absolute() else config.parent / path).is_dir(),
                  f'Generated resource directory exists: {path}')
    for group in ('Graphics', 'OgreInternal'):
        check(any((path / 'OgreUnifiedShader.h').is_file() for path in generated[group]),
              f'Shader header is accessible in {group}')
    check(any((path / 'ShadowVolumeExtude.program').is_file() for path in generated['OgreInternal']),
          'Internal shadow program declarations are accessible')
print(f'CHECKS={checks} FAILURES=0')
