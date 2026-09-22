[<kbd> &nbsp; ← Übersicht &nbsp; </kbd>](../../README.md)
[<kbd> &nbsp; ← Zurück: 7 · Die Timeline &nbsp; </kbd>](07-timeline.md)
[<kbd> &nbsp; Weiter: 9 · Chronik & Detailpanel → &nbsp; </kbd>](09-chronik.md)

---

# 🔗 Kapitel 8 · Beziehungen

> **Worum geht's hier?** Um alles, was eine Weile **gilt**: verheiratet sein, an einem
> Ort sein, etwas besitzen, jemanden hassen. Und darum, dass du die Arten von
> Beziehungen selbst erfindest.

---

## 8.1 Aktion oder Verbindung?

```
   ⚡ AKTION                          🔗 VERBINDUNG
   Ein Moment.                       Ein Zustand.

   "Alice und Bob heiraten"          "Alice ist mit Bob verheiratet"
   Tag 12, 11:00                     ab Tag 12, kein Ende

   "Alice betritt die Burg"          "Alice ist an Ort: Burg"
   Tag 1, 09:00                      Tag 1 bis Tag 4

   ──●──────────────────▶            ──▓▓▓▓▓▓▓▓▓▓▓▓▓▓──────▶
     ein Punkt                         ein Zeitraum
```

Oft braucht man beides: das Ereignis *und* den Zustand, den es auslöst.

---

## 8.2 Der Graph

![Das Connections-Fenster](../bilder/connections.png)

| | |
|:--|:--|
| ⭕ **Kreis** (Knoten) | Ein Element. Farbe = Gruppenfarbe |
| ➖ **Linie** (Kante) | Eine Verbindung, beschriftet mit ihrem Typ |
| ➡️ **Pfeil** | Zeigt von der ersten zur zweiten Rolle |
| **Text unter dem Kreis** | Was für dieses Element **gerade** gilt, z. B. `ist an Ort: Castle` |

| Maus | Wirkung |
|:--|:--|
| **Ziehen** | Knoten verschieben (die Position wird gespeichert) |
| **Klick** | Auswählen → Details‑Fenster |
| <kbd>Strg</kbd>+**Klick** | Mehrere auswählen |
| **Rad** | Zoomen |
| **Rechtsklick auf eine Linie** | Bearbeiten / Löschen |

Die Leiste oben:

| Knopf | Wirkung |
|:--|:--|
| **Verbindung erstellen** | Aus den gerade ausgewählten Knoten |
| **Block erstellen** | Mehrere Verbindungen zu einer Gruppe zusammenfassen |
| **Auswahl leeren** | |
| **Typen verwalten…** | Eigene Beziehungsarten anlegen (siehe unten) |
| **Auto‑Layout** | Knoten automatisch verteilen |
| **Typ‑Filter** | Nur bestimmte Beziehungsarten zeigen |
| **Fokus** | Nur ein Element und seine Nachbarn zeigen |

---

## 8.3 Eigene Beziehungsarten mit Rollen

Das ist der Teil, der das Programm von einem starren Figurenverwalter unterscheidet.

**Typen verwalten… → Neuer Typ**

```
┌─ Verbindungstyp ──────────────────────────────────────────┐
│                                                           │
│  Name:      [ Person an Ort                             ] │
│                                                           │
│  Rolle 1:   [ Person ]  →  Gruppe [ Characters      ▼ ]   │
│  Rolle 2:   [ Ort    ]  →  Gruppe [ Locations       ▼ ]   │
│             [ + Rolle hinzufügen ]                        │
│                                                           │
│  ☑ Über die Timeline setzbar (zeitlich)                   │
│     ☑ pro Element nur eine gleichzeitig                    │
│     ☑ Band in der Timeline zeichnen                        │
│       Band läuft in der Spur von: [ Person ▼ ]            │
│       Beschriftet mit:            [ Ort    ▼ ]            │
│                                                           │
│                              [ Speichern ] [ Abbrechen ]  │
└───────────────────────────────────────────────────────────┘
```

