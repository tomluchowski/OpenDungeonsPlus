# Aufgaben und Arbeitsteilung

Stand: 5. September 2026, festgehalten aus dem bisherigen Gespräch.

## Ziel der Zusammenarbeit

Codex soll die Umsetzung möglichst selbstständig übernehmen; der Nutzer möchte
hauptsächlich das Testen und die visuelle Beurteilung übernehmen.

Die folgenden Bewertungen sind die bisherige Einschätzung aus dem Gespräch, keine
Garantie einer vollständig autonomen Umsetzung. Eine technische Detailprüfung der
einzelnen Aufgaben und ihrer konkreten Anforderungen steht noch aus.

## Besprochene Aufgaben

| Nr. | Aufgabe | Bisherige Einschätzung | Beteiligung oder offene Voraussetzung |
| --- | --- | --- | --- |
| 1 | Windows-Build zuverlässig machen | Sehr gut machbar, aber vollständige Autonomie nicht garantiert. | Lokale Windows- oder Abhängigkeitsprobleme können Eingriffe des Nutzers erfordern. |
| 2 | Ogre-14-Port stabilisieren | Machbar eingeschätzt, aber großer Architektur- und Kompatibilitätsbereich ohne belastbare Vollständigkeitsgarantie. | Umfang und Hindernisse müssen zunächst am Code geprüft werden. |
| 3 | GUI skalierbar machen | Als voraussichtlich vollständig durch Codex umsetzbar eingeschätzt. | Nutzer testet insbesondere die Darstellung; eine lokal baubare und startende Spielversion wird für die Ergebnisprüfung benötigt. |
| 4 | Maus/Cursor sauber lösen | Als voraussichtlich vollständig durch Codex umsetzbar eingeschätzt. | Nutzer prüft das Verhalten im Spiel; der konkrete Änderungsumfang ist noch festzulegen. |
| 5 | UI verständlicher machen | Größtenteils durch Codex umsetzbar eingeschätzt. | Offene Entscheidungen zu Bedienung und Texten müssen geklärt oder ausdrücklich delegiert werden. |
| 6 | Tutorial / Onboarding | Durch Codex umsetzbar eingeschätzt, sofern Inhalt und Ablauf selbst definiert werden dürfen. | Die dafür notwendige Entscheidungsfreiheit wurde bisher nur als Voraussetzung genannt und noch nicht erteilt. |

## Vorgeschlagener Einstieg

Im Gespräch wurden Aufgabe 3 und 4 als Einstieg für möglichst wenig Eigenaufwand
vorgeschlagen; das ist noch kein Auftrag zur Umsetzung dieser Aufgaben.

Für die GUI-Skalierung zuerst eine Umgebung herstellen, in der das Spiel lokal
kompiliert und startet; die besprochenen Voraussetzungen stehen in der
[Windows-Dokumentation](WINDOWS-DEV-SETUP.md).

## Offene Entscheidungen und Prüfung

Fehlende funktionale, gestalterische oder inhaltliche Entscheidungen werden nicht
stillschweigend getroffen; sie müssen aus dem Projekt eindeutig hervorgehen, vom
Nutzer beantwortet oder ausdrücklich an Codex delegiert werden.

Manuelle Spieltests, QA und die visuelle Abnahme übernimmt der Nutzer; Ergebnisse
und dabei gefundene Fehler werden anschließend gemeinsam ausgewertet.
