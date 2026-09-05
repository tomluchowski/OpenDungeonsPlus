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
  lokal als `upstream` einzurichten.
- Zielbranch im Originalprojekt: `shaders-improvement` (Stand: 5. September 2026).
  Vor einem Pull Request den gewünschten Zielbranch auf GitHub prüfen.

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
