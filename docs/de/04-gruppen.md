[<kbd> &nbsp; ← Übersicht &nbsp; </kbd>](../../README.md)
[<kbd> &nbsp; ← Zurück: 3 · Schreiben &nbsp; </kbd>](03-manuskript.md)
[<kbd> &nbsp; Weiter: 5 · Wie die Zeit funktioniert → &nbsp; </kbd>](05-zeit.md)

---

# 👥 Kapitel 4 · Figuren, Orte, Dinge

> **Worum geht's hier?** Um alles, was in deiner Geschichte *existiert*. Du legst selbst
> fest, welche Arten von Dingen es gibt und welche Eigenschaften sie haben.

---

## 4.1 Zwei Begriffe

```
   📁 GRUPPE                          📄 ELEMENT
   Eine Schublade.                    Ein einzelnes Ding darin.
   Kann Untergruppen haben.           Hat Felder mit Werten.

   Characters ────┬── Main ────┬── Alice     name: Alice
                  │            │             age:  27
                  │            └── Bob       status: Alive
                  │
                  └── NPCs ────── Guard

   Locations ─────┬── Castle
                  └── Forest

   Objects ─────────── Sword
```

> 🔑 **Wichtig:** Das Programm kennt keine festen Begriffe wie „Person" oder „Ort".
> `Characters`, `Locations` und `Objects` sind nur der Vorschlag beim Anlegen eines
> Projekts. Du kannst sie umbenennen, löschen und eigene anlegen: `Fraktionen`,
> `Zaubersprüche`, `Schiffe`, `Rezepte` – was deine Geschichte eben braucht.

---

## 4.2 Der Gruppen‑Manager

![Gruppen-Manager und Details](../bilder/details.png)

Links der Baum, rechts die Details des Ausgewählten.

| Oben in der Leiste | Wirkung |
|:--|:--|
| **+ Gruppe** | Neue Gruppe oder Untergruppe anlegen |
| **+ Element** | Neues Element in der gewählten Gruppe |
| **☑ Elemente** | Elemente im Baum ein‑/ausblenden (nur Gruppen sehen) |
| **Suchen…** | Filtert den Baum live |

**Rechtsklick** ist der Hauptweg zu allem anderen:

```
  Rechtsklick auf eine GRUPPE         Rechtsklick auf ein ELEMENT
  ┌────────────────────────────┐      ┌────────────────────────────┐
  │ Neues Element…             │      │ Bearbeiten…                │
  │ Neue Untergruppe…          │      │ Umbenennen…                │
  │ Template bearbeiten…       │      │ Duplizieren                │
  │ Gruppe bearbeiten (Name/   │      │ Verschieben nach…          │
  │   Farbe)…                  │      │ Als Aktion in Timeline     │
  │ Löschen                    │      │   eintragen                │
  └────────────────────────────┘      │ Datei im Explorer zeigen   │
                                      │ Löschen                    │
                                      └────────────────────────────┘
```

Elemente lassen sich auch per **Ziehen und Fallenlassen** in eine andere Gruppe schieben –
die `.md`‑Datei wandert dann im Vault mit.

---

## 4.3 Eine Gruppe anlegen

1. **+ Gruppe** klicken (oder <kbd>Strg</kbd>+<kbd>G</kbd>)
2. **Name** eingeben, z. B. `Fraktionen`
3. **Übergeordnete Gruppe** wählen – leer lassen für eine Hauptgruppe
4. Optional eine **Farbe**; sonst sucht das Programm selbst eine gut unterscheidbare aus

Die Farbe zieht sich durch das ganze Programm: Marken im Text, Punkte in der Timeline,
Kreise im Beziehungsgraph. Untergruppen erben eine leicht abgewandelte Fassung,
damit man die Verwandtschaft sieht.

---

## 4.4 Ein Element anlegen

1. Gruppe im Baum anklicken
2. **+ Element** (oder <kbd>Strg</kbd>+<kbd>N</kbd>)
3. Der Dialog zeigt **alle Felder der Vorlage** dieser Gruppe
4. Pflichtfelder (mit `*` markiert) ausfüllen, der Rest ist optional
5. Speichern → es entsteht `Gruppe/Name.md` im Vault

---

## 4.5 Vorlagen: welche Felder hat eine Gruppe?

Eine **Vorlage** (Template) beschreibt, welche Felder jedes Element einer Gruppe hat –
wie ein Formular.

**Rechtsklick auf die Gruppe → Template bearbeiten…**

```
┌─ Template: Characters ─────────────────────────────────────┐
│                                                            │
│  Geerbte Felder:                     (von der Elterngruppe)│
│    (keine)                                                 │
│                                                            │
│  Eigene Felder:                                            │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ name         Text      [Pflicht]     Bearb.  ✕       │  │
│  │ age          Integer   [Pflicht]     Bearb.  ✕       │  │
│  │ description  Text                    Bearb.  ✕       │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                            │
│  [ + Neues Feld ]                                          │
│                                     [ Speichern ] [ Abbr. ]│
└────────────────────────────────────────────────────────────┘
```

