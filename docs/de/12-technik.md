[<kbd> &nbsp; ← Übersicht &nbsp; </kbd>](../../README.md)
[<kbd> &nbsp; ← Zurück: 11 · Einstellen & Bedienen &nbsp; </kbd>](11-einstellungen.md)

---

# 🔧 Kapitel 12 · Unter der Haube

> **Für wen ist dieses Kapitel?** Für Neugierige, die wissen wollen, was in den
> Dateien steht – und für alle, die am Quellcode arbeiten wollen.
> Zum Schreiben brauchst du nichts davon.

---

## 12.1 Der Vault im Detail

```
MyStory/
├── metadata.json                     Projekt, Gruppen, Vorlagen, Farben,
│                                     eigene Listen, Graph-Positionen
├── Characters/
│   ├── Main/
│   │   ├── Alice.md                  ein Element = eine .md-Datei
│   │   └── Bob.md
│   └── NPCs/
│       └── Guard.md
├── Locations/
│   ├── Castle.md
│   └── Forest.md
├── Objects/
│   └── Sword.md
├── Actions/
│   └── actions.json                  Ereignisse inkl. Änderungen, Tags, Anhängen
├── Connections/
│   └── relationships.json            Verbindungen, Typen mit Rollen, Blöcke
├── Manuscript/
│   ├── manuscript.md                 dein Text mit Marken
│   └── gelesen.md                    Lesefassung (generiert)
├── Timeline/
│   └── events.md                     Leseübersicht (generiert)
└── Assets/
    ├── Images/
    └── Documents/
```

| Format | Wofür | Warum |
|:--|:--|:--|
| **Markdown** | Elemente, Manuskript | Von Menschen lesbar, Obsidian‑kompatibel |
| **JSON** | Aktionen, Verbindungen, Vorlagen | Strukturierte Daten mit Listen und Verschachtelung |

---

## 12.2 Wie eine Elementdatei aussieht

```markdown
# Alice

<!-- story-editor: id=el_7f3a91; group=Characters/Main -->
> Diese Datei wird vom Story Editor verwaltet.
> Bitte ausschliesslich ueber die UI bearbeiten.

## Felder
- name: Alice
- age: 27
- description: Eine mutige Ritterin.

## Text
Freitext, der zu keinem Feld gehört.

## Beziehungen
- married_to -> [[Bob]]

## Verknuepfte Aktionen
- Tag 5, 14:00 - Alice erhaelt das Schwert
```

| Abschnitt | Wird gelesen? | Wird geschrieben? |
|:--|:--|:--|
| `<!-- story-editor: … -->` | ja – ID und Gruppe | ja |
| `## Felder` | **ja** | ja |
| `## Text` | **ja** | ja |
| `## Beziehungen` | nein | ja – aus `relationships.json` erzeugt |
| `## Verknuepfte Aktionen` | nein | ja – aus `actions.json` erzeugt |

Die beiden unteren Abschnitte werden bei jedem Speichern neu gebaut. Änderungen darin
sind also verloren – deshalb der Hinweis in der Datei selbst.

---

## 12.3 Der Schutz vor Handarbeit

Ein **File‑Watcher** beobachtet den Vault. Ändert etwas von außen eine `.md`‑Datei
(Obsidian, VS Code, ein Sync‑Dienst), meldet sich das Programm:

```
┌─ Externe Änderung ──────────────────────────────────────┐
│  ⚠️ Die Datei "Alice.md" wurde extern modifiziert!       │
│  Die Änderung kann inkonsistent mit den                 │
│  Programm-Daten sein.                                   │
│                                                         │
│         [ Reload ]     [ Merge ]     [ Cancel ]         │
└─────────────────────────────────────────────────────────┘
```

| Wahl | Was passiert |
|:--|:--|
| **Reload** | Die Datei gewinnt; der Stand im Programm wird verworfen |
| **Merge** | Externe Feldwerte werden übernommen, zusätzliche Programmfelder bleiben erhalten |
| **Cancel** | Die internen Daten werden zurückgeschrieben; die externe Änderung verschwindet |

---

## 12.4 Das Zeitmodell im Code

```
  Alles ist eine Zahl: Minuten seit Geschichtsbeginn.

  Tag 1, 00:00           →        0
  Tag 1, 14:30           →      870
  Tag 5, 14:00           →     6600
  Jahr 2, Monat 3, Tag 15 →   626400

  1 Tag   = 1440 Minuten
  1 Monat =   30 Tage
  1 Jahr  =  360 Tage
```

Die Umrechnung liegt in `src/core/StoryTime.cpp`; `parseStoryTime()` versteht deutsche
und englische Schreibweisen, `formatStoryTime()` schreibt in der aktuellen UI‑Sprache.

### Wie ein Wert zu einem Zeitpunkt berechnet wird

```cpp
Project::valueAt(element, feld, zeitpunkt)
   1. Startwert aus der Vorlage bzw. dem Element nehmen
   2. alle Aktionen bis `zeitpunkt` chronologisch durchgehen
   3. jede passende Änderung anwenden
   4. Ergebnis zurückgeben
```

