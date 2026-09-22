[<kbd> &nbsp; ← Übersicht &nbsp; </kbd>](../../README.md)
[<kbd> &nbsp; ← Zurück: 10 · Dateien, Bilder & Export &nbsp; </kbd>](10-dateien.md)
[<kbd> &nbsp; Weiter: 12 · Unter der Haube → &nbsp; </kbd>](12-technik.md)

---

# ⚙️ Kapitel 11 · Einstellen & Bedienen

> **Worum geht's hier?** Sprache, Aussehen, eigene Auswahllisten, Tastenkürzel –
> und darum, wie Speichern und Rückgängig funktionieren.

---

## 11.1 Sprache

**Ansicht → Sprache / Language** – oder in den Einstellungen.

Die Umschaltung wirkt sofort, ohne Neustart, und betrifft nicht nur die Knöpfe:

| | Deutsch | English |
|:--|:--|:--|
| Zeitangaben | `Tag 5, 14:00` | `Day 5, 14:00` |
| Dauern | `3 Tage` | `3 days` |
| Abschnitte in `.md` | `## Felder` | `## Fields` |

> 🛟 **Kein Risiko:** Beim **Lesen** werden immer beide Schreibweisen akzeptiert. Ein
> Sprachwechsel macht also kein bestehendes Projekt kaputt.

Beim allerersten Start entscheidet die Windows‑Anzeigesprache; danach steht deine Wahl
in `settings.json`.

---

## 11.2 Design und Farben

**Ansicht → Design** oder **Einstellungen → Farben**

| Design | Beschreibung |
|:--|:--|
| ☀️ **Hell** (Standard) | Papierweiße Flächen, dunkle Schrift, Graphit als Akzent |
| 🌙 **Dunkel** | Neutrales Dunkelgrau mit weißer Schrift |

Beide kommen ohne Blaustich aus. Interaktive Flächen werden aus Panel‑ und Akzentfarbe
gemischt, damit die Schrift in beiden Designs lesbar bleibt.

### Jede Farbe einzeln

Darunter steht die komplette Palette:

```
  Text primär            Timeline Hintergrund
  Text sekundär          Timeline Raster
  Hintergrund            Timeline Spur (alternierend)
  Panel                  Timeline Lineal
  Akzent                 Auswahl
  Warnung                Hover
  Erfolg                 Ghost (Drag)
  Fehler                 Verbindungslinie
                         Knoten-Umriss
```

**Design neu laden** stellt das gewählte Preset wieder her, falls du dich verklickt hast.

> 🎨 **Im Code sind keine Farben festverdrahtet.** Alles kommt aus dem Farbschema bzw.
> aus den Gruppenfarben – deshalb lässt sich wirklich alles ändern.

### Gruppenfarben

Jede Gruppe hat ihre eigene Farbe (→ [Kapitel 4](04-gruppen.md)). Vergibt das Programm
sie automatisch, achtet es auf genügend Abstand zwischen den Farbtönen und passt
Sättigung und Helligkeit ans Design an.

---

## 11.3 Eigene Einträge statt fester Auswahl

Überall, wo eine Aufklappliste angeboten wird, sind die Vorgaben nur ein Startpunkt.
Unten im Menü steht immer ein Feld **„Eigener Eintrag"**:

```
 ┌─ Typ ──────────────────────┐
 │  Dialogue                  │
 │  Action                    │
 │  Scene Change              │
 │  Discovery                 │
 │  Conflict                  │
 │ ────────────────────────── │
 │  Eigener Eintrag:          │
 │  [ Rückblende        ] [+] │ ← tippen, Enter – fertig
 └────────────────────────────┘
```

Das gilt an diesen Stellen:

| Stelle | Liste |
|:--|:--|
| Aktions‑Dialog → Typ | Aktions‑Typen |
| Aktions‑Dialog → Strang | Handlungsstränge |
| Aktions‑Dialog → Tags | frei tippbar; *Vorhandene…* bietet schon benutzte an |
| Aktions‑Dialog → Änderungen → Feld | ein neuer Feldname legt das Feld direkt am Element an |
| Verbindungs‑Dialog → Typ | Verbindungstypen |
| Verbindungs‑Dialog → Block | ein neuer Name erzeugt den Block |
| Element‑Felder vom Typ **Enum** | neue Option landet in der Vorlage der Gruppe |

### Die Listen verwalten

**Einstellungen → Eigene Listen**

```
 ┌─ Aktions-Typen ────────────────────────────────────┐
 │  Dialogue        2×   [ Umben. ] [ ✕ ]             │
 │  Action          1×   [ Umben. ] [ ✕ ]             │
 │  Discovery       1×   [ Umben. ] [ ✕ ]             │
 │  Rückblende      0×   [ Umben. ] [ ✕ ]             │
 └────────────────────────────────────────────────────┘
      │                     │           │
      │                     │           └─ Löschen: fragt, worauf
      │                     │              die Verwendungen umziehen
      │                     └─ Umbenennen zieht alle Verwendungen mit
      └─ so oft wird der Eintrag im Projekt benutzt
```

