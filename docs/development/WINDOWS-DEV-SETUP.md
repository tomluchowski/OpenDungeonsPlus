# Windows-Entwicklungsumgebung

Stand: 5. September 2026, lokaler Rechner von Mario, Windows x64.
Die Voraussetzungen sind installiert; ihre Bibliotheksbuilds und die
CMake-Konfiguration des Spiels waren erfolgreich.
Die Spielbuilds für Release und Debug sind nach vier gezielten Korrekturen
erfolgreich; Spielstart und Paketierung bleiben ungeprüft.

## Einstieg und Zuständigkeit der Dateien

Diese Dokumentation und die Skripte unter [scripts/win32](../../scripts/win32)
sind der gepflegte Projektbezug zur externen Installation.
Für die tägliche Arbeit [BUILDING.md](BUILDING.md) verwenden;
Quellen und Neuaufbau stehen in [WINDOWS-PREREQUISITES.md](WINDOWS-PREREQUISITES.md).
[AGENTS.md](../../AGENTS.md) verweist neue Agentensitzungen auf diese Dateien.

Die großen Downloads, Bibliotheksquellen und kompilierten Dateien liegen außerhalb
des Git-Repositories, damit sie unabhängig vom Arbeitsbranch wiederverwendbar sind.
Die Skripte enthalten bewusst die konkreten Pfade dieser lokalen Installation;
auf einem anderen Rechner müssen diese Pfade geprüft und angepasst werden.
VS Code ist der Editor; Compiler, SDK und Bibliotheken werden zusätzlich benötigt.

## Speicherorte

| Zweck | Tatsächlicher Pfad |
| --- | --- |
| Projekt | `C:\Users\mario\GitHub\OpenDungeonsPlus` |
| Externer Entwicklungsordner | `C:\Users\mario\od-deps` |
| Installationspräfix der Bibliotheken | `C:\Users\mario\od-deps\install` |
| Header / Linkbibliotheken / DLLs | Unter `install\include`, `install\lib`, `install\bin`; einige DLLs liegen auch unter `install\lib` |
| Bibliotheksquellen | `C:\Users\mario\od-deps\src` |
| Bibliotheksbuilds | `C:\Users\mario\od-deps\build` |
| Archive und Prüfsummen | `C:\Users\mario\od-deps\downloads` |
| Konfigurations- und Bibliotheksprotokolle | `C:\Users\mario\od-deps\logs` |
| CMake | `C:\Users\mario\od-deps\tools\cmake-3.31.8-windows-x86_64\bin\cmake.exe` |
| Visual Studio Build Tools | `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools` |
| Python | `C:\Users\mario\AppData\Local\Programs\Python\Python310` |
| 7-Zip für die Quellarchive | `C:\Users\mario\scoop\shims\7z.exe` |
| Generierter Spielbuild | `C:\Users\mario\GitHub\OpenDungeonsPlus\build\windows` |
| Vorgesehener Installationsordner des Spiels | `C:\Users\mario\GitHub\OpenDungeonsPlus\build\windows\install` (noch nicht installiert) |

Unter `od-deps\setup-scripts`, `od-deps\Enter-OpenDungeonsPlus.ps1`,
`od-deps\INSTALLATION.md` und dem ignorierten Projektordner `build` liegen noch
Kopien aus der Installation; für weitere Arbeit die gepflegten Projektdateien
verwenden, da die alten Kopien nicht automatisch aktualisiert werden.
Ein neuer Klon enthält die Anleitungen und Skripte, aber nicht die externe Installation.

## Installierter Stand

| Komponente | Version / Konfiguration |
| --- | --- |
| Visual Studio 2022 Build Tools | 17.14.39, MSVC-Toolsetordner 14.44.35207, von CMake erkannter Compiler 19.44.35228.0, Ziel und Host x64 |
| Windows SDK | 10.0.26100.0 |
| CMake | 3.31.8, portable Installation |
| Python | 3.10.11 mit Headern, Release- und Debug-Importbibliotheken und Debug-Binärdateien |
| OGRE | 13.6.5, GL3Plus; OgreMain, Bites, Overlay, RTShaderSystem, Octree, ParticleFX, STBI |
| CEGUI | 0.9999.0 aus `tomluchowski/cegui`, Tag `scissors_test_disabled`, mit OgreRenderer, Expat und dem gespeicherten MSVC-Kompatibilitätspatch |
| OIS | 1.5.1 |
| SFML | 2.5.1 |
| Boost | 1.82.0; filesystem, locale, program_options, thread, system, chrono, date_time, atomic |
| pybind11 | 2.10.4 |
| FreeType | 2.12.1 |
| Expat | 2.8.4 |
| PCRE | 8.45 mit UTF und Unicode-Properties |

Die kompilierten Bibliotheken liegen für x64 in Release und Debug vor;
Boost zusätzlich statisch und dynamisch, jeweils mit dynamischer C++-Laufzeit.
pybind11 stellt Header und CMake-Konfiguration bereit.
Git, VS Code, 7-Zip und eine passende Visual-C++-Laufzeit waren bereits vorhanden.

