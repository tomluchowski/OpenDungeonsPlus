# Restoring Windows prerequisites

Installation log from September 5, 2026; the prerequisites are already present on
Mario's computer. For daily work, [BUILDING.md](BUILDING.md) is sufficient.
This guide records the sources and options actually used;
it does not guarantee that the downloads will remain available later.

## Directories and tools

External root: `C:\Users\mario\od-deps`, with the subdirectories `src`, `build`,
`install`, `tools`, `downloads` and `logs`.
For an empty installation, create these directories before running the scripts.
Git, VS Code, 7-Zip and the Visual C++ runtime were already installed.
The 7-Zip used is `C:\Users\mario\scoop\shims\7z.exe`.

The original tool installations were performed with these commands:

```powershell
winget install --id Microsoft.VisualStudio.2022.BuildTools --exact --source winget --accept-package-agreements --accept-source-agreements --silent --disable-interactivity --override "--quiet --wait --norestart --nocache --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.Windows11SDK.26100"
winget install --id Python.Python.3.10 --exact --source winget --scope user --accept-package-agreements --accept-source-agreements --silent --disable-interactivity --override "InstallAllUsers=0 PrependPath=1 Include_test=0 Include_doc=0 Include_launcher=0 Include_debug=1 /quiet"
```

This installed Build Tools 17.14.39 / MSVC 14.44.35207 / SDK 10.0.26100.0 and
Python 3.10.11. The commands themselves do not pin these patch versions;
compare the versions actually installed when rebuilding at a later date.
Python also requires `libs\python310_d.lib`; a runtime-only installation is not
sufficient for the documented Debug configuration.

