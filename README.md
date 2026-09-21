# Story Editor

*[English version: README.en.md](README.en.md)*

Schreib-Management-Software für komplexe Geschichten – C++17 + Dear ImGui (Win32/DirectX 11),
Speicherung in einem Obsidian-Vault (Markdown + JSON).

Umsetzung der Spezifikation in `claude.md`.

## Bauen

Voraussetzungen: Visual Studio 2022 (MSVC), CMake ≥ 3.20, Internet beim ersten Konfigurieren
(ImGui, nlohmann/json und stb_image werden automatisch geladen).

```sh
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
./build/bin/StoryEditor.exe
```

Kommandozeile:

| Aufruf | Wirkung |
| --- | --- |
| `StoryEditor.exe` | normale GUI, öffnet zuletzt benutztes Projekt |
| `StoryEditor.exe --selftest` | Prüft Zeitlogik, Templates, Mutations, Markdown- und Vault-Roundtrip (43 Checks), Exitcode 0 = alles ok |
| `StoryEditor.exe --demo <Ordner>` | legt ein Demo-Projekt (Alice, Bob, Castle, Sword, Aktionen, Connections) an |

## Fenster

| Fenster | Spec | Inhalt |
| --- | --- | --- |
| **Timeline** | 3.1 | Spuren je Gruppe/Element, Ereignisse als Punkte, nicht-lineare Zeitskala (dichte Bereiche gedehnt, leere Zeiträume komprimiert mit „~ 5 Tage“-Markierung), Hover-Tooltip, Klick, Rechtsklick-Menü, Drag & Drop zum Verschieben (Ghost-Vorschau), Doppelklick = neue Aktion, Strg+Rad = Zoom, Filter nach Gruppe/Typ/Element/Zeitfenster, verschiebbare Spurtitel-Breite |
| **Gruppen-Manager** | 3.2 | Hierarchiebaum mit Farben, Drag & Drop zum Umsortieren, Kontextmenüs, Template-Editor mit Feld-Dialog (9 Feldtypen, Pflichtfelder, Defaults, Enum-Optionen), Vererbung an Untergruppen |
| **Details** | 3.1.7 / 3.2.3 | Detailpanel für Element, Aktion, Gruppe oder Verbindung – Felder inline editierbar, Werteverlauf über die Zeit, Beziehungen, verknüpfte Aktionen |
| **Aktionen** | 3.5 | Tabelle mit Sortierung, Filtern (Beteiligte, Typ, Tag, Zeitraum, Volltext), ausklappbaren Detailzeilen, Live-Sync mit der Timeline |
| **Story Visualizer** | 3.3 | Read-only-Chronik: Tagesüberschriften, „… vergehen“-Lücken, Attribut-Änderungen, Filter nach Element/Strang/Wichtigkeit |
| **Connections** | 3.4 | Netzwerkgraph, Knoten ziehen, Kanten mit Typ-Label und Pfeil, Blöcke (gruppierte Verbindungen), Fokus auf ein Element mit Grad 1–3, Typ-Filter |
| **Dateimanager** | 3.6 | Strukturierte Ansicht (Gruppen → Dateien) und rohe Vault-Ansicht, Upload (Dialog oder Drag & Drop aus dem Explorer), Umbenennen/Verschieben/Löschen, Bildvorschau, Datei mit Element verknüpfen |
| **Einstellungen** | 2.3 / 7.1 | Sprache, Design (Hell/Dunkel), alle Farben des Farbschemas, Gruppenfarben, Timeline-Parameter, Verwaltung der eigenen Listen; gespeichert in `%APPDATA%/StoryEditor/settings.json` |

## Sprache

Die Oberfläche gibt es auf **Deutsch und Englisch**. Umschalten unter **Ansicht → Sprache /
Language** oder in **Einstellungen → Farben**. Beim allerersten Start entscheidet die
Windows-Anzeigesprache, danach steht die Wahl in `settings.json`.

Die Sprache gilt auch für erzeugte Texte: Zeitangaben (`Tag 5, 14:00` / `Day 5, 14:00`), Dauern
und die Abschnittsüberschriften in den Element-`.md`-Dateien. Beim Lesen werden immer beide
Schreibweisen akzeptiert – ein Sprachwechsel macht also keinen bestehenden Vault kaputt.

## Design

Zwei mitgelieferte Farbwelten, umschaltbar über **Ansicht → Design** oder
**Einstellungen → Farben**:

- **Hell** (Standard): Papierweiße Flächen, dunkle Schrift, Graphit als Akzent
- **Dunkel**: neutrales Dunkelgrau mit weißer Schrift

Beide kommen ohne Blaustich aus – die Akzentfarbe ist in beiden Fällen ein
Grauton. Interaktive Flächen (Buttons, Tabs, Header) werden aus Panel- und
Akzentfarbe gemischt, deshalb bleibt die Schrift in beiden Designs lesbar.
Die automatisch vergebenen Gruppenfarben passen ihre Sättigung und Helligkeit
ans Design an.

Jede Einzelfarbe lässt sich darunter weiter anpassen; **Design neu laden**
stellt das gewählte Preset wieder her. Die Wahl landet in `settings.json`.

## Eigene Einträge statt fester Auswahl

Überall, wo eine Auswahlliste angeboten wird, sind die Vorgaben nur ein Startpunkt – jedes
Dropdown hat unten das Feld **„Eigener Eintrag"**, in das ein neuer Wert getippt wird (Enter oder
`+`). Der Eintrag wird sofort übernommen und in `metadata.json` gespeichert:

