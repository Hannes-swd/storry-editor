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
| **Manuskript** | - | Schreibflaeche mit klickbaren Marken, Gliederung, Lesemodus, "Aktion aus Absatz", Word-Export |
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

## Das Manuskript

Hier wird geschrieben – der Text ist die Hauptsache, die Struktur fällt nebenbei ab. Im Text
stehen kurze Marken, die beim Schreiben anklickbare Variablen sind und beim Lesen zu Werten
werden:

| Marke | Bedeutung |
| --- | --- |
| `@Alice` | Verweis auf ein Element; zeigt den Namen, Klick öffnet die Details |
| `@Alice.age` | Wert des Feldes **zu diesem Zeitpunkt der Geschichte** (Mutations werden angewendet) |
| `@Characters/Main/Alice` | voller Pfad, falls zwei Elemente gleich heißen |
| `#Tag 5, 14:00` | ab hier gilt dieser Zeitpunkt |
| `## Kapitel 1` | Überschrift, erscheint in der Gliederung |
| `!act:…` | verknüpfte Aktion – mit `!` beim Tippen oder über „Aktion setzen“; im fertigen Text **unsichtbar**, rein organisatorisch |

Beim Tippen von `@` schlägt der Editor passende Elemente vor (Tab übernimmt). **„Aktion aus
Absatz"** macht aus dem Absatz am Cursor eine Aktion: Titel aus dem ersten Satz, Zeitpunkt aus
der letzten Zeitmarke, Beteiligte aus den `@`-Verweisen im Absatz – die Timeline füllt sich also
beim Schreiben, nicht durch Formulare.

Zeitmarken und Aktionsmarken stehen **nie** im fertigen Text – sie steuern nur, welche Werte
gelten und an welcher Stelle eine Aktion hängt. Beim Lesen lassen sie sich über „Marken
zeigen“ zur Orientierung einblenden.

**Lesen** zeigt denselben Text ohne Marken: Namen eingesetzt, Werte aufgelöst, Aktionen als
Verweis. Gespeichert wird beides – `Manuscript/manuscript.md` mit den Marken und
`Manuscript/gelesen.md` als fertiger Lesetext für Obsidian.

### Als Word-Datei herausgeben

„Als Word" in der Manuskript-Leiste – oder *Datei → Manuskript als Word (.docx)…* – schreibt
genau diesen fertigen Text als `.docx`: Werte fest eingetragen, Zeit- und Aktionsmarken nicht
enthalten. Der Projektname wird zum Titel, `## Kapitel` zu einer Word-Überschrift, Leerzeilen
trennen Absätze. Die Datei braucht kein Word zum Erzeugen – das Programm baut das Paket selbst.

## Verbindungen: eigene Vorlagen mit Rollen

Das Programm kennt keine festen Begriffe wie „Person" oder „Ort" – auch Verbindungen definierst
du selbst. Unter **Connections → „Typen verwalten…"** entsteht eine Vorlage:

```
Name:     Person an Ort
Rolle 1:  Person  →  Gruppe Characters   (oder "alle Gruppen")
Rolle 2:  Ort     →  Gruppe Locations
          beliebig viele weitere Rollen möglich

[x] Über die Timeline setzbar (zeitlich)
    [x] pro Element nur eine gleichzeitig
    [x] Band in der Timeline zeichnen
    Band läuft in der Spur von: Person    Beschriftet mit: Ort
```

Beim Setzen bietet jede Rolle nur Elemente aus den erlaubten Gruppen an. **Zeitliche** Typen
haben ein „Ab" und optional ein „Bis"; bei **exklusiven** Typen endet die vorherige Setzung
automatisch, sobald eine neue beginnt – damit ist ein Aufenthaltsort abgebildet, ohne dass das
Programm je das Wort „Ort" kennt.

Sichtbar wird das an drei Stellen:

- **Timeline:** farbiges Band hinter der Spur, beschriftet mit dem anderen Ende
  (`▓ Castle ▓│▓ Forest ▓│▓ Castle →`). Rechtsklick in einer Spur setzt eine neue Verbindung,
  Klick aufs Band wählt sie aus. Über **„Bänder"** lassen sich Typen ein- und ausblenden.
- **Connections-Graph:** unter jedem Kreis steht, was gerade gilt (`ist an Ort: Castle`).
  Typen mit mehr als zwei Rollen werden als Stern mit Knotenpunkt gezeichnet. Der Schieber
  **„Stand an Tag N"** zeigt die Beziehungslage zu einem Zeitpunkt.
- **Details:** Rollen, Zeitraum und die beteiligten Elemente der gewählten Verbindung.

Projekte aus einer älteren Version laden weiter: aus dem bisherigen Textfeld `type` entsteht
automatisch ein Typ mit zwei Rollen, `source`/`target` werden die Rollen, Datumsfelder werden
geparst.

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
| Verbindungs-Dialog → Typ | Verbindungstypen (eigene Vorlagen mit Rollen, siehe oben) |
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