| Einstellung | Was sie bewirkt |
|:--|:--|
| **Rolle + Gruppe** | Beim Setzen werden nur Elemente aus dieser Gruppe angeboten. „Alle Gruppen" geht auch |
| **Beliebig viele Rollen** | Eine Verbindung darf drei oder mehr Beteiligte haben |
| **Zeitlich** | Die Verbindung bekommt ein „Ab" und optional ein „Bis" |
| **Exklusiv** | Sobald eine neue beginnt, endet die vorherige automatisch |
| **Band zeichnen** | Erscheint als farbiger Balken in der Timeline |

### Warum „exklusiv" so nützlich ist

Eine Figur kann nur an einem Ort gleichzeitig sein. Mit *exklusiv* musst du nie ein
Ende eintragen:

```
  Du trägst ein:    Alice ist an Ort: Burg     ab Tag 1
  Du trägst ein:    Alice ist an Ort: Wald     ab Tag 5
                              │
                              ▼  das Programm macht automatisch
  Alice ist an Ort: Burg      Tag 1  bis Tag 5
  Alice ist an Ort: Wald      ab Tag 5
```

Und das Beste: Das Programm kennt dabei nie das Wort „Ort". Du hast das Konzept
selbst gebaut.

---

## 8.4 Eine Verbindung setzen

**Weg 1 – im Graphen**

1. Ersten Knoten anklicken, mit <kbd>Strg</kbd> den zweiten dazu
2. **Verbindung erstellen**
3. Typ auswählen, Rollen zuordnen, optional Ab/Bis eintragen

**Weg 2 – in der Timeline**

Rechtsklick in einer Spur → **Verbindung setzen**. Der Zeitpunkt kommt aus der Stelle,
an der du geklickt hast.

**Weg 3 – im Details‑Fenster**

Bei jedem Element gibt es den Abschnitt **Beziehungen** mit **+ Verbindung**.

---

## 8.5 Wo Verbindungen überall auftauchen

| Ort | Darstellung |
|:--|:--|
| 📅 **Timeline** | Farbiges Band hinter der Spur: `▓ Castle ▓│▓ Forest ▓│▓ Castle →` |
| 🔗 **Graph** | Linie mit Beschriftung; unter jedem Kreis der aktuelle Zustand |
| 🔍 **Details** | Rollen, Zeitraum, Beteiligte – bearbeitbar |
| 📄 **Element‑Datei** | Abschnitt `## Beziehungen` mit `married_to -> [[Bob]]` |

### Der Zeit‑Schieber

Unter dem Graphen liegt **Zeitleiste** (aufklappbar). Damit fährst du durch die
Geschichte und siehst, welche Beziehungen zu einem gewählten Tag gerade gelten.

---

## 8.6 Blöcke

Ein **Block** fasst mehrere Verbindungen zu einem Thema zusammen:

```
  ┌─ Block: Dreiecksgeschichte ─────────────┐
  │                                         │
  │    Alice ──── verheiratet ──── Bob      │
  │      ▲                          ▲       │
  │      │ liebt                    │ liebt │
  │      └────────── Carol ─────────┘       │
  │                                         │
  └─────────────────────────────────────────┘
```

Knoten auswählen → **Block erstellen** → Name vergeben. Im Graphen lässt sich ein
Block zusammenklappen, damit das Netz übersichtlich bleibt.

---

## 8.7 Ältere Projekte

Projekte aus einer früheren Programmversion laden ohne Zutun weiter: Aus dem alten
Textfeld `type` entsteht ein Typ mit zwei Rollen, `source`/`target` werden zu diesen
Rollen, und Datumsfelder werden geparst.

---

## ✅ Kurz gesagt

- **Aktion** = Moment, **Verbindung** = Zustand über einen Zeitraum
- Du erfindest die Beziehungsarten selbst – mit beliebig vielen **Rollen**
- **Zeitlich** gibt ihnen Ab/Bis, **exklusiv** beendet die vorherige automatisch
- Sichtbar im Graphen, als **Band** in der Timeline und in den Details
- **Blöcke** fassen zusammengehörende Verbindungen zusammen

---

[<kbd> &nbsp; ← Übersicht &nbsp; </kbd>](../../README.md)
[<kbd> &nbsp; ← Zurück: 7 · Die Timeline &nbsp; </kbd>](07-timeline.md)
[<kbd> &nbsp; Weiter: 9 · Chronik & Detailpanel → &nbsp; </kbd>](09-chronik.md)
