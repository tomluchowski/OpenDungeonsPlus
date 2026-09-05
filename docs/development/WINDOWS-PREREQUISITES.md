# Windows-Voraussetzungen wiederherstellen

Installationsprotokoll vom 5. September 2026; die Voraussetzungen sind auf Marios
Rechner bereits vorhanden. Für die tägliche Arbeit genügt [BUILDING.md](BUILDING.md).
Diese Anleitung hält die tatsächlich verwendeten Quellen und Optionen fest;
die spätere Verfügbarkeit der Downloads ist damit nicht garantiert.

## Verzeichnisse und Werkzeuge

Externer Stamm: `C:\Users\mario\od-deps`, mit den Unterordnern `src`, `build`,
`install`, `tools`, `downloads` und `logs`.
Bei einer leeren Installation diese Ordner vor dem Ausführen der Skripte anlegen.
Git, VS Code, 7-Zip und die Visual-C++-Laufzeit waren bereits installiert.
Das verwendete 7-Zip ist `C:\Users\mario\scoop\shims\7z.exe`.

Die ursprünglichen Werkzeuginstallationen wurden mit diesen Befehlen durchgeführt:

```powershell
winget install --id Microsoft.VisualStudio.2022.BuildTools --exact --source winget --accept-package-agreements --accept-source-agreements --silent --disable-interactivity --override "--quiet --wait --norestart --nocache --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.Windows11SDK.26100"
winget install --id Python.Python.3.10 --exact --source winget --scope user --accept-package-agreements --accept-source-agreements --silent --disable-interactivity --override "InstallAllUsers=0 PrependPath=1 Include_test=0 Include_doc=0 Include_launcher=0 Include_debug=1 /quiet"
```

Dabei wurden Build Tools 17.14.39 / MSVC 14.44.35207 / SDK 10.0.26100.0 und
Python 3.10.11 installiert. Die Befehle selbst fixieren diese Patchversionen nicht;
bei einem späteren Neuaufbau die tatsächlich installierten Versionen abgleichen.
Python benötigt auch `libs\python310_d.lib`; eine reine Laufzeitinstallation reicht
für die dokumentierte Debug-Konfiguration nicht aus.