Werte, die in den Daten vorkommen, aber in der Liste fehlen, werden beim Laden
automatisch ergänzt – du kannst also nichts verlieren.

> 🔒 **Fest bleiben nur zwei Dinge:** die neun Feld‑Datentypen und die Zeiteinheiten.
> Die sind im Code verankert.

---

## 11.4 Speichern

| Einstellung | Verhalten |
|:--|:--|
| ☑ **Automatisch speichern** (Standard) | Nach einer kurzen Tipp‑Pause wird geschrieben, spätestens nach 5 Sekunden |
| ☐ ausgeschaltet | Nur auf <kbd>Strg</kbd>+<kbd>S</kbd>, per Klick auf die Speicheranzeige oder beim Schließen |

Zu finden unter **Ansicht → Automatisch speichern**.

Den Stand siehst du rechts in der Manuskript‑Leiste:

| | |
|:--|:--|
| 🟢 **Gespeichert** | Alles liegt im Vault |
| 🟡 **[ Speichern ]** | Es gibt ungeschriebene Änderungen – Klick schreibt sofort |

> ⚙️ **Warum die Pause?** Ohne sie würde bei jedem einzelnen Tastendruck die ganze
> Manuskript‑Datei neu geschrieben. Die kurze Verzögerung schont Platte und Nerven.

---

## 11.5 Rückgängig

<kbd>Strg</kbd>+<kbd>Z</kbd> und <kbd>Strg</kbd>+<kbd>Y</kbd>, oder über **Bearbeiten**.

- **64 Schritte** werden vorgehalten
- Jeder Schritt ist ein Abzug des **ganzen Projekts** – auch Löschen, Umbenennen und
  Verschieben lassen sich zurücknehmen
- Im Menü steht dabei, **was** rückgängig gemacht wird: *„Rückgängig: Aktion aus Absatz"*
- Der wiederhergestellte Stand wird sofort in den Vault geschrieben

---

## 11.6 Tastenkürzel

| Taste | Wirkung |
|:--|:--|
| <kbd>Strg</kbd>+<kbd>N</kbd> | Neues Element |
| <kbd>Strg</kbd>+<kbd>Umschalt</kbd>+<kbd>N</kbd> | Neues Projekt |
| <kbd>Strg</kbd>+<kbd>G</kbd> | Neue Gruppe |
| <kbd>Strg</kbd>+<kbd>T</kbd> | Neue Aktion |
| <kbd>Strg</kbd>+<kbd>O</kbd> | Projekt öffnen |
| <kbd>Strg</kbd>+<kbd>S</kbd> | Speichern |
| <kbd>Strg</kbd>+<kbd>Z</kbd> / <kbd>Strg</kbd>+<kbd>Y</kbd> | Rückgängig / Wiederholen |

**Nur im Manuskript:**

| Taste | Wirkung |
|:--|:--|
| <kbd>Strg</kbd>+<kbd>F</kbd> | Suchen |
| <kbd>F3</kbd> / <kbd>Umschalt</kbd>+<kbd>F3</kbd> | Nächster / vorheriger Treffer |
| <kbd>Strg</kbd>+<kbd>B</kbd> | Fett |
| <kbd>Strg</kbd>+<kbd>I</kbd> | Kursiv |
| <kbd>@</kbd> | Vorschlagsliste für Elemente |
| <kbd>!</kbd> | Vorschlagsliste für Aktionen |
| <kbd>Tab</kbd> | Vorschlag übernehmen |
| <kbd>Esc</kbd> | Vorschlagsliste schließen |

**Nur in der Timeline:**

| Taste | Wirkung |
|:--|:--|
| <kbd>Strg</kbd>+Mausrad | Zoomen |
| <kbd>Umschalt</kbd>+Mausrad | Waagerecht scrollen |

Die Liste steht auch im Programm unter **Hilfe → Tastenkürzel**.

---

## 11.7 Wo die Einstellungen liegen

```
  %APPDATA%/StoryEditor/
  ├── settings.json          Sprache, Design, Farben, offene Fenster,
  │                          zuletzt benutzter Vault
  └── imgui_layout_v3.ini    Fensterpositionen und -größen
```

Diese Dateien gehören zum **Programm**, nicht zum Projekt – ein Vault bleibt also
auf jedem Rechner gleich.

**Ansicht → Layout zurücksetzen** stellt die Anfangsanordnung wieder her.

---

## ✅ Kurz gesagt

- Sprache und Design im laufenden Betrieb umschaltbar, ohne Risiko fürs Projekt
- Jede Auswahlliste lässt sich um **eigene Einträge** erweitern und unter
  **Einstellungen → Eigene Listen** pflegen
- Automatisch gespeichert wird nach einer kurzen Pause; die Anzeige rechts sagt dir den Stand
- **64 Schritte** Rückgängig, für das ganze Projekt

---

[<kbd> &nbsp; ← Übersicht &nbsp; </kbd>](../../README.md)
[<kbd> &nbsp; ← Zurück: 10 · Dateien, Bilder & Export &nbsp; </kbd>](10-dateien.md)
[<kbd> &nbsp; Weiter: 12 · Unter der Haube → &nbsp; </kbd>](12-technik.md)