| Stelle | Liste |
| --- | --- |
| Aktions-Dialog → Typ | Aktions-Typen (Dialogue, Action, … + eigene) |
| Aktions-Dialog → Strang | Handlungsstränge |
| Aktions-Dialog → Tags | frei tippbar, `Vorhandene…` bietet bereits benutzte Tags an |
| Aktions-Dialog → Mutations → Feld | ein neuer Feldname legt das Feld direkt am Element an |
| Verbindungs-Dialog → Typ | Beziehungstypen (married_to, … + eigene) |
| Verbindungs-Dialog → Block | ein neuer Name erzeugt den Block |
| Element-Felder vom Typ Enum | neue Option landet im Template der definierenden Gruppe |
| Template-Editor → Feld | Feldname, Beschreibung, Enum-Optionen sind ohnehin frei |

Verwaltet werden die Listen unter **Einstellungen → Eigene Listen**: dort steht pro Eintrag, wie
oft er benutzt wird; Umbenennen zieht alle Verwendungen mit, Löschen setzt sie auf einen anderen
Eintrag um. Werte, die in den Daten vorkommen, aber in der Liste fehlen, werden beim Laden
automatisch ergänzt. Fest bleiben nur die neun Feld-Datentypen (Text, Integer, …) und die
Zeiteinheiten – die sind im Code verankert.

Fensterlayout: Blender-artiges Docking (ImGui-Dockspace, Multi-Viewport – Fenster können auch
aus dem Hauptfenster gezogen werden). Positionen/Größen landen in
`%APPDATA%/StoryEditor/imgui_layout_v2.ini`, sichtbare Fenster in `settings.json`.

## Tastenkürzel

`Strg+N` neues Element · `Strg+Shift+N` neues Projekt · `Strg+G` neue Gruppe · `Strg+T` neue
Aktion · `Strg+S` speichern · `Strg+Z`/`Strg+Y` rückgängig/wiederholen · `Strg+Rad` Timeline
zoomen · `Shift+Rad` horizontal scrollen.

## Datenablage (Obsidian-Vault)

```
MyStory/
├── metadata.json                     Projekt, Gruppen, Templates, Farben, Graph-Positionen
├── Characters/Main/Alice.md          ein Element = eine .md-Datei
├── Locations/Castle.md
├── Objects/Sword.md
├── Actions/actions.json              Aktionen inkl. Mutations, Tags, Anhängen
├── Connections/relationships.json    Verbindungen und Blöcke
├── Timeline/events.md                generierte Leseübersicht für Obsidian
└── Assets/Images, Assets/Documents   hochgeladene Dateien
```

Elementdatei:

```markdown
# Alice

<!-- story-editor: id=el_...; group=Characters/Main -->
> Diese Datei wird vom Story Editor verwaltet. Bitte ausschliesslich ueber die UI bearbeiten.

## Felder
- name: Alice
- age: 27
- description: Eine mutige Ritterin.

## Text
Freitext

## Beziehungen
- married_to -> [[Bob]]

## Verknuepfte Aktionen
- Tag 5, 14:00 - Alice erhaelt das Schwert
```

`## Beziehungen` und `## Verknuepfte Aktionen` werden aus actions/relationships generiert;
gelesen wird nur `## Felder` und `## Text`.

**.md-Dateien schreibt ausschließlich das Programm.** Ein File-Watcher erkennt externe
Änderungen und fragt mit **Reload / Merge / Cancel** nach (Reload = Datei gewinnt, Merge =
externe Feldwerte übernehmen und zusätzliche Programmfelder behalten, Cancel = interne Daten
zurückschreiben).

## Zeitmodell

Zeit wird als Minuten seit Geschichtsbeginn gespeichert; der fiktive Kalender hat 30-Tage-Monate
und 12-Monats-Jahre. Eingaben wie `Tag 5, 14:30`, `Day 5`, `Jahr 2, Monat 3, Tag 15` oder `14:30`
werden erkannt, Aktionen können auch relativ zu einer anderen Aktion liegen („120 Minuten nach
…“) und werden daraus berechnet.

Attribut-Änderungen (Mutations) hängen an Aktionen: `Alice.age: 27 → 28`. Der Wert eines Feldes
zu einem Zeitpunkt ergibt sich aus dem Basiswert plus allen Mutations bis dahin
(`Project::valueAt`), sichtbar im Detailpanel und im Story Visualizer.

## Speichern & Undo

Nach jeder Änderung wird automatisch gespeichert (abschaltbar unter Ansicht → Automatisch
speichern). Undo/Redo arbeitet mit Snapshots des gesamten Projekts (64 Schritte) und schreibt
den wiederhergestellten Stand direkt in den Vault.

## Quellcode

```
src/core     Datenmodell, Zeitlogik, Vault-I/O (Markdown+JSON), File-Watcher, Selbsttest
src/app      Win32/DX11-Host, Plattform-Helfer (Dialoge, Unicode-Pfade), Texture-Cache
src/ui       Editor-Zustand (Undo, Auswahl, Speicher-Queue), Farbschema, Sprache, Dialoge, Fenster
```

Farben kommen ausschließlich aus `ColorScheme` (`src/ui/Theme.h`) bzw. aus den Gruppenfarben –
keine fest verdrahteten RGB-Werte in den Fenstern. UI-Texte laufen über `TR("...")` mit dem
deutschen Text als Schlüssel; die Tabelle steht in `src/ui/Lang.cpp`.

## Bekannte Grenzen

- Backend ist Win32/DirectX 11 (kein Linux/macOS-Port).
- Markdown wird im Dateimanager als Rohtext angezeigt, nicht gerendert.
- Kein Plugin-System (Phase 3 der Spezifikation).