Deshalb ergibt `@Alice.age` an zwei Stellen zwei verschiedene Zahlen, ohne dass
irgendwo ein zweiter Wert gespeichert wäre.

---

## 12.5 Der Quellcode

```
src/
├── core/        Datenmodell, Zeitlogik, Vault-I/O (Markdown+JSON),
│                Manuskript-Parser, Word-Export, File-Watcher, Selbsttest
├── app/         Win32/DX11-Host, Plattform-Helfer (Dialoge, Unicode-Pfade),
│                Texture-Cache
└── ui/          Editor-Zustand (Undo, Auswahl, Speicher-Queue),
                 Farbschema, Sprache, Dialoge, alle Fenster
```

| Datei | Zuständig für |
|:--|:--|
| `core/Manuscript.cpp` | Der einzige Ort, der die Marken `@`, `#`, `!act:`, `**`, `---` kennt |
| `core/StoryTime.cpp` | Zeit lesen und schreiben |
| `core/Project.cpp` | Datenmodell, `valueAt()`, Gruppen, Vorlagen |
| `core/VaultIO.cpp` | Lesen und Schreiben des Vaults |
| `core/DocxExport.cpp` | Baut das `.docx`-Paket von Hand (ZIP + OOXML, ohne Fremdbibliothek) |
| `ui/ManuscriptWindow.cpp` | Das Schreibfenster |
| `ui/Theme.cpp` | Farbschema – **alle** Farben kommen von hier |
| `ui/Lang.cpp` | Übersetzungstabelle |

### Zwei Hausregeln im Code

1. **Keine festverdrahteten Farben.** Alles kommt aus `ColorScheme` (`ui/Theme.h`)
   oder aus den Gruppenfarben.
2. **Alle UI‑Texte über `TR("...")`** mit dem deutschen Text als Schlüssel. Die
   Tabelle steht in `ui/Lang.cpp`; fehlt ein Eintrag, bleibt der deutsche Text stehen –
   die Oberfläche ist also nie kaputt.

---

## 12.6 Der Selbsttest

```sh
StoryEditor.exe --selftest
```

122 Prüfungen, keine Oberfläche, Exitcode `0` = alles gut. Der Bericht landet außerdem
in `%APPDATA%/StoryEditor/selftest.txt`.

Geprüft werden unter anderem:

| Bereich | Beispiele |
|:--|:--|
| **Zeit** | Parsen, Formatieren, Hin‑und‑Rück‑Umwandlung |
| **Vorlagen** | Vererbung, Standardwerte, nachträglich ergänzte Felder |
| **Änderungen** | Wert vor und nach einer Aktion, relative Zeitpunkte |
| **Manuskript** | Marken erkennen, Werte einsetzen, Fett/Kursiv, Szenenwechsel, Suche |
| **Vault** | Speichern und Wiederladen, Umbenennen, alte Projektformate |
| **Word‑Export** | Gültiges ZIP, Überschriften, Fett/Kursiv, keine Marken im Text |

---

## 12.7 Abhängigkeiten

| Bibliothek | Wofür | Wie sie hereinkommt |
|:--|:--|:--|
| [Dear ImGui](https://github.com/ocornut/imgui) `v1.92.9b-docking` | Die komplette Oberfläche | CMake `FetchContent` |
| [nlohmann/json](https://github.com/nlohmann/json) `3.11.3` | JSON lesen/schreiben | CMake lädt die Header‑Datei |
| [stb_image](https://github.com/nothings/stb) | Bilder für die Vorschau | dito |

Alles andere ist Win32 und DirectX 11 aus dem Betriebssystem. Die fertige `.exe` läuft
ohne installiertes VC++-Redistributable (statische CRT).

---

## 12.8 Bekannte Grenzen

| | |
|:--|:--|
| 🖥️ **Nur Windows** | Das Backend ist Win32 + DirectX 11. Kein Linux‑ oder macOS‑Port |
| 📄 **Markdown wird nicht gerendert** | Der Dateimanager zeigt `.md`-Dateien als Rohtext |
| 🧩 **Kein Plugin‑System** | War als Phase 3 der Spezifikation vorgesehen, ist nicht gebaut |

---

## ✅ Kurz gesagt

- Elemente sind Markdown, alles Strukturierte ist JSON – beides lesbar und Git‑tauglich
- Zeit ist intern nur eine Zahl: Minuten seit Geschichtsbeginn
- Werte zu einem Zeitpunkt werden **berechnet**, nicht gespeichert
- `--selftest` prüft 122 Dinge, bevor du dich auf etwas verlässt
- Farben und Texte sind konsequent aus dem Code herausgezogen

---

[<kbd> &nbsp; ← Übersicht &nbsp; </kbd>](../../README.md)
[<kbd> &nbsp; ← Zurück: 11 · Einstellen & Bedienen &nbsp; </kbd>](11-einstellungen.md)
