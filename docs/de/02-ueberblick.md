[<kbd> &nbsp; ← Übersicht &nbsp; </kbd>](../../README.md)
[<kbd> &nbsp; ← Zurück: 1 · Loslegen &nbsp; </kbd>](01-loslegen.md)
[<kbd> &nbsp; Weiter: 3 · Schreiben → &nbsp; </kbd>](03-manuskript.md)

---

# 🪟 Kapitel 2 · Das Programmfenster

> **Worum geht's hier?** Welches Fenster wofür da ist, wie du sie herumschiebst und
> wie du ein versehentlich geschlossenes Fenster zurückholst.

---

## 2.1 Der erste Eindruck

![Das Programmfenster](../bilder/ueberblick.png)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ Datei  Bearbeiten  Ansicht  Fenster  Hilfe        Demo Story | C:/…/Vault  │ ← Menüleiste
├──────────────┬──────────────────────────────────────────┬───────────────────┤
│              │  Manuskript │ Timeline                   │                   │
│  Gruppen-    │ ┌──────────────────────────────────────┐ │    Details        │
│  Manager     │ │                                      │ │                   │
│              │ │   hier wird geschrieben              │ │  alles über das   │
│  deine       │ │                                      │ │  gerade           │
│  Figuren,    │ │                                      │ │  angeklickte      │
│  Orte,       │ │                                      │ │  Element          │
│  Dinge       │ └──────────────────────────────────────┘ │                   │
│              ├──────────────────────────────────────────┤                   │
│              │  Aktionen │ Connections                  │                   │
│              │                                          │                   │
│              │   Ereignisliste bzw. Beziehungsnetz      │                   │
└──────────────┴──────────────────────────────────────────┴───────────────────┘
```

Fünf Fenster sind am Anfang offen. Vier weitere kannst du dazuholen.

---

## 2.2 Alle Fenster im Überblick

| Fenster | Wofür | Kapitel |
|:--|:--|:--|
| ✍️ **Manuskript** | Hier schreibst du die Geschichte. Das wichtigste Fenster. | [3](03-manuskript.md) |
| 👥 **Gruppen‑Manager** | Der Baum links: alle Figuren, Orte und Dinge, nach Gruppen sortiert. | [4](04-gruppen.md) |
| 🔍 **Details** | Zeigt alles über das gerade ausgewählte Element, Ereignis oder die Verbindung. Hier bearbeitest du Felder. | [9](09-chronik.md) |
| 📋 **Aktionen‑Panel** | Alle Ereignisse als sortierbare, filterbare Tabelle. | [6](06-aktionen.md) |
| 📅 **Timeline** | Dieselben Ereignisse als Punkte auf einer Zeitachse, mit einer Spur je Element. | [7](07-timeline.md) |
| 🔗 **Connections** | Das Beziehungsnetz als Graph mit Kreisen und Linien. | [8](08-connections.md) |
| 📜 **Story Visualizer** | Liest die Geschichte als Chronik vor: „Tag 1 … 14 Tage vergehen … Tag 15 …". Nur lesen. | [9](09-chronik.md) |
| 💾 **Dateimanager** | Bilder und andere Dateien in den Vault holen und mit Elementen verknüpfen. | [10](10-dateien.md) |
| ⚙️ **Einstellungen** | Sprache, Design, Farben, eigene Listen. | [11](11-einstellungen.md) |

---

## 2.3 Ein Fenster öffnen oder schließen

Jedes Fenster hat oben rechts ein **✕**. Weg ist es damit aber nicht – du holst es
jederzeit über das Menü **Fenster** zurück:

![Das Fenster-Menü](../bilder/fenstermenue.png)

Ein Haken ✓ heißt: das Fenster ist gerade offen. Klick darauf schaltet es um.

> 💡 Alles zu, nichts mehr zu finden? **Ansicht → Layout zurücksetzen** stellt die
> Anfangsanordnung wieder her.

---

## 2.4 Fenster verschieben (Andocken)

Das Programm benutzt dasselbe Prinzip wie Blender: Fenster kleben aneinander, lassen sich
aber beliebig umsortieren.

```
 Schritt 1: Am Titel anfassen        Schritt 2: Über eine Kante ziehen
 ┌──────────────┐                    ┌──────────────┐
 │ Timeline  ✕  │ ← hier ziehen      │              │    ╔═══════════╗
 ├──────────────┤                    │  Manuskript  │    ║ blau      ║
 │              │                    │              │ ←  ║ markiert  ║
 │              │                    │              │    ║ das Ziel  ║
 └──────────────┘                    └──────────────┘    ╚═══════════╝

 Schritt 3: Loslassen
 ┌──────────────┬──────────────┐     Oder: genau auf die Mitte ziehen,
 │  Manuskript  │   Timeline   │     dann werden beide zu Tabs in
 │              │              │     einem gemeinsamen Fenster.
 └──────────────┴──────────────┘
