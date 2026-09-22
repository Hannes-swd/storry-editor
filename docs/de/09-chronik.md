[<kbd> &nbsp; ← Übersicht &nbsp; </kbd>](../../README.md)
[<kbd> &nbsp; ← Zurück: 8 · Beziehungen &nbsp; </kbd>](08-connections.md)
[<kbd> &nbsp; Weiter: 10 · Dateien, Bilder & Export → &nbsp; </kbd>](10-dateien.md)

---

# 📜 Kapitel 9 · Chronik & Detailpanel

> **Worum geht's hier?** Um die beiden Fenster zum *Nachschauen*: den **Story
> Visualizer**, der die Geschichte als Chronik erzählt, und das **Details**‑Panel,
> das alles über ein einzelnes Ding zeigt.

---

# Teil 1 · Das Details‑Panel

Das Fenster rechts. Es zeigt immer das, was du zuletzt angeklickt hast – egal wo.

![Details-Panel](../bilder/details.png)

## 9.1 Bei einem Element

```
 ┌───────────────────────────────────────────────┐
 │ ● Alice                                       │ ← Name + Gruppenfarbe
 │ Characters/Main/Alice                         │ ← wo es liegt
 │ [Bearbeiten][Duplizieren][Aktion hinzufügen]  │
 ├───────────────────────────────────────────────┤
 │ ▼ Felder                                      │
 │   name *                                      │ ← * = Pflichtfeld
 │   [ Alice                                   ] │
 │                                               │
 │   age                                         │
 │   [ 27 ]  [ − ] [ + ]  (?)                    │
 │   • Tag 11, 08:00: 27 → 28                    │ ← Werteverlauf!
 │                                               │
 │   description                                 │
 │   [ Eine mutige Ritterin mit unruhiger…     ] │
 │                                               │
 │   status                                      │
 │   [ Alive                                 ▼ ] │
 │                                               │
 │   [ + Eigenes Feld ]                          │
 │ ▶ Freitext                                    │
 │ ▼ Beziehungen                                 │
 │   married_to   [Bob] [Bearbeiten]             │
 │   [ + Verbindung ]                            │
 │ ▼ Erwähnt in Aktionen                         │
 │   Tag 1, 09:00  Alice kehrt zurück            │
 │   Tag 5, 14:00  Alice erhält das Schwert      │
 └───────────────────────────────────────────────┘
```

| Abschnitt | Was drinsteht |
|:--|:--|
| **Felder** | Alle Werte, direkt bearbeitbar. Darunter jeweils der **Verlauf**: welche Aktion diesen Wert wann geändert hat |
| **Freitext** | Ein freies Textfeld ohne Vorlage – für Notizen |
| **Beziehungen** | Alle Verbindungen dieses Elements, anklickbar |
| **Erwähnt in Aktionen** | Alle Ereignisse mit diesem Element, anklickbar |

> 💡 Der **Werteverlauf** unter einem Feld ist die schnellste Antwort auf
> *„Wann hat sich das eigentlich geändert?"* – ein Klick darauf springt zur Aktion.

## 9.2 Bei einer Aktion, Gruppe oder Verbindung

Dasselbe Fenster zeigt je nach Auswahl:

| Auswahl | Inhalt |
|:--|:--|
| ⚡ **Aktion** | Titel, Zeit, Beschreibung, Beteiligte, Typ, Tags, Änderungen, Anhänge |
| 📁 **Gruppe** | Name, Farbe, Vorlage, Anzahl der Elemente |
| 🔗 **Verbindung** | Typ, Rollen mit Elementen, Zeitraum, Beschreibung |

---

# Teil 2 · Der Story Visualizer

**Fenster → Story Visualizer**

Eine **Nur‑Lese‑Ansicht**: hier wird die Geschichte erzählt, nicht bearbeitet.

## 9.3 So liest sich das

```
 ╔═══════════════════════════════════════════════════════════╗
 ║  Nur-Lese-Ansicht: hier wird die Geschichte erzählt,      ║
 ║  nicht bearbeitet.                                        ║
 ╚═══════════════════════════════════════════════════════════╝

 ── TAG 1 ─────────────────────────────────────────────────────
   → Alice kehrt zurück
     Alice kommt nach Jahren in ihre Heimatstadt.
     Beteiligte: Alice · Castle

   → Alice trifft Bob am Markt
     Beteiligte: Alice · Bob

 ── 4 TAGE VERGEHEN ───────────────────────────────────────────

 ── TAG 5 ─────────────────────────────────────────────────────
   → Alice erhält das Schwert
     Alice findet ein uraltes Schwert in einer Ruine.
     • Alice.power_level: 1 → 3            ← Werteänderung

 ── 6 TAGE VERGEHEN ───────────────────────────────────────────

 ── TAG 11 ────────────────────────────────────────────────────
   → Alice' Geburtstag
     • Alice.age: 27 → 28
```

| Element | Bedeutung |
|:--|:--|
| **── TAG n ──** | Ein Tag mit Ereignissen |
| **── n TAGE VERGEHEN ──** | Eine Lücke ohne Ereignisse |
| **→ Titel** | Ein Ereignis |
| **• Feld: alt → neu** | Eine Werteänderung, ausgelöst von diesem Ereignis |

## 9.4 Die Filter oben

| Filter | Wirkung |
|:--|:--|
| **Fokus** | Nur Ereignisse mit einem bestimmten Element. Alles andere wird zu „… vergehen" zusammengefasst |
| **Strang** | Nur einen Handlungsstrang zeigen, z. B. nur die Hauptquest |
| **☑ nur Wichtiges** | Nur Ereignisse, die als *wichtig* markiert sind |
| **☑ voller Text** | Ganze Beschreibung statt nur der ersten Zeilen |
| **☑ Werteänderungen** | Die `• Feld: alt → neu`‑Zeilen ein‑/ausblenden |

Wenn ein Filter alles wegfiltert, steht da ehrlich
*„Keine Ereignisse für diese Auswahl."*

## 9.5 Wofür man das benutzt

| Frage | So beantwortest du sie |
|:--|:--|
| *Was erlebt Alice eigentlich alles?* | **Fokus** auf Alice – ihre ganze Geschichte in einer Spalte |
| *Wo sind meine Löcher?* | Auf die „… vergehen"‑Blöcke schauen |
| *Wie entwickelt sich eine Figur?* | **Werteänderungen** an, **Fokus** auf die Figur |
| *Trägt der Nebenstrang?* | **Strang** umschalten |

---

## 9.6 Drei Sichten auf dieselbe Geschichte

| | Wofür gemacht | Bearbeitbar? |
|:--|:--|:--|
| ✍️ **Manuskript** | Den Text schreiben | ja |
| 📅 **Timeline** | Zeitliche Verhältnisse sehen | ja |
| 📜 **Story Visualizer** | Die Handlung am Stück lesen | nein |

---

## ✅ Kurz gesagt

- **Details** zeigt immer das zuletzt Angeklickte – inklusive **Werteverlauf** pro Feld
- **Story Visualizer** erzählt die Geschichte chronologisch, mit Lücken als „… vergehen"
- **Fokus** auf eine Figur ergibt ihre komplette Biografie
- Der Visualizer ist bewusst nur zum Lesen da

---

[<kbd> &nbsp; ← Übersicht &nbsp; </kbd>](../../README.md)
[<kbd> &nbsp; ← Zurück: 8 · Beziehungen &nbsp; </kbd>](08-connections.md)
[<kbd> &nbsp; Weiter: 10 · Dateien, Bilder & Export → &nbsp; </kbd>](10-dateien.md)
