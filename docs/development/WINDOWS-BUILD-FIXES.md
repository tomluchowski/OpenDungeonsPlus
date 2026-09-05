# Windows-Build: Fehler und Korrekturen

Stand: 5. September 2026. Die Fehler wurden beim tatsächlichen Build mit
Visual Studio 2022, MSVC 19.44.35228.0, Windows SDK 10.0.26100.0 und x64
untersucht. Die verwendeten Bibliotheken und die lokale Umgebung sind in
[WINDOWS-DEV-SETUP.md](WINDOWS-DEV-SETUP.md) dokumentiert.

## Fenster-Icon: ungeeignete 32-Bit-Schnittstelle

Der erste Release-Build scheiterte in [ODApplication.cpp](../../source/ODApplication.cpp)
beim Setzen des Fenster-Icons:

```text
error C2065: GCL_HICON: nichtdeklarierter Bezeichner
warning C4311 / C4302: Zeigerverkürzung von HICON zu LONG
```

Der betroffene Windows-Code verwendete `SetClassLong`, `GCL_HICON` und eine
Umwandlung des Icon-Handles in `LONG`. Der installierte Windows-SDK-Header
`um/WinUser.h` entfernt `GCL_HICON` ausdrücklich bei gesetztem `_WIN64`;
außerdem ist `LONG` nicht breit genug für einen 64-Bit-Zeiger.

Die Korrektur verwendet an derselben Stelle `SetClassLongPtr`, `GCLP_HICON`
und `LONG_PTR`. Das bestehende Icon und der Zeitpunkt seiner Zuweisung bleiben
erhalten; die Änderung liegt weiterhin ausschließlich im Windows-Codepfad.

Nachweis des Ausgangsfehlers: `build/windows/game-Release-initial.log`.
Die ursprüngliche Buildausgabe bleibt zur Nachvollziehbarkeit lokal erhalten.

## CEGUI-Makro verändert Boost-Header

Derselbe Build meldete `C2039: _snprintf ist kein Member von std`, unter anderem
beim Übersetzen von `ODClient.cpp`, `SettingsWindow.cpp` und `ModeManager.cpp`.
Die Fehlermeldung zeigt auf `boost/assert/source_location.hpp`;
dort verwendet Boost auf aktuellen MSVC-Versionen korrekt `std::snprintf`.

Die Ursache liegt im zuvor eingebundenen `CEGUI/PropertyHelper.h` des verwendeten
CEGUI-Tags: Ein für alle MSVC-Versionen gesetztes Makro ersetzt `snprintf` durch
`_snprintf` und verändert damit auch den späteren Boost-Code zu `std::_snprintf`.
Das Projekt selbst definiert dieses Makro nicht.

Der [CEGUI-Patch](../../scripts/win32/patches/cegui-msvc-snprintf.patch) begrenzt
diesen Ersatz auf `_MSC_VER < 1900`; für neuere Compiler bleibt die vorhandene
Standardfunktion verwendbar. Das
[Installationsskript](../../scripts/win32/install-cegui-prereq.ps1) wendet den
Patch vor dem CEGUI-Build an und erkennt einen bereits angewendeten Patch.
CEGUI wurde anschließend in Release und Debug erfolgreich neu gebaut und
installiert; der installierte Header ist per SHA-256 mit der gepatchten Quelle
abgeglichen. Die Erkennung des bereits angewendeten Patches wurde mit
`git apply --reverse --check` erfolgreich geprüft.
Protokolle: `build/windows/cegui-rebuild-driver.log` im Projekt und
`cegui-Release.log` / `cegui-Debug.log` im externen Bibliotheks-Logordner.

## Boost-Linkerpfad zeigt in den Headerordner

Nach den beiden Compilerkorrekturen erreichte Release den Linker und scheiterte
mit `LNK1104` für `libboost_filesystem-vc143-mt-x64-1_82.lib`.
Die passende Datei war installiert und im CMake-Cache bereits korrekt gefunden.
Das generierte Visual-Studio-Projekt enthielt jedoch als zusätzlichen Suchpfad
`include/boost-1_82/stage/lib`, wo diese Bibliothek nicht liegt.

Ursache war in [CMakeLists.txt](../../CMakeLists.txt) die Ableitung des
MSVC-Linkerpfads aus `Boost_INCLUDE_DIRS`. Stattdessen wird jetzt das von
der Paketsuche ermittelte `Boost_LIBRARY_DIRS` verwendet; so benötigt die
Korrektur keinen rechnerabhängigen Pfad und erhält die bisherige automatische
Boost-Verknüpfung unter MSVC.
Nachweis des Linkerfehlers: `build/windows/game-Release-pass1.log`.