CMake wurde als [Version 3.31.8](https://github.com/Kitware/CMake/releases/tag/v3.31.8)
heruntergeladen und unter `od-deps\tools` entpackt, sodass
`tools\cmake-3.31.8-windows-x86_64\bin\cmake.exe` existiert.
Die dokumentierten Skripte verwenden diesen Stand; die ältere Buildlogik wurde
nicht mit einer beliebigen neueren CMake-Version geprüft.

## Git-Quellen

Alle Zielpfade sind relativ zu `C:\Users\mario\od-deps`.
Die Commits wurden an den vorhandenen lokalen Quellklonen abgeglichen.

| Quelle | Tag | Zielpfad | Commit |
| --- | --- | --- | --- |
| [OGRE](https://github.com/OGRECave/ogre) | `v13.6.5` | `src\ogre` | `856cf743ebcce8250d181a621ee47a70b12ed17e` |
| [CEGUI-Projektfork](https://github.com/tomluchowski/cegui) | `scissors_test_disabled` | `src\cegui` | `d8c7290c0eabc62e4319af4168a81757c6267403` |
| [OIS](https://github.com/wgois/OIS) | `v1.5.1` | `src\ois` | `6edb487cccb54d59e5b0fff86549d5eef475dea6` |
| [SFML](https://github.com/SFML/SFML) | `2.5.1` | `src\sfml` | `2f11710abc5aa478503a7ff3f9e654bd2078ebab` |
| [FreeType](https://github.com/freetype/freetype) | `VER-2-12-1` | `src\freetype` | `e8ebfe988b5f57bfb9a3ecb13c70d9791bce9ecf` |
| [Expat](https://github.com/libexpat/libexpat) | `R_2_8_4` | `src\expat` | `12cf0b1f25f026a022fe728ad8f7e3d017285b80` |
| [pybind11](https://github.com/pybind/pybind11) | `v2.10.4` | `src\pybind11` | `5b0a6fc2017fcc176545afe3e09c9f9885283242` |

Für einen fehlenden Quellordner das betreffende Repository mit
`git clone --depth 1 --branch <Tag> <Repository-URL>.git <Zielpfad>` klonen und
`git -C <Zielpfad> rev-parse HEAD` mit der Tabelle vergleichen.
Bestehende Quellen dabei erhalten. `scissors_test_disabled` ist ein Tag.
Expat wird aus dem Unterordner `src\expat\expat` konfiguriert.
Zusätzlich zum aufgeführten CEGUI-Basiscommit wird beim Windows-Neuaufbau der
[MSVC-Kompatibilitätspatch](../../scripts/win32/patches/cegui-msvc-snprintf.patch)
angewendet; die Ursache ist im [Buildfehlerprotokoll](WINDOWS-BUILD-FIXES.md) belegt.

## Archive und Prüfsummen

Die Downloadarchive liegen unter `od-deps\downloads`; die folgenden Hashes wurden
bei der Installation geprüft und vor der Übernahme in diese Dokumentation erneut
mit den vorhandenen Dateien abgeglichen.
Downloads erst nach erfolgreichem Hashvergleich entpacken.

| Archiv / Download | Entpacktes Ziel |
| --- | --- |
| [cmake-3.31.8-windows-x86_64.zip](https://github.com/Kitware/CMake/releases/download/v3.31.8/cmake-3.31.8-windows-x86_64.zip) | `tools\cmake-3.31.8-windows-x86_64` |
| [boost_1_82_0.zip](https://archives.boost.io/release/1.82.0/source/boost_1_82_0.zip) | `src\boost_1_82_0` |
| [pcre-8.45.zip](https://pilotfiber.dl.sourceforge.net/project/pcre/pcre/8.45/pcre-8.45.zip), lokal als `pcre-8.45-verified.zip` | `src\pcre-8.45` |

CMake, SHA-256 aus der
[Release-Prüfsummendatei](https://github.com/Kitware/CMake/releases/download/v3.31.8/cmake-3.31.8-SHA-256.txt):

```text
81aa9964dbabd71fe02e7ec50472fd3ad56138c49944515ece9001efbff8d719
```

Boost, SHA-256 aus den
[Archivmetadaten](https://archives.boost.io/release/1.82.0/source/boost_1_82_0.zip.json):

```text
f7c9e28d242abcd7a2c1b962039fcdd463ca149d1883c3a950bbcc0ce6f7c6d9
```

PCRE, SHA-512 abgeglichen mit dem
[vcpkg-Port](https://github.com/microsoft/vcpkg/blob/master/ports/pcre/portfile.cmake)
zum Installationszeitpunkt; vcpkg selbst wurde nicht benötigt oder installiert:

```text
71f246c0abbf356222933ad1604cab87a1a2a3cd8054a0b9d6deb25e0735ce9f40f923d14cbd21f32fdac7283794270afcb0f221ad24662ac35934fcb73675cd
```

Zum Prüfen und Entpacken beispielsweise:

```powershell
Get-FileHash -Algorithm SHA512 -LiteralPath 'C:\Users\mario\od-deps\downloads\pcre-8.45-verified.zip'
# Nur bei Übereinstimmung mit dem oben dokumentierten Hash:
& 'C:\Users\mario\scoop\shims\7z.exe' x 'C:\Users\mario\od-deps\downloads\pcre-8.45-verified.zip' '-oC:\Users\mario\od-deps\src'
```

Für CMake und Boost `SHA256` verwenden; CMake nach `tools`, Boost nach `src`
entpacken. Ein früherer PCRE-Download namens `pcre-8.45.zip` enthält HTML und
ist kein verwendbares Archiv; die geprüfte Datei trägt den Zusatz `-verified`.

## Bibliotheken neu bauen, falls erforderlich

Die Skripte wurden aus der erfolgreichen Einrichtung übernommen und erwarten die
oben vorbereiteten Quellen und Werkzeuge; sie laden selbst keine Quellen herunter.
Sie konfigurieren, bauen und installieren ins gemeinsame Präfix `od-deps\install`.
Release und Debug werden jeweils nacheinander gebaut.

Aus dem Projektstamm, bei einem notwendigen vollständigen Neuaufbau:

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
    if ($LASTEXITCODE -ne 0) { throw "Installation fehlgeschlagen: $taskInstallScript" }
}
```

Die separaten PowerShell-Prozesse halten die Verzeichnis- und Umgebungsänderungen
des Boost-Skripts aus der aufrufenden Sitzung heraus; Fehler beenden die Reihenfolge.

| Skript | Zuständigkeit / Reihenfolge |
| --- | --- |
| [install-base-prereqs.ps1](../../scripts/win32/install-base-prereqs.ps1) | Expat, OIS, SFML, danach FreeType, pybind11, PCRE; bei gezieltem Bedarf filterbar mit `-Only` |
| [install-boost-prereq.ps1](../../scripts/win32/install-boost-prereq.ps1) | Boost-Buildwerkzeug und Bibliotheken; erzeugt `od-deps\boost-user-config.jam` |
| [install-ogre-prereq.ps1](../../scripts/win32/install-ogre-prereq.ps1) | OGRE mit GL3Plus und den benötigten Komponenten |
| [install-cegui-prereq.ps1](../../scripts/win32/install-cegui-prereq.ps1) | Wendet den gespeicherten MSVC-Patch einmalig an und baut CEGUI mit OgreRenderer und Expat, nach OGRE und den Basisbibliotheken |

Danach den Starthelfer laden und das Spiel nach [BUILDING.md](BUILDING.md)
konfigurieren; ein Bibliotheksbuild allein belegt keinen erfolgreichen Spielbuild.
Logs liegen unter `od-deps\logs`; jeder erneute Skriptlauf ersetzt seine Logs.

## Behobene Installationsprobleme erhalten

- **SFML / FreeType:** SFML installiert eine alte mitgelieferte statische
  `freetype.lib` über die selbst gebaute Datei im gemeinsamen Präfix; das führte
  beim Linken von OgreOverlay zu `sprintf` / `LNK2019`.
  Deshalb FreeType nach SFML installieren; diese Reihenfolge steht im Basisskript.
  Die endgültige Datei wurde per Hash mit dem eigenen FreeType-Build abgeglichen.
- **Boost / Compilerinitialisierung:** Die automatische Suche verwendete einen
  falschen Pfad zu `vcvarsall.bat`. Das Skript erzeugt deshalb eine Jam-Konfiguration
  mit dem tatsächlich gefundenen `cl.exe` und
  `VC\Auxiliary\Build\vcvarsall.bat`, verwendet `--user-config`, `--reconfigure`
  sowie `architecture=x86 address-model=64` für den x64-Build.
- **MSVC-Optionen:** OGRE und CEGUI verwenden
  `/DWIN32 /D_WINDOWS /W3 /GR /EHsc /MP4`; `/EHsc` beim Anpassen der Parallelität
  erhalten. OGRE verwendet vorkompilierte Header und
  `OGRE_BUILD_MSVC_MP=OFF`, da `/MP4` bereits explizit gesetzt ist.
- **PCRE-Download:** Den HTML-Fehldownload nicht erneut verwenden; Archiv und Hash
  sind oben eindeutig festgehalten.
- **Paketierung:** Die alte Windows-Paketlogik findet einige Boost-DLL-Namen nicht,
  obwohl CMake die Linkbibliotheken findet; Paketierung und Laufzeit bleiben
  ungeprüft, siehe [aktueller Status](WINDOWS-DEV-SETUP.md).

Die ursprüngliche Einrichtung benötigte für diese Lösungen keine Quellpatches;
bei der anschließenden Spielkompilierung wurde der oben verlinkte CEGUI-Patch
erforderlich. Die konkreten CMake- und Boost-Optionen stehen vollständig in den
Skripten, die Änderung am CEGUI-Header in der zugehörigen Patchdatei.