CMake was downloaded as [version 3.31.8](https://github.com/Kitware/CMake/releases/tag/v3.31.8)
and extracted under `od-deps\tools`, so that
`tools\cmake-3.31.8-windows-x86_64\bin\cmake.exe` exists.
The documented scripts use this version; the older build logic has not been
tested with an arbitrary newer CMake version.

## Git sources

All target paths are relative to `C:\Users\mario\od-deps`.
The commits were checked against the existing local source clones.

| Source | Tag | Target path | Commit |
| --- | --- | --- | --- |
| [OGRE](https://github.com/OGRECave/ogre) | `v13.6.5` | `src\ogre` | `856cf743ebcce8250d181a621ee47a70b12ed17e` |
| [CEGUI project fork](https://github.com/tomluchowski/cegui) | `scissors_test_disabled` | `src\cegui` | `d8c7290c0eabc62e4319af4168a81757c6267403` |
| [OIS](https://github.com/wgois/OIS) | `v1.5.1` | `src\ois` | `6edb487cccb54d59e5b0fff86549d5eef475dea6` |
| [SFML](https://github.com/SFML/SFML) | `2.5.1` | `src\sfml` | `2f11710abc5aa478503a7ff3f9e654bd2078ebab` |
| [FreeType](https://github.com/freetype/freetype) | `VER-2-12-1` | `src\freetype` | `e8ebfe988b5f57bfb9a3ecb13c70d9791bce9ecf` |
| [Expat](https://github.com/libexpat/libexpat) | `R_2_8_4` | `src\expat` | `12cf0b1f25f026a022fe728ad8f7e3d017285b80` |
| [pybind11](https://github.com/pybind/pybind11) | `v2.10.4` | `src\pybind11` | `5b0a6fc2017fcc176545afe3e09c9f9885283242` |

For a missing source directory, clone the relevant repository with
`git clone --depth 1 --branch <Tag> <Repository-URL>.git <TargetPath>` and
compare `git -C <TargetPath> rev-parse HEAD` with the table.
Preserve existing sources when doing so. `scissors_test_disabled` is a tag.
Expat is configured from the subdirectory `src\expat\expat`.
In addition to the listed CEGUI base commit, the
[MSVC compatibility patch](../../scripts/win32/patches/cegui-msvc-snprintf.patch)
is applied when rebuilding on Windows; the cause is documented in the [build error log](WINDOWS-BUILD-FIXES.md).

## Archives and checksums

The download archives are stored under `od-deps\downloads`; the following hashes were
verified during installation and checked against the existing files again before
being added to this documentation.
Extract downloads only after a successful hash comparison.

| Archive / download | Extraction target |
| --- | --- |
| [cmake-3.31.8-windows-x86_64.zip](https://github.com/Kitware/CMake/releases/download/v3.31.8/cmake-3.31.8-windows-x86_64.zip) | `tools\cmake-3.31.8-windows-x86_64` |
| [boost_1_82_0.zip](https://archives.boost.io/release/1.82.0/source/boost_1_82_0.zip) | `src\boost_1_82_0` |
| [pcre-8.45.zip](https://pilotfiber.dl.sourceforge.net/project/pcre/pcre/8.45/pcre-8.45.zip), locally named `pcre-8.45-verified.zip` | `src\pcre-8.45` |

CMake, SHA-256 from the
[release checksum file](https://github.com/Kitware/CMake/releases/download/v3.31.8/cmake-3.31.8-SHA-256.txt):

```text
81aa9964dbabd71fe02e7ec50472fd3ad56138c49944515ece9001efbff8d719
```

Boost, SHA-256 from the
[archive metadata](https://archives.boost.io/release/1.82.0/source/boost_1_82_0.zip.json):

```text
f7c9e28d242abcd7a2c1b962039fcdd463ca149d1883c3a950bbcc0ce6f7c6d9
```

PCRE, SHA-512 checked against the
[vcpkg port](https://github.com/microsoft/vcpkg/blob/master/ports/pcre/portfile.cmake)
at the time of installation; vcpkg itself was neither required nor installed:

```text
71f246c0abbf356222933ad1604cab87a1a2a3cd8054a0b9d6deb25e0735ce9f40f923d14cbd21f32fdac7283794270afcb0f221ad24662ac35934fcb73675cd
```

For example, to verify and extract:

```powershell
Get-FileHash -Algorithm SHA512 -LiteralPath 'C:\Users\mario\od-deps\downloads\pcre-8.45-verified.zip'
# Only if it matches the hash documented above:
& 'C:\Users\mario\scoop\shims\7z.exe' x 'C:\Users\mario\od-deps\downloads\pcre-8.45-verified.zip' '-oC:\Users\mario\od-deps\src'
```

Use `SHA256` for CMake and Boost; extract CMake to `tools` and Boost to `src`.
An earlier PCRE download named `pcre-8.45.zip` contains HTML and
is not a usable archive; the verified file has the suffix `-verified`.

## Rebuild libraries if necessary

The scripts were taken from the successful setup and expect the
sources and tools prepared above; they do not download sources themselves.
They configure, build and install into the shared prefix `od-deps\install`.
Release and Debug are built sequentially for each library.

From the project root, when a complete rebuild is necessary:

```powershell
Set-Location -LiteralPath 'C:\Users\mario\GitHub\OpenDungeonsPlus'
$taskInstallScripts = @(
    'install-base-prereqs.ps1'
    'install-boost-prereq.ps1'
    'install-ogre-prereq.ps1'
    'install-cegui-prereq.ps1'
)
foreach ($taskInstallScript in $taskInstallScripts) {
    $taskScriptPath = Join-Path $PWD.Path "scripts\win32\$taskInstallScript"
    & powershell.exe -NoProfile -File $taskScriptPath
    if ($LASTEXITCODE -ne 0) { throw "Installation failed: $taskInstallScript" }
}
```

The separate PowerShell processes keep the Boost script's directory and environment
changes out of the calling session; errors stop the sequence.

| Script | Responsibility / order |
| --- | --- |
| [install-base-prereqs.ps1](../../scripts/win32/install-base-prereqs.ps1) | Expat, OIS, SFML, then FreeType, pybind11, PCRE; can be filtered with `-Only` for targeted needs |
| [install-boost-prereq.ps1](../../scripts/win32/install-boost-prereq.ps1) | Boost build tool and libraries; generates `od-deps\boost-user-config.jam` |
| [install-ogre-prereq.ps1](../../scripts/win32/install-ogre-prereq.ps1) | Applies the saved multi-window settings patch once, then builds OGRE with GL3Plus and the required components |
| [install-cegui-prereq.ps1](../../scripts/win32/install-cegui-prereq.ps1) | Applies the saved MSVC patch once and builds CEGUI with OgreRenderer and Expat, after OGRE and the base libraries |

Then load the environment helper and configure the game following [BUILDING.md](BUILDING.md);
a library build alone does not establish a successful game build.
Logs are stored under `od-deps\logs`; each new script run replaces its logs.

## Preserve fixes for installation problems

- **SFML / FreeType:** SFML installs an old bundled static
  `freetype.lib` over the locally built file in the shared prefix; this caused
  `sprintf` / `LNK2019` when linking OgreOverlay.
  Therefore, install FreeType after SFML; this order is recorded in the base script.
  The final file was compared with the local FreeType build by hash.
- **Boost / compiler initialization:** The automatic search used an
  incorrect path to `vcvarsall.bat`. The script therefore generates a Jam configuration
  with the actual detected `cl.exe` and
  `VC\Auxiliary\Build\vcvarsall.bat`, uses `--user-config`, `--reconfigure`
  and `architecture=x86 address-model=64` for the x64 build.
- **MSVC options:** OGRE and CEGUI use
  `/DWIN32 /D_WINDOWS /W3 /GR /EHsc /MP4`; preserve `/EHsc` when adjusting
  parallelism. OGRE uses precompiled headers and
  `OGRE_BUILD_MSVC_MP=OFF`, since `/MP4` is already set explicitly.
- **PCRE download:** Do not reuse the failed HTML download; the archive and hash
  are clearly recorded above.
- **Packaging:** The old Windows packaging logic cannot find some Boost DLL names,
  although CMake finds the link libraries; packaging and runtime remain
  unverified, see the [current status](WINDOWS-DEV-SETUP.md).

The original setup did not require source patches. The CEGUI patch linked above
became necessary during the subsequent game compilation. The live-settings work
also requires
[the OGRE multi-window settings patch](../../scripts/win32/patches/ogre-multiwindow-settings.patch)
for context-safe secondary windows, hardware-gamma detection and runtime GL3Plus
options. Both installers apply their patch idempotently. The specific CMake and
Boost options are fully recorded in the scripts.
