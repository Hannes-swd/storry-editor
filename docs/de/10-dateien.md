[<kbd> &nbsp; ← Übersicht &nbsp; </kbd>](../../README.md)
[<kbd> &nbsp; ← Zurück: 9 · Chronik & Detailpanel &nbsp; </kbd>](09-chronik.md)
[<kbd> &nbsp; Weiter: 11 · Einstellen & Bedienen → &nbsp; </kbd>](11-einstellungen.md)

---

# 💾 Kapitel 10 · Dateien, Bilder & Export

> **Worum geht's hier?** Bilder in die Geschichte holen, und die fertige Geschichte
> wieder herausbekommen – als Word‑Datei oder zum Lesen in Obsidian.

---

# Teil 1 · Der Dateimanager

**Fenster → Dateimanager**

## 10.1 Zwei Ansichten

```
  ┌─ Strukturiert ──────────┐     ┌─ Rohe Dateien ──────────┐
  │                         │     │                         │
  │  📁 Characters          │     │  📁 Actions             │
  │    📁 Main              │     │  📁 Assets              │
  │      📄 Alice.md        │     │  📁 Characters          │
  │      🖼️ alice.png        │     │  📁 Connections         │
  │      📄 Bob.md          │     │  📁 Locations           │
  │  📁 Locations           │     │  📁 Manuscript          │
  │      📄 Castle.md       │     │  📁 Objects             │
  │                         │     │  📄 metadata.json       │
  │  so wie das Programm    │     │  so wie es wirklich     │
  │  die Welt sortiert      │     │  auf der Platte liegt   │
  └─────────────────────────┘     └─────────────────────────┘
```

| Ansicht | Wofür |
|:--|:--|
| **Strukturiert** | Nach deinen Gruppen sortiert. Der normale Weg |
| **Rohe Dateien** | Der echte Ordnerinhalt, inklusive `metadata.json` und `Assets/` |

## 10.2 Was du hier machen kannst

| Knopf / Menü | Wirkung |
|:--|:--|
| **Datei hochladen…** | Dateiauswahl; die Datei wird **in den Vault kopiert** |
| **Drag & Drop** | Datei aus dem Explorer ins Programmfenster ziehen |
| **Neuer Ordner…** | Unterordner anlegen |
| **Vault öffnen** | Den Vault im Windows‑Explorer zeigen |
| Rechtsklick → **Öffnen** | Bild anzeigen bzw. Rohtext einblenden |
| Rechtsklick → **Umbenennen** | |
| Rechtsklick → **Verschieben nach…** | Zielordner relativ zum Vault |
| Rechtsklick → **Löschen** | Mit Rückfrage |
| Rechtsklick → **Mit Element verknüpfen** | Siehe unten |

> 📌 Hochgeladene Dateien werden **kopiert**, nicht verlinkt. Der Vault bleibt damit
> vollständig – du kannst ihn auf einen anderen Rechner mitnehmen.

## 10.3 Ein Bild an eine Figur hängen

```
  1. Bild hochladen        →  landet in  Assets/Images/alice.png
  2. Rechtsklick → "Mit Element verknüpfen"
  3. Element auswählen     →  Alice
  4. Fertig                →  In Alice.md steht jetzt der Verweis,
                              das Details-Fenster zeigt eine Vorschau
```

Alternativ legst du in der Vorlage ein Feld vom Typ **File** an
(→ [Kapitel 4](04-gruppen.md)) – dann hat jede Figur von Haus aus einen Portrait‑Platz.

## 10.4 Vorschau

| Dateityp | Ansicht |
|:--|:--|
| 🖼️ **Bilder** (png, jpg, …) | Vorschaubild, Klick zeigt es groß |
| 📄 **Markdown/Text** | Rohinhalt, **nur zum Ansehen** |
| 📦 **Alles andere** | Nur Name und Größe |

> 🔴 Auch hier gilt: `.md`‑Dateien **nicht** von Hand bearbeiten. Der Dateimanager
> zeigt sie deshalb bewusst nur an.

---

# Teil 2 · Herausgeben