### Ein Feld hinzufügen

**+ Neues Feld** öffnet einen kleinen Dialog:

| Eingabe | Bedeutung |
|:--|:--|
| **Feldname** | Wie es heißt, z. B. `geburtstag`. Auch der Name, den du im Text als `@Alice.geburtstag` benutzt |
| **Typ** | Welche Art von Wert (siehe unten) |
| **Pflichtfeld** | Muss beim Anlegen ausgefüllt werden |
| **Standardwert** | Womit neue Elemente starten |
| **Beschreibung** | Erscheint als Hilfetext neben dem Feld |

> ⚙️ **Was danach passiert:** Das Programm geht **alle vorhandenen Elemente** dieser
> Gruppe durch und ergänzt das neue Feld mit dem Standardwert. Bestehende Werte bleiben
> unangetastet. Alle betroffenen `.md`‑Dateien werden neu geschrieben.

---

## 4.6 Die neun Feldtypen

| Typ | Wofür | Eingabe im Programm | Beispiel |
|:--|:--|:--|:--|
| 🔤 **Text** | Beliebiger Text, auch mehrzeilig | Textfeld | `Eine mutige Ritterin.` |
| 🔢 **Integer** | Ganze Zahl | Zahlenfeld mit − / + | `27` |
| 🔣 **Float** | Kommazahl | Zahlenfeld | `1.78` |
| 📅 **Date** | Zeitpunkt in der Geschichte | Textfeld mit Prüfung | `Tag 15, 08:00` |
| 📋 **Enum** | Auswahl aus festen Möglichkeiten | Aufklappliste (neue Optionen frei ergänzbar) | `Alive` / `Dead` / `Missing` |
| ☑️ **Boolean** | Ja oder Nein | Häkchen | `true` |
| 📝 **List** | Mehrere Werte | Liste mit + / − | `["Schwertkampf", "Magie"]` |
| 🔗 **Reference** | Verweis auf ein anderes Element | Suchfeld mit Vorschlägen | `@Characters/Main/Bob` |
| 📎 **File** | Bild oder Datei | Dateiauswahl, Vorschau | `Assets/Images/alice.png` |

---

## 4.7 Vererbung: Untergruppen bekommen mehr

Eine Untergruppe erbt alle Felder ihrer Elterngruppe und kann eigene dazulegen.

```
  📁 Characters                    name, age, description
        │                                   │
        │                                   │ erben
        ▼                                   ▼
  📁 Main                          name, age, description
                                 + backstory
                                 + status

  → Alice (in Main) hat 5 Felder
  → Guard (in NPCs, ohne eigene Felder) hat 3
```

Im Template‑Dialog stehen geerbte Felder oben, klar getrennt und nicht löschbar –
geändert werden sie in der Gruppe, die sie definiert.

---

## 4.8 Ein Feld nur für eine einzelne Figur

Manchmal braucht genau eine Figur etwas Besonderes, ohne dass alle anderen es
mitbekommen sollen.

Im Details‑Fenster unten: **+ Eigenes Feld**. Das Feld gilt dann nur für dieses eine
Element und ist mit **(nur hier)** gekennzeichnet.

---

## 4.9 Löschen – und was dabei passiert

Das Programm prüft vorher, ob etwas kaputtgehen würde, und zeigt es dir:

```
┌─ Element löschen ──────────────────────────────────┐
│                                                    │
│  "Alice" wirklich löschen?                         │
│                                                    │
│  Folgende Verweise werden ungültig:                │
│   • Aktion #1  "Alice kehrt zurück"                │
│   • Aktion #5  "Alice' Geburtstag"                 │
│   • Verbindung "married_to" mit Bob                │
│   • 6 Stellen im Manuskript                        │
│                                                    │
│                       [ Löschen ]  [ Abbrechen ]   │
└────────────────────────────────────────────────────┘
```

Beim Löschen einer **Gruppe** steht dabei, wie viele Elemente und Untergruppen
mitgelöscht würden.

---

## ✅ Kurz gesagt

- **Gruppe** = Schublade, **Element** = einzelnes Ding darin
- Es gibt keine eingebauten Begriffe – du legst alle Gruppen selbst an
- Eine **Vorlage** bestimmt die Felder einer Gruppe; Untergruppen erben und ergänzen
- Neun Feldtypen, von Text bis Datei
- Rechtsklick im Baum ist der Weg zu allem
- Löschen zeigt vorher, welche Verweise ungültig werden

---

[<kbd> &nbsp; ← Übersicht &nbsp; </kbd>](../../README.md)
[<kbd> &nbsp; ← Zurück: 3 · Schreiben &nbsp; </kbd>](03-manuskript.md)
[<kbd> &nbsp; Weiter: 5 · Wie die Zeit funktioniert → &nbsp; </kbd>](05-zeit.md)
