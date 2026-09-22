[<kbd> &nbsp; ← Übersicht &nbsp; </kbd>](../../README.md)
[<kbd> &nbsp; ← Zurück: 5 · Wie die Zeit funktioniert &nbsp; </kbd>](05-zeit.md)
[<kbd> &nbsp; Weiter: 7 · Die Timeline → &nbsp; </kbd>](07-timeline.md)

---

# ⚡ Kapitel 6 · Ereignisse & Werteänderungen

> **Worum geht's hier?** Um **Aktionen** – einzelne Dinge, die zu einem bestimmten
> Zeitpunkt passieren. Und darum, wie eine Aktion einen Wert verändert, sodass Alice
> ab ihrem Geburtstag überall 28 ist.

---

## 6.1 Was ist eine Aktion?

Eine **Aktion** ist ein Ereignis in deiner Geschichte. Sie hat:

```
   ┌──────────────────────────────────────────────────────┐
   │  ⚡ Alice erhält das Schwert                          │
   ├──────────────────────────────────────────────────────┤
   │  🕐 Wann        Tag 5, 14:00                          │
   │  📝 Beschreibung  Alice findet ein uraltes Schwert…   │
   │  👥 Beteiligte   Alice, Sword, Forest                 │
   │  🏷️ Typ          Discovery                            │
   │  🔖 Tags         Magie, Wendepunkt                    │
   │  📈 Änderungen   Alice.power: 1 → 3                   │
   └──────────────────────────────────────────────────────┘
```

