[<kbd> &nbsp; ← Übersicht &nbsp; </kbd>](../../README.md)
[<kbd> &nbsp; ← Zurück: 6 · Ereignisse &nbsp; </kbd>](06-aktionen.md)
[<kbd> &nbsp; Weiter: 8 · Beziehungen → &nbsp; </kbd>](08-connections.md)

---

# 📅 Kapitel 7 · Die Timeline

> **Worum geht's hier?** Um die Landkarte deiner Geschichte. Eine Zeile pro Figur,
> Ort und Ding – und darauf jedes Ereignis als Punkt.

---

## 7.1 So sieht sie aus

![Die Timeline](../bilder/timeline.png)

```
                  ◀────────────── Zeit läuft nach rechts ──────────────▶
                  TAG 2   TAG 3   TAG 5     TAG 8      TAG 19    TAG 40
  ┌─────────────┬──────────────────────────────────────────────────────┐
  │             │        ~ 4 Tage        ~ 5 Tage       ~ 4 Wochen     │ ← gestauchte
  │ ▼ Characters│                                                      │   Leerzeiten
  │  ▼ Main     │                                                      │
  │    ● Alice  │ ▓Burg▓●━━●━━━━━▓Wald▓●━━━━━━━━━━━▓Burg▓              │
  │    ● Bob    │ ▓Burg▓●━━●━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━●    │
  │  ▼ NPCs     │                                                      │
  │    ● Guard  │                                              ●       │
  │ ▼ Locations │                                                      │
  │    ● Castle │ ●━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━●    │
  │    ● Forest │                 ●                                    │
  │ ▼ Objects   │                                                      │
  │    ● Sword  │                 ●                                    │
  └─────────────┴──────────────────────────────────────────────────────┘
       Spuren                     Ereignisse als Punkte
```

| Element | Bedeutung |
|:--|:--|
| **Spur** (Zeile) | Ein Element oder eine Gruppe. ▼/▶ klappt zu und auf |
| **Punkt** ● | Ein Ereignis, in dem dieses Element beteiligt ist. Farbe = Gruppenfarbe |
| **Band** ▓…▓ | Eine Verbindung, die über diesen Zeitraum gilt (→ [Kapitel 8](08-connections.md)) |
| **~ 4 Wochen** | Ein Zeitraum ohne Ereignisse, zusammengestaucht |

---

## 7.2 Die nicht‑lineare Zeitachse

Ein Problem jeder Zeitleiste: In deiner Geschichte passiert an einem Tag sehr viel und
danach ein Jahr lang nichts. Maßstabsgetreu wäre das unbrauchbar.

```
  Echt maßstäblich:
  ●●●●●────────────────────────────────────────────────────────────●
  alles an     ein Jahr, in dem nichts passiert – aber 95 % der Breite
  einem Tag

  Wie das Programm es zeichnet:
  ●───●───●───●───●   ~ 1 Jahr   ●
  die dichten Stellen werden gedehnt, die leeren gestaucht
```

Steuerung über die Filterleiste:

| Schalter | Wirkung |
|:--|:--|
| **Einheit** | Minute · Stunde · Tag · Woche · Monat · Jahr – wie fein die Beschriftung ist |
| **Zoom** | Schieberegler; oder <kbd>Strg</kbd> + Mausrad direkt über der Timeline |
| **☑ Komprimieren** | Leere Zeiträume stauchen. Ausschalten = echter Maßstab |

<kbd>Umschalt</kbd> + Mausrad scrollt waagerecht.

---

## 7.3 Mit Ereignissen arbeiten

| Aktion | Wirkung |
|:--|:--|
| 🖱️ **Darüberfahren** | Tooltip mit Titel, Anfang der Beschreibung, Zeit und Beteiligten |
| 🖱️ **Klick** | Auswählen – Details‑Fenster und Aktionen‑Panel springen mit |
| 🖱️ **Ziehen** | Zeitpunkt ändern. Eine durchscheinende Vorschau zeigt, wo es landen wird |
| 🖱️ **Doppelklick auf freie Stelle** | Neues Ereignis zu dieser Zeit, mit diesem Element als Beteiligtem |
| 🖱️ **Rechtsklick** | Menü: Bearbeiten, Duplizieren, Löschen, Verbindung setzen |

Beim Loslassen nach dem Ziehen wird sofort in den Vault geschrieben – und das
Aktionen‑Panel zeigt die neue Zeit im selben Moment.

---

## 7.4 Spuren

```
 ▼ Characters          ← Klick auf ▼ klappt die ganze Gruppe zu
   ▼ Main
     ● Alice           ← eine Spur je Element
     ● Bob
   ▶ NPCs              ← zugeklappt: Guard ist ausgeblendet
```

| | |
|:--|:--|
| **Auf‑/Zuklappen** | ▼ / ▶ links vom Namen |
| **Spaltenbreite** | Die senkrechte Trennlinie zwischen Namen und Zeitfläche ziehen |
| **Zeilenhöhe** | Einstellungen → *Zeilenhöhe Timeline* |
| **Farbe** | Der Punkt vor dem Namen zeigt die Gruppenfarbe |

---

## 7.5 Filter

Zwei Zeilen über der Zeitfläche:

| Filter | Wirkung |
|:--|:--|
| **Gruppen‑Filter** | Nur bestimmte Gruppen zeigen (Mehrfachauswahl) |
| **Typ‑Filter** | Nur bestimmte Ereignis‑Typen |
| **Element suchen…** | Live‑Suche über alle Spurnamen |
| **Zeitfenster** | Von–bis eingrenzen |
| **Bänder** | Welche Verbindungs‑Typen als Band gezeichnet werden |
| **Filter zurücksetzen** | Alles wieder anzeigen |

Aktive Filter werden farblich hervorgehoben, damit du nicht rätselst, warum etwas
fehlt.

---

## 7.6 Timeline oder Aktionen‑Panel?

Beides zeigt dieselben Daten. Nimm das, was gerade zur Frage passt:

| | 📅 Timeline | 📋 Aktionen‑Panel |
|:--|:--|:--|
| **Form** | Punkte auf einer Achse | Tabelle |
| **Stark bei** | *Wann* etwas passiert, was gleichzeitig läuft, wo Lücken sind | Suchen, sortieren, Details lesen, viele Felder ändern |
| **Bearbeiten** | Ziehen | Formular |
| **Sortierung** | immer zeitlich | frei wählbar |

Beide Fenster lassen sich unabhängig öffnen und schließen – und sind immer synchron.

---

## ✅ Kurz gesagt

- Eine **Spur** je Element, ein **Punkt** je Ereignis, ein **Band** je Verbindung
- Leere Zeiträume werden gestaucht, dichte gedehnt – abschaltbar über **Komprimieren**
- **Ziehen** verschiebt ein Ereignis in der Zeit, **Doppelklick** legt ein neues an
- <kbd>Strg</kbd>+Rad zoomt, <kbd>Umschalt</kbd>+Rad scrollt

---

[<kbd> &nbsp; ← Übersicht &nbsp; </kbd>](../../README.md)
[<kbd> &nbsp; ← Zurück: 6 · Ereignisse &nbsp; </kbd>](06-aktionen.md)
[<kbd> &nbsp; Weiter: 8 · Beziehungen → &nbsp; </kbd>](08-connections.md)
