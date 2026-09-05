# Am Originalprojekt mitarbeiten

## Eigenständig im Fork arbeiten

Im eigenen Fork kann das Projekt unabhängig weiterentwickelt werden; eine Aufnahme
ins Team des Originalprojekts oder dessen Schreibrechte sind dafür nicht nötig.
Die tatsächlichen persönlichen Schreibrechte am Originalrepository wurden nicht
geprüft.

Das Originalprojekt wird als Upstream benötigt, wenn dessen neue Änderungen
übernommen oder eigene Änderungen per Pull Request zurückgegeben werden sollen;
beides ist für die reine Weiterentwicklung im eigenen Fork optional.

Auch im Fork einen eigenen Arbeitsbranch pro Aufgabe verwenden. Für reine
Fork-Arbeiten diesen vom aktuellen eigenen Entwicklungsstand erstellen, damit
bereits vorhandene eigene Änderungen erhalten bleiben. Die nachfolgenden Schritte
beschreiben dagegen einen Beitrag ans Originalprojekt und starten dessen
Arbeitsbranch direkt vom Upstream-Stand.

## Repositories und Zielbranch

- Eigener Fork: [Rokk001/OpenDungeonsPlus](https://github.com/Rokk001/OpenDungeonsPlus),
  lokal als `origin` eingerichtet.
- Originalprojekt für unsere Beiträge:
  [tomluchowski/OpenDungeonsPlus](https://github.com/tomluchowski/OpenDungeonsPlus),
  lokal als `upstream` eingerichtet.
- Zielbranch im Originalprojekt: `shaders-improvement` (Stand: 5. September 2026).
  Vor einem Pull Request den gewünschten Zielbranch auf GitHub prüfen.

## Aktuell eingerichtet: Windows-Unterstützung

Stand: 5. September 2026. Unser Arbeitsbranch im Fork ist
`feature/windows-support`; hier setzen wir die Windows-Arbeit fort.
Auch Dokumentation und die Vorbereitung einer Build-Umgebung gehören auf einen
Arbeitsbranch; ein Branch ist nicht auf neue Spielfunktionen beschränkt.

Der Branch wurde vom vorhandenen Fork-Stand `a8ffa583` erstellt und übernimmt
die bis dahin uncommittierten Setup-Skripte und Entwicklungsnotizen.
Der Standardbranch `shaders-improvement` bleibt auf diesem Stand;
seinen bereits vorhandenen Dokumentationscommit schreiben wir nicht um.
Beim Abruf zeigte der bestätigte Standardbranch des Originals,
`upstream/shaders-improvement`, auf den Commit `be44649f`.
Der Fork-Standardbranch liegt damit zum Einrichtungszeitpunkt einen
Dokumentationscommit vor dem Original.

Die Einrichtung wird in zwei Commits festgehalten: zuerst die bisherigen lokalen
Windows-Setup-Skripte, danach die Dokumentation einschließlich dieser Arbeitsweise.
Es gibt dabei noch keine Änderung am Spielcode oder an der Spielversion;
README und Entwicklungsdokumentation beschreiben den tatsächlichen Stand,
ein zusätzlicher Spiele-Changelog-Eintrag ist für diese Einrichtung nicht nötig.

### Tägliche Arbeit

Beim Wiederaufnehmen `git status` prüfen und auf `feature/windows-support`
weiterarbeiten; Umgebung und Buildbefehle stehen in [BUILDING.md](BUILDING.md).
Für diese laufende Windows-Aufgabe nicht bei jeder Sitzung einen neuen Branch
erstellen. Fachlich abgeschlossene Änderungen separat committen und gemeinsam
mit ihrer zugehörigen allgemeinen Build-Dokumentation prüfen; persönliche
Rechnernotizen in einem eigenen Dokumentationscommit halten.

Der Standardbranch wird für diese Arbeit nicht verändert. Neue Originaländerungen
zunächst mit `git fetch upstream` abrufen und vor einer Übernahme vergleichen;
ein Abruf allein verändert weder Arbeitsdateien noch lokale Arbeitsbranches.

`origin` ist lokal als Standardziel für spätere Pushes gesetzt, für den
Windows-Arbeitsbranch ebenfalls ausdrücklich als Push-Remote.
Der neue Branch ist bisher nur lokal vorhanden und hat noch keinen Remote-Tracking-Branch;
das Hauptprojekt ist als Quelle zum Abrufen und als späteres PR-Ziel eingerichtet.
Ein Push wird weiterhin nur nach ausdrücklicher Freigabe ausgeführt.

### Weg zum späteren Windows-PR

Der Arbeitsbranch enthält unseren Fork-Kontext einschließlich persönlicher Pfade
und Notizen; diese werden durch den Ordnernamen oder einen separaten Commit
nicht automatisch aus einem Pull Request ausgeschlossen.
Deshalb wird der fertige Beitrag später auf einem separaten PR-Branch direkt
vom dann aktuellen `upstream/shaders-improvement` zusammengestellt.
Dieser PR-Branch ist jetzt noch nicht angelegt.

Vorher den Windows-Build tatsächlich zum Laufen bringen, die Setup-Skripte für
andere Rechner nutzbar machen und die Ergebnisse der Spieltests durch den Nutzer
dokumentieren. Für den PR nur die geprüften, allgemein nutzbaren Änderungen und
ihre Anleitung übernehmen; lokale Installationsprotokolle und Agentenvorgaben
aus diesem Fork bleiben außerhalb des Beitrags.
Die Auswahl und alle betroffenen Dateiunterschiede vor dem PR ausdrücklich prüfen
und den zusammengestellten Stand erneut bauen, da er den privaten Fork-Kontext
nicht voraussetzen darf. Erst nach ausdrücklicher Push-Freigabe den PR-Branch in
den eigenen Fork veröffentlichen und gegen den Standardbranch des Originals anbieten.

## 1. Originalprojekt einmalig als Remote eintragen

Vorhandene Remotes anzeigen:

```powershell
git remote -v
```

Falls `upstream` noch fehlt:

```powershell
git remote add upstream https://github.com/tomluchowski/OpenDungeonsPlus.git
```

## 2. Basis aktuell halten

Vor einem Branchwechsel mit `git status` prüfen, dass keine ungesicherten Änderungen
vorliegen; laufende Arbeit zuerst auf ihrem eigenen Branch sichern.

```powershell
git fetch upstream
git switch shaders-improvement
git merge --ff-only upstream/shaders-improvement
```

Damit wird der lokale Basisbranch aktualisiert; auf diesem Branch keine Features
entwickeln. Falls Git den Fast-Forward ablehnt, die abweichenden Commits prüfen,
bevor weitere Schritte erfolgen.

Um auch den Basisbranch auf GitHub im eigenen Fork zu aktualisieren:

```powershell
git push origin shaders-improvement
```

## 3. Für jede Änderung einen eigenen Branch erstellen

Beispiel für eine Arbeit an der GUI-Skalierung:

```powershell
git switch -c feature/gui-scaling upstream/shaders-improvement
```

Den Namen an die konkrete Aufgabe anpassen. Der Branch startet direkt vom zuvor
abgerufenen Originalstand, damit bestehende Änderungen nur im Fork nicht automatisch
Teil des Beitrags werden.

## 4. Entwickeln, prüfen und committen

Die Änderung umsetzen und die betroffene Funktion prüfen; im Commit nur Dateien
aufnehmen, die zu dieser Aufgabe gehören. Vor jedem Commit prüfen, ob Version,
README, Änderungsprotokoll oder weitere Dokumentation angepasst werden müssen.

Mit `git diff` die Änderungen prüfen, die gewünschten Dateien gezielt mit `git add`
vormerken und anschließend mit `git diff --cached` den vollständigen Commit-Inhalt
kontrollieren.

```powershell
git commit -m "Describe the change"
```

Die Beispielnachricht durch eine konkrete Beschreibung der Änderung ersetzen.

## 5. Branch in den eigenen Fork pushen

Für den Beispielbranch:

```powershell
git push -u origin feature/gui-scaling
```

Wenn Codex die Arbeit ausführt, benötigt jeder Push eine ausdrückliche Freigabe;
die Befehle in dieser Anleitung sind selbst keine Freigabe.

## 6. Pull Request ans Originalprojekt erstellen

Auf GitHub einen Pull Request mit diesen Einstellungen öffnen:

- Zielrepository (base repository): `tomluchowski/OpenDungeonsPlus`.
- Zielbranch (base): `shaders-improvement`.
- Quellrepository (head repository): `Rokk001/OpenDungeonsPlus`.
- Quellbranch (compare): der eigene Arbeitsbranch, im Beispiel `feature/gui-scaling`.

Problem, Änderung und durchgeführte Prüfungen beschreiben; vor dem Erstellen unter
"Files changed" kontrollieren, dass ausschließlich die vorgesehenen Änderungen
enthalten sind.

## 7. Review-Kommentare bearbeiten

Korrekturen auf demselben Arbeitsbranch umsetzen, prüfen und committen; anschließend
denselben Branch erneut in den eigenen Fork pushen, wodurch sich der bestehende
Pull Request automatisch aktualisiert.

## Dokumentation im Fork und im Pull Request

Eigene Entwicklungsnotizen liegen unter `docs/development/`. Für einen Beitrag ans
Originalprojekt nur die dafür relevante Dokumentation ausdrücklich auf den
Arbeitsbranch übernehmen; der Ordnername allein schließt Dateien nicht aus einem
Pull Request aus.