> ⚠️ **Aktion ≠ Verbindung.** Eine Aktion ist ein **Moment** („Alice bekommt das
> Schwert"). Eine Verbindung ist ein **Zustand**, der eine Weile gilt („Alice besitzt
> das Schwert"). Verbindungen stehen in [Kapitel 8](08-connections.md).

---

## 6.2 Der schnellste Weg: aus einem Absatz

Du schreibst sowieso schon eine Szene. Mach einfach ein Ereignis daraus:

1. Cursor irgendwo in den Absatz setzen
2. **Einfügen → Aktion aus diesem Absatz**

Das Programm füllt alles selbst aus:

```
  Dein Absatz:
  ┌──────────────────────────────────────────────────────────┐
  │ In der Ruine lag das @Sword, halb im Staub. Es glomm     │
  │ **schwach**, als @Alice es aufhob.                       │
  └──────────────────────────────────────────────────────────┘
        │              │                    │
        │              │                    └─ Beteiligte: Alice, Sword
        │              └─ Zeitpunkt: aus der letzten Zeitmarke davor
        └─ Titel: erster Satz, mit eingesetzten Werten

  ⚡ Neue Aktion "In der Ruine lag das Sword, halb im Staub"
     Tag 5, 14:00 · Alice, Sword
```

Ans Ende des Absatzes kommt eine unsichtbare Marke (`!act:…`), die Text und Ereignis
verbindet. Danach kannst du den Titel im Details‑Fenster noch geradeziehen.

> 💡 So füllt sich die Timeline **beim Schreiben** – nicht durch Formulare.

---

## 6.3 Das Aktionen‑Panel

![Das Aktionen-Panel](../bilder/aktionen.png)

Alle Ereignisse als Tabelle. Das ist die Verwaltungs‑Ansicht; die Timeline
([Kapitel 7](07-timeline.md)) zeigt dieselben Daten grafisch.

| Spalte | Sortierbar? |
|:--|:--|
| **#** | ja – Klick auf den Kopf |
| **Titel** | ja, alphabetisch |
| **Zeit** | ja |
| **Typ** | ja |
| **Beteiligte** | ja |
| **Details** | „auf" klappt die Zeile aus |

### Filtern

Die Leiste oben schränkt die Liste ein:

| Filter | Wirkung |
|:--|:--|
| **Volltextsuche** | Sucht in Titel und Beschreibung |
| **Beteiligte filtern** | Nur Ereignisse mit bestimmten Elementen |
| **Typ** | Nur Dialogue, nur Conflict, … |
| **Tag** | Nur ein bestimmter Tag |
| **Zeitraum** | Von–bis |
| **Filter zurücksetzen** | Alles wieder anzeigen |

Über der Tabelle steht immer, wie viel gerade durchkommt: *„6 von 6 Aktionen"*.

### Eine Zeile ausklappen

```
 ────────────────────────────────────────────────────────────────
 #4 │ Alice erhält das Schwert            │ Tag 5, 14:00
 ────────────────────────────────────────────────────────────────
   Beschreibung:  Alice findet ein uraltes Schwert in einer Ruine.
   Beteiligte:    Alice · Sword · Forest       ← anklickbar
   Tags:          Magie, Wendepunkt
   Änderungen:    Alice.power_level: 1 → 3
   [ Bearbeiten ] [ Duplizieren ] [ In Timeline zeigen ] [ Löschen ]
 ────────────────────────────────────────────────────────────────
```

---

## 6.4 Eine Aktion von Hand anlegen

**+ Neue Aktion** im Panel, <kbd>Strg</kbd>+<kbd>T</kbd>, Doppelklick auf eine freie
Stelle in der Timeline, oder **Einfügen → Aktion verknüpfen → + Neue Aktion hier**.

```
┌─ Neue Aktion ───────────────────────────────────────────┐
│ Titel        [ Alice' Geburtstag                      ] │
│ Beschreibung [ Alice wird ein Jahr älter.             ] │
│              [                                        ] │
│ Zeitpunkt    [ Tag 11, 08:00        ]                   │
│              ☐ Relativ zu einer anderen Aktion          │
│ Typ          [ Action              ▼ ]                  │
│ Strang       [ Hauptstrang         ▼ ]                  │
│ Tags         [ Geburtstag                             ] │
│ ☐ Wichtig                                               │
│                                                         │
│ Beteiligte   ┌──────────────────────────────────────┐   │
│              │ Suchen…                              │   │
│              │ ☑ Alice      ☐ Bob      ☐ Guard      │   │
│              │ ☐ Castle     ☐ Forest   ☐ Sword      │   │
│              └──────────────────────────────────────┘   │
│                                                         │
│ Änderungen   [ + Änderung hinzufügen ]                  │
│                                                         │
│                              [ Speichern ] [ Abbrechen ]│
└─────────────────────────────────────────────────────────┘
```

**Typ** und **Strang** sind Aufklapplisten, die du selbst erweitern kannst – unten im
Menü steht immer ein Feld für einen eigenen Eintrag
(→ [Kapitel 11](11-einstellungen.md)).

---

## 6.5 Werteänderungen (das Herzstück)

Hier wird aus einer Figurendatenbank eine Geschichte, die sich entwickelt.

### Das Problem

Alice ist 27. Irgendwann hat sie Geburtstag. Danach ist sie 28. Du willst das
**nicht** in jedem Kapitel nachtragen.

### Die Lösung

Häng die Änderung an die Aktion:

```
┌─ Änderung ─────────────────────────────────────────┐
│  Element  [ Alice            ▼ ]                   │
│  Feld     [ age              ▼ ]                   │
│  vorher   [ 27 ]    nachher  [ 28 ]                │
└────────────────────────────────────────────────────┘
```

Ab jetzt gilt:

```
  Basiswert aus der Vorlage:  age = 27
                              │
   Tag 1 ──── Tag 5 ──── Tag 11 ──────── Tag 40 ──▶
                           │
                           ⚡ "Alice' Geburtstag"
                              age: 27 → 28
                           │
     27  27  27  27  27  │ 28  28  28  28  28
```

Und zwar **überall gleichzeitig**:

| Wo | Was du siehst |
|:--|:--|
| Im Manuskript vor Tag 11 | `@Alice.age` → 27 |
| Im Manuskript nach Tag 11 | `@Alice.age` → 28 |
| Im Details‑Fenster | `age  27`, darunter `• Tag 11, 08:00: 27 → 28` |
| Im Story Visualizer | `• Alice.age: 27 → 28` unter dem Ereignis |
| In der Word‑Datei | Die jeweils richtige Zahl, fest eingesetzt |

### Ein neues Feld direkt hier anlegen

Im Feld‑Aufklappmenü der Änderung kannst du einen Namen eintippen, den es noch nicht
gibt – das Feld wird dann sofort am Element angelegt. Du musst also nicht erst in die
Vorlage.

---

## 6.6 Aktions‑Typen

Zum Sortieren und Filtern. Mitgeliefert werden:

| Typ | Wofür |
|:--|:--|
| **Dialogue** | Ein Gespräch |
| **Action** | Eine Handlung |
| **Scene Change** | Szenenwechsel |
| **Discovery** | Eine Enthüllung |
| **Conflict** | Ein Konflikt |

Eigene Typen tippst du einfach in die Aufklappliste. Sie landen in `metadata.json` und
stehen ab dann überall zur Auswahl.

---

## 6.7 Text und Ereignis verknüpfen

Wenn eine Aktion schon existiert und du sie an einer Textstelle festmachen willst:

**Einfügen → Aktion verknüpfen** → suchen → anklicken.

Im Text steht dann `!act:…` – im Schreibmodus als farbige Marke sichtbar, im fertigen
Text unsichtbar. Sie sagt nur: *„Hier in der Erzählung passiert dieses Ereignis."*

---

## 6.8 Alles bleibt synchron

```
   Timeline                    Aktionen-Panel              Manuskript
   ────────                    ──────────────              ──────────
   Punkt von Tag 5      ───▶   Zeitspalte ändert    ───▶   Werte im Text
   nach Tag 6 ziehen           sich sofort                 rechnen neu
                                                           
   Aktion bearbeiten    ◀───   [ Bearbeiten ]              
   
   Doppelklick auf       ───▶  neue Zeile taucht    ───▶   Marke gesetzt
   freie Stelle                sofort auf
```

Es gibt keine „Aktualisieren"-Schaltfläche – alle Fenster zeigen immer denselben Stand.

---

## ✅ Kurz gesagt

- Eine **Aktion** = ein Ereignis zu einem Zeitpunkt, mit Beteiligten
- Der schnellste Weg: **Einfügen → Aktion aus diesem Absatz**
- **Änderungen** an einer Aktion verschieben Werte ab diesem Zeitpunkt – überall
- Typen, Stränge und Tags kannst du frei erweitern
- Aktionen‑Panel und Timeline zeigen dieselben Daten, nur anders

---

[<kbd> &nbsp; ← Übersicht &nbsp; </kbd>](../../README.md)
[<kbd> &nbsp; ← Zurück: 5 · Wie die Zeit funktioniert &nbsp; </kbd>](05-zeit.md)
[<kbd> &nbsp; Weiter: 7 · Die Timeline → &nbsp; </kbd>](07-timeline.md)