## Python-Debug-Bibliothek und Headerauswahl widersprechen sich

Der vollständige Debug-Compilerlauf war erfolgreich, der Linker brach jedoch
mit `LNK1104` für `python310.lib` ab. CMake hatte bereits die vorhandene
`python310_d.lib` als explizite Debug-Abhängigkeit eingetragen.

`pybind11/detail/common.h` setzt beim Einbinden von Python unter MSVC das Makro
`_DEBUG` vorübergehend außer Kraft, solange `Py_DEBUG` nicht definiert ist.
Dadurch erzeugt `Python/include/pyconfig.h` eine zusätzliche automatische
Linkanforderung für `python310.lib`, obwohl CMake die Debug-Bibliothek auswählt.

Wenn eine Python-Debug-Bibliothek konfiguriert ist und pybind11 den Debug-Modus
nicht bereits aktiviert hat, definiert das Spieltarget unter MSVC deshalb
`Py_DEBUG` ausschließlich für die Debug-Konfiguration.
Der leere Makrowert entspricht der Definition im Windows-Python-Header;
Release und Konfigurationen ohne gefundene Debug-Bibliothek bleiben unverändert.
Damit werden Header und Bibliotheksvariante konsistent ausgewählt, statt die
gleichzeitige Verwendung beider Varianten durch einen zusätzlichen Suchpfad zu ermöglichen.
Nachweis: `build/windows/game-Debug-pass2.log`.

## Verifikation

Nach erneutem Konfigurieren enthält das generierte Visual-Studio-Projekt für
Release und Debug den tatsächlichen Boost-Library-Ordner.

| Prüfung | Ergebnis / Nachweis |
| --- | --- |
| CEGUI mit Patch, Release und Debug | Erfolgreich gebaut und installiert; Treiberprotokoll `build/windows/cegui-rebuild-driver.log` |
| Spiel Release nach allen vier Korrekturen | Exitcode 0, `build/windows/game-Release-pass3.log`, Ausgabe `build/windows/opendungeons-plus.exe` |
| Spiel Debug nach allen vier Korrekturen | Exitcode 0, `build/windows/game-Debug-pass3.log`, Ausgabe `build/windows/opendungeons-plus_d.exe` |
| Binärdateien | PE-Kennung und AMD64-Maschinenkennung beider erzeugter Programme geprüft |
| Python-Varianten | Statische Importprüfung: Release verwendet ausschließlich `python310.dll`, Debug ausschließlich `python310_d.dll` als Python-Laufzeit; `build/windows/game-Release-dependencies.log` und `game-Debug-dependencies.log` |

Beide abschließenden Buildprotokolle enthalten keine Compiler- oder Linkerfehler.
Die vorherigen Fehlversuche bleiben bewusst als getrennte Protokolle erhalten.
Die Python-DLL-Prüfung erfolgte mit `dumpbin /dependents` auf beiden Programmen;
es wurde keine Spielinstanz gestartet.

Die Spielbuilds werden über `cmake --build build/windows --config Release --parallel 4`
bzw. dieselbe Anweisung mit `--config Debug` ausgeführt, nach dem Konfigurieren
mit dem [Windows-Konfigurationsskript](../../scripts/win32/configure-windows-prereqs.ps1).
Spielstart, manuelle QA und die sichtbare Darstellung des Icons sind davon
getrennte Prüfungen durch den Nutzer.

Die Builds sind nicht warnungsfrei: Unter anderem
bleiben Warnungen zu numerischen Konvertierungen, die veraltete Option `/Gm` und
im Release-Linkschritt `LNK4075` zur Kombination von Edit-and-Continue mit
Linkeroptimierung sowie im Debug-Linkschritt zur Kombination von `/INCREMENTAL`
und `/FORCE` bestehen. Diese Arbeit behebt die nachgewiesenen Buildabbrüche;
sie schaltet keine Warnungen ab.

## Version und Umfang

Die Spielversion bleibt 0.7.1; diese Arbeit behebt Buildfehler und veröffentlicht
keine neue Spielversion. Die technische Änderung wird zusammen mit diesem
Fehlerprotokoll committed; lokale Status- und Einstiegshinweise werden in einem
separaten Dokumentationscommit aktualisiert.