```

| Was du willst | Wie |
|:--|:--|
| Fenster nebeneinander | Titel packen, an den **Rand** eines anderen Fensters ziehen |
| Fenster als Tab dazu | Titel packen, auf die **Mitte** eines anderen Fensters ziehen |
| Fenster frei schweben lassen | Titel packen, **auf den Desktop** ziehen – es wird ein eigenes Windows‑Fenster |
| Größe ändern | Die Trennlinie zwischen zwei Fenstern ziehen |

Deine Anordnung wird gespeichert (`%APPDATA%/StoryEditor/imgui_layout_v3.ini`) und ist
beim nächsten Start wieder da.

---

## 2.5 Die Menüleiste

| Menü | Was drin steckt |
|:--|:--|
| **Datei** | Neues Projekt, Projekt öffnen, Speichern, Vault im Explorer zeigen, Manuskript als Word, Beenden |
| **Bearbeiten** | Rückgängig / Wiederholen, Neues Element, Neue Gruppe, Neue Aktion |
| **Ansicht** | Design (Hell/Dunkel), Sprache, Einstellungen, Layout zurücksetzen, Automatisch speichern |
| **Fenster** | Fenster ein‑ und ausblenden (siehe oben) |
| **Hilfe** | Tastenkürzel, Über Story Editor |

Rechts oben in der Leiste steht immer, **welches Projekt** gerade offen ist und **wo**
es liegt.

---

## 2.6 Auswählen: alles hängt zusammen

Ein Klick auf irgendetwas wählt es aus – und alle anderen Fenster ziehen mit.

```
   Klick auf "Alice"                Klick auf einen Punkt
   im Gruppen-Manager               in der Timeline
          │                                 │
          ▼                                 ▼
   ┌─────────────────────────────────────────────────┐
   │  Details zeigt Alice          Details zeigt     │
   │  Timeline springt zu ihr      das Ereignis      │
   │  Aktionen filtert auf sie     Aktionen scrollt  │
   └─────────────────────────────────────────────────┘
```

Dasselbe gilt im Text: ein Klick auf eine farbige Marke im Manuskript öffnet das
zugehörige Element in den Details.

---

## ✅ Kurz gesagt

- **Manuskript** ist das Hauptfenster, **Details** zeigt immer das gerade Ausgewählte
- Geschlossene Fenster holst du über **Fenster** zurück, verhauene Anordnung über **Ansicht → Layout zurücksetzen**
- Fenster ziehst du am **Titel** an Ränder (nebeneinander) oder in die Mitte (als Tab)
- Eine Auswahl gilt **überall gleichzeitig**

---

[<kbd> &nbsp; ← Übersicht &nbsp; </kbd>](../../README.md)
[<kbd> &nbsp; ← Zurück: 1 · Loslegen &nbsp; </kbd>](01-loslegen.md)
[<kbd> &nbsp; Weiter: 3 · Schreiben → &nbsp; </kbd>](03-manuskript.md)