## 10.5 Als Word‑Datei

**Manuskript → Datei → Als Word**
oder **Datei → Manuskript als Word (.docx)…**

Was dabei entsteht:

| Im Manuskript | In der Word‑Datei |
|:--|:--|
| Projektname | Titel auf der ersten Seite – außer das Manuskript hat selbst einen Titel (`# …`) |
| `# Titel` | Titel |
| `## Kapitel 1` | Überschrift 1 |
| `### Szene` | Überschrift 2 |
| `@Alice` | `Alice` |
| `@Alice.age` | die Zahl, die zu dieser Stelle gehört |
| Fett, kursiv, unterstrichen, durchgestrichen, hoch‑/tiefgestellt | dieselbe Formatierung in Word |
| Schriftart, Größe, Farbe, Hervorhebung | dieselbe Formatierung in Word |
| Ausrichtung, Aufzählung, Nummerierung | dieselbe Formatierung in Word |
| `---` | zentrierte Trennung `* * *` |
| `#Tag 5, 14:00` | **weg** |
| `!act:…` | **weg** |
| Kommentare | Word‑Kommentare am Rand |
| Lesezeichen | Word‑Textmarken |
| `==markiert==`, Hervorhebung in Word‑Farben | echter Word‑Textmarker (andere Farben als Schattierung) |
| `* Punkt`, `+ Punkt` (wie in Obsidian) | Aufzählung |
| `***`, `* * *`, `___` | zentrierte Trennung `* * *` |
| jede Zeile | ein Absatz – wie auf der Seite im Editor |
| Einzüge und Tabstopps aus dem Lineal | echte Word‑Einzüge und ‑Tabstopps |
| Grundschrift, Zeilenabstand, Ränder, Papierformat | aus **Layout** und dem Lineal |

Die Datei ist ein normales `.docx` und lässt sich mit Word, LibreOffice oder Google
Docs öffnen. Du brauchst kein Word installiert zu haben – das Programm baut das Paket
selbst.

## 10.6 In Obsidian weiterlesen

Bei jedem Speichern legt das Programm **zwei** Fassungen deines Textes ab:

```
  Manuscript/
    manuscript.md   ← dein Arbeitstext, mit allen Marken
                       (das Programm liest und schreibt diese Datei)

    gelesen.md      ← die Lesefassung: Werte eingesetzt, Marken weg,
                       Fett/Kursiv als normales Markdown
                       (nur für dich zum Lesen)
```

Öffne den Vault‑Ordner in Obsidian und du kannst `gelesen.md` wie jede andere Notiz
lesen und durchsuchen. Auch die Elementdateien (`Alice.md`) sind normale Notizen mit
funktionierenden `[[Wiki-Links]]`.

## 10.7 Was noch generiert wird

| Datei | Inhalt |
|:--|:--|
| `Timeline/events.md` | Alle Ereignisse chronologisch als Leseübersicht |
| `Alice.md` → `## Beziehungen` | Aus den Verbindungen erzeugt |
| `Alice.md` → `## Verknuepfte Aktionen` | Aus den Aktionen erzeugt |

Diese Abschnitte werden bei jedem Speichern **neu geschrieben** – Änderungen darin
gehen verloren. Gelesen wird aus einer Elementdatei nur `## Felder` und `## Text`.

---

## ✅ Kurz gesagt

- Der **Dateimanager** holt Bilder in den Vault und verknüpft sie mit Elementen
- Dateien werden **kopiert**, der Vault bleibt vollständig
- **Als Word speichern** erzeugt ein fertiges Manuskript mit Überschriften und Formatierung
- `gelesen.md` ist die Lesefassung für Obsidian – deine Arbeitsdatei bleibt `manuscript.md`

---

[<kbd> &nbsp; ← Übersicht &nbsp; </kbd>](../../README.md)
[<kbd> &nbsp; ← Zurück: 9 · Chronik & Detailpanel &nbsp; </kbd>](09-chronik.md)
[<kbd> &nbsp; Weiter: 11 · Einstellen & Bedienen → &nbsp; </kbd>](11-einstellungen.md)