OGRE, CEGUI, OIS, SFML und Python 3.10 orientieren sich an der vorhandenen
[Snap-Baurezeptur](../../snap/snapcraft.yaml); die konkreten Windows-Optionen stehen
in den Skripten. Dies dokumentiert OGRE 13.6.5 und belegt keinen Ogre-14-Port.
Die älteren allgemeinen Angaben in README und AppVeyor ersetzen diesen lokalen Stand nicht.

## Wie Projekt und externe Installation zusammenfinden

[Enter-OpenDungeonsPlus.ps1](../../scripts/win32/Enter-OpenDungeonsPlus.ps1)
lädt die Visual-Studio-Entwicklungsumgebung für x64 und setzt in der aktuellen
PowerShell-Sitzung:

- `PATH` für CMake, Bibliotheks-DLLs und Python.
- `CMAKE_PREFIX_PATH`, `CEGUI_HOME` und `OIS_HOME` auf `od-deps\install`.
- `BOOST_ROOT`, `BOOST_INCLUDEDIR`, `BOOST_LIBRARYDIR` auf die installierten Boost-Dateien.
- `LIB` zusätzlich auf `od-deps\install\lib` für MSVCs automatische Verknüpfung.

CMake wurde bei der Installation außerdem dem Benutzer-PATH hinzugefügt;
für einen eindeutigen Build trotzdem den Starthelfer laden.
Die Compilerinitialisierung verwendet unter den Build Tools
`Common7\Tools\Microsoft.VisualStudio.DevShell.dll`;
Boost verwendet zusätzlich `VC\Auxiliary\Build\vcvarsall.bat`.
Der Compiler liegt dort unter
`VC\Tools\MSVC\14.44.35207\bin\HostX64\x64\cl.exe`.

[configure-windows-prereqs.ps1](../../scripts/win32/configure-windows-prereqs.ps1)
ermittelt den Projektstamm relativ zu seinem eigenen Speicherort, lädt den
benachbarten Starthelfer und konfiguriert `build\windows` mit Visual Studio 2022/x64.
Es übergibt Python-Executable, Header und beide Importbibliotheken explizit
und deaktiviert die Testtargets. Bei der ursprünglichen Einrichtung wurden die
Spielquellen und `CMakeLists.txt` nicht geändert; die anschließenden Korrekturen
aus der tatsächlichen Kompilierung stehen im [Buildfehlerprotokoll](WINDOWS-BUILD-FIXES.md).

## Überprüft und noch offen

| Schritt | Stand / Nachweis |
| --- | --- |
| Compiler, CMake und Python laden | Über den Starthelfer erfolgreich geprüft |
| Bibliotheken bauen und installieren | Release und Debug erfolgreich; CEGUI anschließend mit dem gespeicherten MSVC-Patch neu gebaut und installiert; Protokolle unter `od-deps\logs` |
| Spiel konfigurieren | Mit den Buildkorrekturen erfolgreich erneut konfiguriert; `opendungeons-configure.log` endet mit erfolgreicher Konfiguration und Generierung |
| Konfiguration über die ins Projekt übernommenen Skripte | Erfolgreich erneut ausgeführt, auch aus `docs\development`; Projektpfad und benachbarter Starthelfer werden korrekt aufgelöst |
| Spiel kompilieren | Release und Debug erfolgreich mit Exitcode 0; Befehle in [BUILDING.md](BUILDING.md), Nachweise im [Buildfehlerprotokoll](WINDOWS-BUILD-FIXES.md) |
| Erzeugte Programme prüfen | Beide Dateien tragen die AMD64-PE-Kennung; Release importiert nur `python310.dll`, Debug nur `python310_d.dll` als Python-Laufzeit |
| Spiel starten, GUI und Cursor testen | Noch nicht geprüft; manuelle QA durch den Nutzer |
| Installationspaket erstellen | Noch nicht geprüft; ältere Paketlogik findet einige erwartete Boost-DLL-Namen nicht |

CMake hat Python, pybind11, OIS, OGRE, CEGUI samt OgreRenderer, SFML und die
benötigten Boost-Linkbibliotheken gefunden. Die Meldung
`DEPS_BOOST_FILESYSTEM_BIN_REL-NOTFOUND` betrifft die separate DLL-Suche für die
Paketierung; daraus weder einen fehlgeschlagenen Linktest noch ein funktionierendes
Installationspaket ableiten.
Die erneute Konfiguration meldet außerdem `Boost toolset is unknown` für
MSVC 19.44.35228.0; die Generierung und beide Linkschritte gelingen trotzdem,
nachdem der im [Buildfehlerprotokoll](WINDOWS-BUILD-FIXES.md) beschriebene
Boost-Suchpfad korrigiert wurde.

Die Buildfehler sind behoben; vorhandene Compiler-/Linkerwarnungen sind im
Buildfehlerprotokoll festgehalten. Als Nächstes steht die Laufzeitprüfung durch
den Nutzer an; erfolgreiche Builds ersetzen diese Prüfung nicht.
