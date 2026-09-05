# Unter Windows konfigurieren und kompilieren

Stand: 5. September 2026. Die [Voraussetzungen](WINDOWS-DEV-SETUP.md) sind installiert
und CMake sowie die Spielbuilds für Windows x64 in Release und Debug wurden
erfolgreich ausgeführt; der Spielstart ist noch nicht praktisch geprüft.
Die vier behobenen Buildfehler und ihre Nachweise stehen in
[WINDOWS-BUILD-FIXES.md](WINDOWS-BUILD-FIXES.md).

## 1. PowerShell-Sitzung vorbereiten

In einer neuen PowerShell-Konsole:

```powershell
Set-Location -LiteralPath 'C:\Users\mario\GitHub\OpenDungeonsPlus'
. .\scripts\win32\Enter-OpenDungeonsPlus.ps1
```

Der Punkt am Anfang lädt Compiler und Suchpfade in dieselbe Sitzung; in dieser
Konsole anschließend konfigurieren und bauen. Erwartet sind CMake 3.31.8,
Python 3.10.11 und der x64-Compiler der Visual Studio 2022 Build Tools.
Bei Bedarf ohne Build prüfen:

```powershell
Get-Command cl.exe, cmake.exe, python.exe | Select-Object Name, Source
cmake --version
python --version
```

## 2. CMake konfigurieren

```powershell
& .\scripts\win32\configure-windows-prereqs.ps1
```

Das Skript lädt auch selbst den Starthelfer und verwendet:

- Quelle: Projektstamm, aus dem Skriptpfad abgeleitet.
- Buildordner: `build\windows`.
- Generator: `Visual Studio 17 2022`, Architektur `x64`.
- `OD_BUILD_TESTING=OFF` und `BUILD_TESTING=OFF`.
- Installationsziel: `build\windows\install`.
- Python unter `C:\Users\mario\AppData\Local\Programs\Python\Python310`:
  `python.exe`, `include`, `libs\python310.lib`, `libs\python310_d.lib`.

Das Konfigurationsprotokoll wird bei jedem Aufruf ersetzt:
`C:\Users\mario\od-deps\logs\opendungeons-configure.log`.
Bei Fehlern gibt das Skript die letzten Zeilen aus und bricht ab.
Nach Änderungen an CMake-Dateien oder Quelllisten erneut konfigurieren;
für gewöhnliche Änderungen bestehender C++-Dateien den vorhandenen Build verwenden.

## 3. Spiel bauen

Release aus derselben vorbereiteten Konsole:

```powershell
cmake --build .\build\windows --config Release --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Spielbuild Release fehlgeschlagen' }
```

Für Debug stattdessen:

```powershell
cmake --build .\build\windows --config Debug --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Spielbuild Debug fehlgeschlagen' }
```

Die erfolgreich erzeugten Ausgabedateien sind
`build\windows\opendungeons-plus.exe` und `build\windows\opendungeons-plus_d.exe`,
direkt im Buildordner. Beide Dateien wurden auf ihre AMD64-PE-Kennung und die
jeweils passende Python-DLL geprüft, ohne das Spiel zu starten.
Die Buildausgabe erscheint in der Konsole; bei einem Fehler die erste konkrete
Compiler-/Linkermeldung und die verwendete Konfiguration festhalten.
Die protokollierten erfolgreichen Verifikationsläufe dieser Einrichtung liegen
unter `build\windows\game-Release-pass3.log` und `game-Debug-pass3.log`.

## 4. Spielstart und manuelle Prüfung

Erst nach erfolgreichem Build durch den Nutzer prüfen; der folgende Startbefehl
ist noch nicht praktisch verifiziert und setzt dieselbe geladene Umgebung voraus:

```powershell
Push-Location -LiteralPath .\build\windows
try {
    & .\opendungeons-plus.exe
} finally {
    Pop-Location
}
```

Für Debug die Datei `opendungeons-plus_d.exe` verwenden.
CMake erzeugt hier `plugins.cfg`, `plugins_d.cfg`, `resources.cfg` und Verbindungen
zu den Spieldaten. Bei fehlenden DLLs oder Plugins den tatsächlichen Startfehler
prüfen; DLL-Verteilung, Plugin-Laden und Spielstart sind noch offen.
Manuelle Spieltests und die visuelle Abnahme führt der Nutzer durch.

## Protokolle und Wiederaufnahme

- Aktueller Installations- und Prüfstand: [WINDOWS-DEV-SETUP.md](WINDOWS-DEV-SETUP.md).
- Bibliotheksprotokolle: `C:\Users\mario\od-deps\logs\<name>-configure.log`,
  `<name>-Release.log`, `<name>-Debug.log`; Boost verwendet
  `boost-bootstrap.log` und `boost-build.log`.
- Abhängigkeiten nur bei tatsächlichem Bedarf nach
  [WINDOWS-PREREQUISITES.md](WINDOWS-PREREQUISITES.md) neu bauen.

`build` ist von Git ausgeschlossen und enthält generierte Dateien;
`build\windows` enthält außerdem Verzeichnisverbindungen zu Projektressourcen.
Diese Verbindungen beim Aufräumen beachten und keine Quellverzeichnisse über sie löschen.
Nach einem neuen Ergebnis Datum, verwendete Konfiguration, Fehler bzw. Erfolg
und verbleibende Prüfungen im Windows-Status nachtragen.
