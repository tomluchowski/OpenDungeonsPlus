# Entwicklungsdokumentation

Hier sammeln wir Anleitungen und Erkenntnisse zur Mitarbeit an OpenDungeonsPlus,
mit einer Markdown-Datei pro Thema.

Für eine neue Sitzung zuerst [Windows-Entwicklungsumgebung](WINDOWS-DEV-SETUP.md)
und [Konfigurieren und kompilieren](BUILDING.md) lesen; der Einstieg für Agenten
ist zusätzlich in [AGENTS.md](../../AGENTS.md) im Projektstamm verankert.

## Vorhandene Anleitungen

- [Am Originalprojekt mitarbeiten](CONTRIBUTING-WORKFLOW.md): Fork, Synchronisierung,
  eingerichteter Windows-Arbeitsbranch, getrennte Commits und der Weg zum späteren
  Pull Request ins Originalprojekt.
- [Aufgaben und Arbeitsteilung](TASKS.md): bisherige Einschätzung zur autonomen
  Umsetzung, Beteiligung beim Testen und vorgeschlagener Einstieg.
- [Windows-Entwicklungsumgebung](WINDOWS-DEV-SETUP.md): installierte Versionen,
  genaue Speicherorte, Verbindungen zum Projekt und überprüfter Stand.
- [Konfigurieren und kompilieren](BUILDING.md): Umgebung laden, CMake ausführen,
  Release/Debug bauen und Protokolle finden.
- [Voraussetzungen wiederherstellen](WINDOWS-PREREQUISITES.md): Quellen,
  Prüfsummen, Installationsskripte, Reihenfolge und behobene Installationsprobleme.

## Weitere Notizen ablegen

Neue Dateien bei Bedarf hier ergänzen und oben verlinken, zum Beispiel:

- `DEBUGGING.md`: nachvollziehbare Fehleranalysen und Lösungen.
- `ARCHITECTURE-NOTES.md`: Erkenntnisse zum bestehenden Code und dessen Zusammenhängen.
- `GUI-SCALING.md`: Erkenntnisse zur GUI-Skalierung, sobald daran gearbeitet wird.

Bei technischen Erkenntnissen den betroffenen Code, die Schritte zur Prüfung und
offene Fragen festhalten; noch nicht überprüfte Aussagen entsprechend kennzeichnen.

Die Sammlung ist zunächst für den eigenen Fork gedacht; welche Dokumentation ins
Originalprojekt übernommen wird, entscheiden wir für den jeweiligen Pull Request.
