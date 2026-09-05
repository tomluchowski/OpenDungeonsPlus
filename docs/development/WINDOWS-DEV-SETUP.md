# Windows-Entwicklungsumgebung

Stand: 5. September 2026. Diese Notiz hält die besprochene Vorbereitung für
Aufgabe 3, GUI-Skalierung, fest; sie ist noch keine praktisch verifizierte
Installations- oder Build-Anleitung.

## Reicht VS Code allein?

Nein. VS Code dient als Editor; um die Änderungen im Spiel zu prüfen, müssen
zusätzlich Compiler, Build-Werkzeuge und Projektabhängigkeiten verfügbar sein,
sodass das Spiel lokal kompiliert und startet.

## Im Gespräch vorgeschlagene Werkzeuge

- VS Code als Editor.
- Git für die Versionsverwaltung.
- CMake für die Build-Konfiguration.
- Visual Studio 2022 Build Tools mit dem Workload "Desktop development with C++",
  MSVC-Compiler und Windows SDK.
- Der lokale Klon des eigenen OpenDungeonsPlus-Forks.

Visual Studio 2022 wurde im Gespräch vorgeschlagen; eine funktionierende
Kombination mit diesem Projekt und seinen Abhängigkeiten ist noch nicht bestätigt.
Die vorhandene [Windows-Build-Konfiguration](../../appveyor.yml) enthält stattdessen
ältere Einträge für Visual Studio 12 und MinGW, die keinen erfolgreichen Build mit
Visual Studio 2022 belegen.

## Im aktuellen Build-Code bestätigte Abhängigkeiten

Die [Build-Datei](../../CMakeLists.txt) sucht folgende Bibliotheken:

- OGRE, einschließlich der vom Spiel verknüpften Komponenten.
- CEGUI einschließlich des OGRE-Renderers.
- SFML.
- OIS.
- Boost.
- Python-Entwicklungsbibliotheken und Header.
- pybind11 für die Python-Einbettung.

Die Build-Datei nennt CMake 3.5 als Mindestversion, prüft OGRE ab 1.9 und CEGUI ab
0.8 und sucht SFML 2. Diese Angaben beschreiben die vorhandenen Build-Prüfungen;
sie belegen keine konkret funktionierende Kombination aktueller Paketversionen
und keinen stabilen Ogre-14-Port.

## Im Gespräch genannte optionale Ergänzungen

- CMake Tools für VS Code.
- C/C++-Erweiterung für VS Code.
- Codex in VS Code.

## Noch zu prüfen

- Welche Werkzeuge und Abhängigkeiten auf dem Rechner bereits installiert sind.
- Welche konkreten Versionen, Compiler und Zielarchitektur zusammen funktionieren.
- Wie die Abhängigkeiten für diese Kombination bereitgestellt und gefunden werden.
- Ob das Spiel damit tatsächlich gebaut und gestartet werden kann.

Die Bibliotheksliste wurde statisch mit dem Repository abgeglichen; im Rahmen
dieser Dokumentation wurden keine Programme installiert und keine Builds oder
Spieltests gestartet.

Sobald ein funktionierender Ablauf feststeht, die tatsächlich verwendeten Versionen
und Build-Schritte in einer eigenen Build-Anleitung festhalten und hier verlinken.
