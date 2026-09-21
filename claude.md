# Story Editor – Detaillierte Anforderungsspezifikation

## 1. Zusammenfassung & Kernziel

Der Story Editor ist eine umfassende Schreib-Management-Software für komplexe Geschichten (Romane, Theater, Spiele, etc.). Das Kernproblem, das gelöst werden soll: Bei großen Geschichtsprojekten verliert der Autor mit der Zeit den Überblick über:

- Wo bin ich zeitlich in der Geschichte?
- Was habe ich bereits geschrieben und wo?
- Welche Charaktere sind zu welchem Zeitpunkt an welchem Ort?
- Was weiß Charakter A über Charakter B (und umgekehrt)?
- Welche Interaktionen haben bereits stattgefunden?
- Wie haben sich Charaktere/Objekte über die Zeit verändert?
- Welche Handlungsstränge existieren parallel oder sequenziell?

Die Software soll all diese Informationen visuell und logisch organisiert darstellen, sodass der Autor jederzeit Inkonsistenzen, Lücken oder logische Fehler erkennt.

---

## 2. Technische Grundlagen

### 2.1 Programmiersprache & GUI-Framework
- **Sprache:** C++17 oder neuer
- **GUI-Framework:** ImGui (immediate mode GUI)
  - Begründung: Ermöglicht schnelle Prototypisierung, flexible Fensterarchitektur, gute Kontrolle über Layouts
  - Die gesamte UI wird über ImGui realisiert, nicht über externe UI-Builder

### 2.2 Datenspeicherung & Persistenz
- **Speicherort:** Obsidian Vault (Standard Obsidian-Verzeichnisstruktur)
- **Dateiformat:** Markdown (.md) für Texte, Ordnerstruktur für Hierarchie
- **Warum Obsidian?** 
  - Versionskontrolle mit Git möglich
  - Portabel und standardisiert
  - Dateien sind lesbar und human-editable (für Debugging/Backups)

**🔴 KRITISCH: MD-Dateien dürfen NUR vom Programm bearbeitet werden!**

- Der Benutzer darf **NIEMALS direkt in .md-Dateien schreiben**
- Der Benutzer bearbeitet **AUSSCHLIESSLICH über die UI**
- Das Programm ist der alleinige Schreiber für alle .md-Dateien
- **Grund:** Die interne Datenstruktur ist präzise organisiert. Direkte Bearbeitung könnte das Format beschädigen und Konsistenz-Probleme verursachen.

**Datenschutz-Mechanismus bei externen Änderungen:**
Wenn ein Benutzer versucht, eine .md-Datei extern zu bearbeiten (z.B. mit Obsidian, VSCode, etc.):
1. Das Programm erkennt die externe Änderung (File-Watcher)
2. Ein Warning-Dialog wird angezeigt:
   ```
   ⚠️ Die Datei "Alice.md" wurde extern modifiziert!
   Die Änderung kann inkonsistent mit den Programm-Daten sein.
   
   [Reload]   [Merge]   [Cancel]
   ```
3. Optionen:
   - **Reload:** Verwerfe lokale Änderungen, lade externe Datei
   - **Merge:** Versuche, externe Änderungen mit lokalen zu kombinieren
   - **Cancel:** Ignoriere externe Änderung, behalte interne Daten

**Dateiorganisation in Obsidian:**
```
MyStory/
├── Characters/
│   ├── Main/
│   │   ├── Alice.md
│   │   ├── Bob.md
│   ├── NPCs/
│   │   ├── Guard.md
├── Locations/
│   ├── Castle.md
│   ├── Forest.md
├── Objects/
│   ├── Sword.md
├── Timeline/
│   └── events.md (oder Struktur mit Unterordnern)
├── Metadata/
│   └── project.json (Projekteinstellungen, benutzerdefinierte Gruppen, Templates)
```

### 2.3 Fensterarchitektur & Layout-System

**Prinzip:** Das Programm folgt dem Blender-Ansatz für Fenster-Management:
- **Persistente Fenster:** Sind immer sichtbar und können nicht geschlossen werden (z.B. die Hauptansicht oder Timeline)
- **Angedockte Fenster:** Sind in Paneln organisiert, können aber zwischen Panels verschoben werden
- **Schwebende Fenster:** Können frei über den Bildschirm bewegt und skaliert werden
- **Kontextfenster:** Popup-Fenster, die spezifische Aktionen ermöglichen

**Top-Menubar:**
- Enthält eine Menüleiste mit Optionen: File, View, Windows, Help
- Ein Fenster-Menü, das alle verfügbaren Fenster auflistet und deren Öffnen/Schließen ermöglicht
- Toggle für verschiedene Panels (z.B. „Timeline anzeigen", „Gruppen anzeigen")

**Farbvariablen-System (KRITISCH):**
```cpp
// Beispiel-Struktur (nicht hardcoded!)
struct ColorScheme {
    ImVec4 textPrimary;
    ImVec4 textSecondary;
    ImVec4 backgroundColor;
    ImVec4 panelBackground;
    ImVec4 accentColor;
    ImVec4 warningColor;
    ImVec4 successColor;
    std::map<std::string, ImVec4> groupColors; // Pro Gruppe eine Farbe
};
```
- Alle Farben müssen konfigurierbar sein (ideale über ein Konfigurationsfile)
- Keine hartcodierten RGB/HEX-Werte im Code
- Farben sollten über ein Einstellungs-UI änderbar sein

---

## 3. Fenstermodule – Detaillierte Spezifikation

### 3.1 Timeline-Fenster (KRITISCHES MODUL)

Die Timeline ist die Hauptnavigations- und Strukturierungsmethode der Software.

#### 3.1.1 Grundkonzept

Die Timeline ist eine **horizontale Linie mit zeitlichen Markierungen**, auf der **Ereignisse als Punkte** eingetragen werden. Vertikal sind Spuren für jede Gruppe/Untergruppe organisiert (ähnlich wie eine DAW wie Ableton oder Blender's Video Sequence Editor).

**Spur-Aufbau:**
- Jede Hauptgruppe bekommt eine Spur (Zeile)
- Untergruppen sind in Baumstruktur einklappbar
- Jedes Element (Charakter, Ort, etc.) kann eigene Unterspuren haben oder ihre Aktionen in der Gruppen-Spur angezeigt werden

**Beispiel Timeline-Layout:**
```
Zeit:        0h        8h       16h      24h      Week2    Week3
─────────────────────────────────────────────────────────────────
Characters ▼
  Main ▼
    Alice  ●──────●────────────────────●
    Bob           ●────●
  NPCs ▼
    Guard                    ●──────●
Locations ▼
  Castle         ●────────────────────────●
  Forest ●
Objects ▼
  Sword                ●
```

#### 3.1.2 Zeitskalierung & Dynamische Anpassung

**Problem:** Wenn Ereignisse unregelmäßig verteilt sind, wird die Timeline unbrauchbar. Lösung: Intelligente, nicht-lineare Skalierung.

**Skalierungs-Algorithmus:**
1. Benutzer wählt Zeiteinheit: Minute, Stunde, Tag, Woche, Monat, Jahr
2. System analysiert alle Ereignisse und ihre zeitlichen Abstände
3. Ereignisse werden in **Cluster** eingeteilt:
   - **Dichter Cluster:** Viele Ereignisse dicht beieinander (z.B. 5 Ereignisse in 2 Stunden) → Diese Bereiche werden vergrößert dargestellt
   - **Sparsamer Cluster:** Lange Zeiträume ohne Ereignisse (z.B. 1 Jahr Pause) → Diese Bereiche werden komprimiert

**Beispiel mit Minuten-Ansicht:**
```
Dichter Bereich (5 Minuten, 4 Ereignisse):
●───●───●───●───●  (jeder ● ist 1 Minute)

Sparsamer Bereich (50 Minuten Pause, 1 Ereignis):
●────...────●  (die "..." stellt 50 Minuten dar, aber nimmt nur 3 Zeichen Platz)
```

**Implementierung:**
- Das System berechnet automatisch die Breite jedes Zeitsegments basierend auf der Ereignisdichte
- Der Benutzer kann manuell zwischen vordefinierten Zeitskalierungen wechseln (Minute, Stunde, Tag, etc.)
- Es gibt einen Zoom-Schieberegler für feinere Kontrolle

#### 3.1.3 Ereignis-Punkte (Event Markers)

Jedes Ereignis wird als ein **visueller Punkt** dargestellt:

**Visuell:**
- Kleiner Kreis oder Marker (ca. 8-12px Durchmesser)
- Die Farbe folgt der Farbe der entsprechenden Gruppe
- Hover-Effekt: Marker wird größer oder leuchtet auf
- Click-Effekt: Marker wird ausgewählt (deutlicher Highlight)

**Hover-Verhalten:**
- Beim Hovern über einen Event-Marker erscheint ein **Tooltip** mit:
  - Titel der Aktion
  - Kurze Beschreibung (erste 100 Zeichen)
  - Zeitstempel (z.B. "Tag 5, 14:30 Uhr")
  - Beteiligte Elemente (z.B. "Alice, Schwert")

**Click-Verhalten:**
- Einfach-Klick: 
  - Der Event wird in einem Detail-Panel ausgewählt und angezeigt
  - Das Fenster scrollt zum entsprechenden Element (z.B. zur Alice-Datei oder zur Aktion-Liste)
  - Der Event erhält visuellen Highlight (z.B. weißer Glüh-Outline)
  
- Rechts-Klick:
  - Kontextmenü mit Optionen: Edit, Duplicate, Delete, Move to...

**Drag & Drop:**
- Event-Marker können in der Timeline verschoben werden
  - Horizontales Verschieben = Zeitpunkt ändern
  - Vertikales Verschieben = zu anderer Spur verschieben (optional)
- Beim Verschieben erscheint eine **Ghost-Ansicht** des Events an der neuen Position
- Beim Ablegen wird die Änderung live in die Obsidian-Dateien geschrieben

#### 3.1.4 Spur-Verwaltung & Hierarchie

**Spur-Elemente:**
- Jede Hauptgruppe hat eine **Spur** (eine Zeile mit Titel)
- Links neben dem Spurtitel: Ein **Toggle-Icon (▼/►)** zum Aus- und Einklappen
- Beim Einklappen verschwinden alle Untergruppen dieser Spur aus der Ansicht
- Untergruppen erben die Einrückung (Indentation) ihrer Elterngruppe

**Beispiel mit Einrückungen:**
```
▼ Characters
  ▼ Main
    ● Alice Spur        [Ereignisse von Alice]
    ● Bob Spur          [Ereignisse von Bob]
  ► NPCs                [eingeklappt, keine Spur sichtbar]
▼ Locations
  ● Castle Spur         [Ereignisse im Castle]
```

**Spur-Breite:**
- Der Spurtitel (links) hat eine feste Breite (z.B. 200px), kann aber manuell resizable sein
- Die Ereignisse werden rechts vom Titel auf der **Timeline-Fläche** angezeigt

#### 3.1.5 Filter & Ansichtsmodi

Der Benutzer kann zwischen verschiedenen **Filtermodi** wechseln, die bestimmen, welche Spuren sichtbar sind und welche Ereignisse angezeigt werden:

**Verfügbare Filter-Modi:**
1. **Nach Gruppen filtern**
   - Dropdown-List aller Gruppen
   - Benutzer wählt: „Characters" → nur Character-Events werden angezeigt
   - Mehrfach-Selektion möglich: Ctrl+Click zum Hinzufügen/Entfernen

2. **Nach spezifischen Elementen filtern**
   - Benutzer kann einzelne Charaktere/Orte/Objekte auswählen
   - Nur Events, die dieses Element betreffen, werden angezeigt
   - Suchfeld zum Suchen nach Element-Namen

3. **Nach Zeit filtern**
   - Start- und End-Datum eingeben
   - Timeline zeigt nur Events in diesem Zeitfenster

4. **Nach Event-Typ filtern** (optional)
   - Nur bestimmte Arten von Events anzeigen (z.B. nur Dialoge, nur Schauplatzwechsel, etc.)

5. **Keine Filter (Standard)**
   - Zeige alle Events

**Filter-UI:**
- Eine Filterleiste oberhalb oder unterhalb der Timeline mit verschiedenen Dropdown-Menüs
- Eine Schaltfläche „Filter zurücksetzen"
- Visuelles Feedback: Aktive Filter werden farblich hervorgehoben

#### 3.1.6 Spur-Eigenschaften & Darstellung

**Farbe der Spuren:**
- Jede Gruppe hat eine Farb-Zuordnung
- Diese Farbe wird:
  - Beim Spur-Titel angezeigt (z.B. ein kleines Farbquadrat neben dem Namen)
  - Bei allen Event-Markern dieser Spur verwendet
  - Konsistent über die gesamte Software (auch in Gruppen-Manager, Connections, etc.)

**Spur-Höhe:**
- Jede Spur hat eine minimale Höhe (z.B. 30px)
- Wenn viele Events in einer Spur sind, kann der Benutzer die Spur höher machen (draggable Spur-Grenze)
- Beim Vergrößern werden Events nebeneinander oder gestaffelt angezeigt (je nach Dichte)

#### 3.1.7 Event-Details anzeigen

**Info-Panel (rechts oder als eigenes Fenster):**
Wenn der Benutzer auf einen Event klickt, wird ein Detail-Panel angezeigt mit:
- Titel des Events
- Vollständige Beschreibung
- Zeitstempel (Datum + Uhrzeit + Beziehung zu anderen Events, z.B. „3 Tage nach Event XY")
- Beteiligte Elemente (als Clickable Links):
  - „Character: Alice (Link zur Alice-Datei)"
  - „Location: Castle (Link zur Castle-Datei)"
  - „Objects: Sword, Shield"
- Tags/Labels (wenn vorhanden)
- Edit-Button: Öffnet einen Editor für diesen Event
- Attached Files/Images (falls vorhanden)

#### 3.1.8 Event erstellen & bearbeiten

**Event erstellen:**
1. Benutzer klickt auf einen leeren Platz in einer Spur auf der Timeline
2. Ein **Dialog-Fenster** öffnet sich mit Feldern für:
   - Titel (Text)
   - Beschreibung (Rich Text Editor)
   - Zeitpunkt (Datum + Uhrzeit, oder relativ zu anderem Event)
   - Beteiligte Elemente (Suchfeld mit Multi-Select)
   - Bilder/Anhänge hinzufügen (File-Dialog oder Drag-Drop)
3. Benutzer bestätigt → Event wird erstellt und sofort in der Timeline angezeigt

**Event bearbeiten:**
1. Event in Timeline auswählen
2. Im Detail-Panel auf „Edit" klicken (oder Doppel-Klick auf Event)
3. Der selbe Dialog-Fenster öffnet sich mit den aktuellen Werten
4. Benutzer ändert die Werte
5. Speichern → Änderung wird in der Timeline und in Obsidian aktualisiert

#### 3.1.9 Zeitschritte & Versatz-Berechnung

Die Timeline muss mit verschiedenen Zeitkonzepten umgehen:

**Absolute Zeit:**
- Der Benutzer gibt einen festen Zeitpunkt an: z.B. „Tag 5, 14:30 Uhr"

**Relative Zeit:**
- Der Benutzer bezieht sich auf ein anderes Event: z.B. „2 Stunden nach Alice kommt an"
- Das System berechnet automatisch den absoluten Zeitpunkt

**Implementierung:**
- Jeder Event speichert intern einen Timestamp
- Das System kann Events nach Timestamp sortieren, auch wenn der Benutzer relativ eingegeben hat

---

### 3.2 Gruppen-Manager (Visueller Hierarchie-Manager)

Der Gruppen-Manager ist die zentrale Verwaltungsstelle für alle benutzerdefinierten Kategorien und deren Inhalte.

#### 3.2.1 Zweck & Übersicht

Der Benutzer definiert benutzerdefinierte Gruppen, z.B.:
- **Characters** (mit Untergruppen: Main, NPCs, Antagonists)
- **Locations** (Cities, Buildings, Dungeons)
- **Objects** (Weapons, Artifacts, Potions)
- **Factions** (Custom Group)
- etc.

Jede Gruppe ist eine **Hierarchie** mit beliebig vielen Ebenen. Der Manager zeigt diese Hierarchie visuell und erlaubt:
- Gruppen erstellen/löschen
- Elemente hinzufügen/bearbeiten
- Templates definieren und anwenden
- Farben zuweisen

#### 3.2.2 Visuelle Darstellung (Hierarchie-Baum)

**Layout:**
Das Fenster zeigt eine **baumähnliche Struktur** (ähnlich wie ein Datei-Manager oder IDE Project-Tree):

```
📁 Characters
  ├─ 📁 Main
  │  ├─ 📄 Alice
  │  ├─ 📄 Bob
  │  └─ 📄 Carol
  ├─ 📁 NPCs
  │  ├─ 📄 Guard
  │  └─ 📄 Merchant
  └─ 📁 Antagonists
     └─ 📄 Dragon

📁 Locations
  ├─ 📄 Castle
  ├─ 📄 Forest
  └─ 📄 Village

📁 Objects
  ├─ 📄 Sword
  └─ 📄 Shield
```

**Icons:**
- 📁 = Gruppe (ordner)
- 📄 = Element (Datei/Eintrag)
- Toggle-Icon (▼/►) = zum Aus/Einklappen

**Farben:**
- Jede Hauptgruppe hat eine Farbe (z.B. grüner Punkt neben „Characters")
- Untergruppen erben eine abgedunkelte oder leicht versetzte Version dieser Farbe
- Farbe wird als kleines Quadrat/Punkt vor dem Namen angezeigt

#### 3.2.3 Element-Details Ansicht

Wenn der Benutzer ein Element auswählt (z.B. auf „Alice" klickt), wird ein **Detail-Panel** angezeigt mit:

1. **Header:**
   - Name des Elements
   - Gruppen-Pfad (z.B. Characters > Main > Alice)
   - Farbindikator

2. **Field-Ansicht:**
   - Alle Felder des Elements, basierend auf dem Template
   - Beispiel für Character „Alice":
     ```
     Name:        Alice
     Age:         28
     Occupation:  Knight
     Description: A brave knight with...
     Birthday:    Day 15, Month 3
     ```

3. **Edit-Modus:**
   - Benutzer kann auf jeden Field klicken um ihn zu bearbeiten
   - Text-Felder: Inline-Edit oder Modal-Dialog
   - Enums/Dropdowns: Dropdown-Menü
   - Bilder: Thumbnail mit Replace/Upload-Option

4. **Beziehungen/Links:**
   - Zeige andere Elemente, die dieses Element erwähnen oder mit ihm verlinkt sind
   - Z.B. „Erwähnt in 5 Events", „Beziehung zu: Bob (Friend)"

5. **Aktionen-Button:**
   - Edit
   - Delete
   - Duplicate
   - Add to Timeline Event
   - View File (öffne rohe .md-Datei)

#### 3.2.4 Template-System & Vererbung – AUSFÜHRLICHER UI-FLOW

**Konzept:**
Der Benutzer definiert **Templates** (wie Klassendefinitionen in OOP) über die UI. Templates vorgeben, welche Felder jedes Element einer Gruppe haben soll.

**🔴 KRITISCH: Template-Felder werden NUR über UI erstellt, nicht in .md-Dateien geschrieben!**

**Template erstellen (Schritt-für-Schritt):**

**Schritt 1-2: Template-Dialog öffnen**
```
Benutzer navigiert im Gruppen-Manager zu "Characters"
Rechts-Klick → "Edit Template" ODER klick auf "⚙️ Edit Template"

Dialog öffnet sich:
┌─ Template: Characters ──────────────────────────────┐
│                                                     │
│ Existing Fields:                                    │
│ ├─ name      [Type: Text]    [Default: ""]        │
│ │            [Required ✓]    [Edit] [Delete ✕]    │
│ ├─ age       [Type: Integer] [Default: 0]         │
│ │            [Required ✓]    [Edit] [Delete ✕]    │
│ ├─ occupation [Type: Text]   [Default: ""]        │
│ │            [Required ✗]    [Edit] [Delete ✕]    │
│ └─ description [Type: Text]  [Default: ""]        │
│              [Required ✗]    [Edit] [Delete ✕]    │
│                                                     │
│ [+ Add New Field]                                  │
│                        [Save All] [Cancel]         │
└─────────────────────────────────────────────────────┘
```

**Schritt 3: [+ Add New Field] klicken**
Ein neuer **Field-Creation-Dialog** öffnet sich:
```
┌─ New Field Definition ──────────────────────────┐
│                                                │
│ Field Name:                                   │
│ [________________________________]             │
│ (z.B. "age", "birthday", "status")           │
│                                                │
│ Field Type: [▼ -- Select Type --]            │
│ ┌──────────────────────────────┐             │
│ │ - Text                       │             │
│ │ - Integer                    │ ← wählen   │
│ │ - Float                      │             │
│ │ - Date                       │             │
│ │ - Enum                       │             │
│ │ - Boolean                    │             │
│ │ - List                       │             │
│ │ - Reference                  │             │
│ │ - File                       │             │
│ └──────────────────────────────┘             │
│                                                │
│ Required: [☐] (Checkbox)                      │
│ (Wenn aktiviert: Feld MUSS beim Element    │
│  ausgefüllt werden)                          │
│                                                │
│ Default Value:                                │
│ [________________________________]             │
│ (Hängt vom Type ab:                         │
│  - Text: Leeres Feld                         │
│  - Integer: "0"                              │
│  - Date: Leer oder heutiges Datum            │
│  - Boolean: Unchecked)                       │
│                                                │
│ Description/Label:                            │
│ [________________________________]             │
│ [________________________________]             │
│ (Für den Benutzer: "Alter des Charakters"  │
│  wird als Tooltip/Hilfetext angezeigt)      │
│                                                │
│ Display Format: (optional)                    │
│ [________________________________]             │
│ (Z.B. "DD.MM.YYYY" für Date,                │
│  "Years old" für Integer Alter)             │
│                                                │
│                    [Save] [Cancel]            │
└────────────────────────────────────────────────┘
```

**Schritt 4: Beispiel – "age"-Feld hinzufügen**
Benutzer füllt aus:
```
Field Name:       age
Field Type:       Integer (aus Dropdown wählen)
Required:         ✓ (Checkbox angehakt)
Default Value:    0
Description:      Alter des Charakters in Jahren
Display Format:   (leer, Standard)
```

**Schritt 5: [Save] klicken**
Dialog schliesst, neues Feld taucht in Template-Liste auf:
```
├─ age       [Type: Integer] [Default: 0]
│            [Required ✓]    [Edit] [Delete ✕]
```

**Schritt 6: Template speichern**
Benutzer klickt [Save All] im Haupt-Template-Dialog

**Programm-Aktion:**
1. Aktualisiert `metadata.json` mit neuer Feld-Definition
2. Für ALLE existierenden Elemente (Alice, Bob, Guard, etc.):
   - Prüft: Existiert bereits ein "age"-Feld?
   - Falls NEIN: Fügt Feld mit Default-Wert (0) hinzu
   - Falls JA: Lässt Wert unverändert
3. Schreibt alle geänderten .md-Dateien in Obsidian-Vault
4. Aktualisiert die UI in allen offenen Fenstern

**Beispiel – Auswirkung auf Alice.md nach Feld-Addition:**

**Alte Alice.md (vorher):**
```markdown
# Alice
- name: Alice
- age: 28
- occupation: Knight
- description: A brave knight...
```

**Neue Alice.md (nachher, mit neuem birthday-Feld):**
```markdown
# Alice
- name: Alice
- age: 28
- occupation: Knight
- birthday: 
- description: A brave knight...
```
(Das "birthday"-Feld ist leer, da kein sinnvoller Default für Date)

**Neuer Charakter bekommt das Feld automatisch:**
Wenn Benutzer jetzt einen neuen Character erstellt, zeigt der Dialog ALLE Template-Felder inkl. "birthday" mit [Required ✓] Indikator.

**Feld-Typ-Erläuterung für UI-Eingaben:**

**Template anwenden:**
- Wenn ein neues Element erstellt wird, müssen alle **Required**-Felder ausgefüllt werden
- Optionale Felder können leer gelassen werden
- Weitere Felder können vom Benutzer hinzugefügt werden

**Vererbung über Untergruppen:**
Wenn der Benutzer ein Template auf eine **Untergruppe** anwendet, erben alle Elemente dieser Untergruppe diese Felder:

```
Template: Characters (Hauptgruppe)
  - name, age, occupation

Template: Main (Untergruppe, erbt von Characters)
  - name, age, occupation  [geerbt]
  - backstory              [zusätzlich]
  - personality_traits     [zusätzlich]

Wenn Alice unter "Main" erstellt wird:
  - Muss: name, age, occupation
  - Kann auch ausfüllen: backstory, personality_traits
```

**Feld-Typen (Datentypen):**

1. **Text** (String)
   - Beliebig lange Textzeichenfolge
   - Editor: Einfaches Text-Input oder Rich-Text-Editor
   - Speicherung: Plain Text oder Markdown

2. **Integer** (Ganzzahl)
   - Nur Zahlen (positiv oder negativ)
   - Editor: Zahleneingabe mit Up/Down-Pfeilen
   - Speicherung: `18`, `-5`, `0`

3. **Float** (Dezimalzahl)
   - Zahlen mit Dezimalstellen
   - Editor: Zahleneingabe
   - Speicherung: `3.14`, `18.5`

4. **Date** (Datum)
   - Zeitpunkt in der Geschichte (z.B. „Day 15, Month 3, Year 2")
   - Editor: Date-Picker oder Text-Input
   - Speicherung: `2-3-15` oder ähnlich

5. **Enum** (Aufzählung)
   - Vorgegebene Optionen, nur eine auswählbar
   - Beispiel: `status: ["Alive", "Dead", "Missing"]`
   - Editor: Dropdown-Menü
   - Speicherung: `Alive`

6. **Boolean** (Ja/Nein)
   - Nur zwei Zustände: wahr oder falsch
   - Beispiel: `is_main_character: true`
   - Editor: Checkbox
   - Speicherung: `true` oder `false`

7. **List** (Liste)
   - Mehrere Werte vom selben Typ
   - Beispiel: `skills: ["Sword Fighting", "Magic", "Diplomacy"]`
   - Editor: Liste mit Add/Remove-Buttons
   - Speicherung: `["item1", "item2", "item3"]`

8. **Reference** (Verweis auf anderes Element)
   - Verlinkt auf ein Element aus einer anderen Gruppe
   - Beispiel: Character `Alice` hat Reference zu Charakter `Bob` (als Freund)
   - Editor: Suchfeld mit Autocomplete
   - Speicherung: `@Characters/Main/Bob`

9. **File** (Datei)
   - Ein Bild, PDF oder andere Datei
   - Editor: File-Upload oder Drag-Drop
   - Speicherung: Datei wird in Obsidian-Vault kopiert, Link gespeichert

#### 3.2.5 Farbassignment & Automatische Farbvergabe

**Farb-Hierarchie:**
1. **Hauptgruppe** bekommt eine Farbe zugewiesen (z.B. grün für Characters)
2. **Untergruppen** erben eine abgestufte Variante:
   - Leicht dunkler oder heller als Hauptgruppe
   - Visuell erkennbar als verwandt mit Hauptgruppe

3. **Elemente** erben die Farbe ihrer nächsten Eltern-Gruppe

**Manuelle Farbwahl:**
- Benutzer kann Farben manuell ändern via **Color-Picker-Dialog**
- Beim Ändern einer Hauptgruppen-Farbe wird gefragt: „Untergruppen auch anpassen?"

**Intelligente Farbvergabe:**
- Wenn der Benutzer viele Gruppen erstellt, vergibt das System automatisch unterscheidbare Farben
- Verwendet dabei ein Farbsystem (z.B. HSL), das garantiert, dass Farben genug Kontrast haben

#### 3.2.6 Verwaltungs-Aktionen

**Neue Gruppe erstellen:**
1. Benutzer klickt auf „New Group" oder + -Button in der Hierarchie
2. Ein Dialog fragt nach:
   - Gruppe-Name (z.B. „Characters")
   - Übergeordnete Gruppe (falls Untergruppe)
   - Template (optional)
   - Farbe (optional, sonst automatisch)
3. Gruppe wird erstellt und taucht sofort in der Hierarchie auf

**Neue Element erstellen:**
1. Benutzer klickt auf Gruppe (z.B. „Characters > Main")
2. Klickt auf „New Element" oder + -Button
3. Ein Dialog/Formular öffnet sich mit allen Template-Feldern
4. Benutzer füllt Required-Felder aus
5. Auf Speichern klicken → Element wird erstellt, .md-Datei wird in Obsidian geschrieben
6. Element taucht sofort in der Hierarchie auf

**Element bearbeiten:**
1. Element in Hierarchie anklicken
2. Detail-Panel zeigt alle Felder
3. Benutzer klickt auf Feld um zu bearbeiten
4. Änderungen werden sofort in der .md-Datei synchronisiert

**Element/Gruppe löschen:**
1. Benutzer wählt Element/Gruppe
2. Klickt auf „Delete" oder Rechts-Klick → Delete
3. Bestätigungs-Dialog (wegen Abhängigkeiten)
4. Wenn gelöscht: Element/Gruppe wird aus Hierarchie entfernt, .md-Datei wird gelöscht
5. Alle Verweise auf dieses Element werden angezeigt (Warnung vor Broken Links)

**Element verschieben:**
1. Benutzer kann Element via Drag-Drop in eine andere Gruppe verschieben
2. Oder: Rechts-Klick → Move To → Zielgruppe auswählen
3. Die .md-Datei wird in der Obsidian-Struktur umorganisiert

---

### 3.3 Story Visualizer (Narrative Timeline View)

Dieser Viewer zeigt die Geschichte nicht als strukturierte Datenbank, sondern wie sie **narrativ abläuft** – ähnlich einem Tagebuch oder Chronik.

#### 3.3.1 Konzept & Unterschied zur Timeline

**Timeline-Fenster:**
- Zeigt ALLE Daten, strukturiert nach Gruppen
- Ist ein Editing-Tool
- Zeigt zeitliche Abstände exakt (nicht-lineare Skalierung, aber präzise)

**Story Visualizer:**
- Zeigt eine **Erzähl-Perspektive** der Geschichte
- Ist ein **Read-Only** Viewer (zum Verstehen, nicht zum Bearbeiten)
- Zeitliche Abstände sind nicht exakt dargestellt, sondern narrativ gefiltert
- Fokussiert auf Handlung und Charakter-Entwicklung, nicht auf Metadaten

#### 3.3.2 Darstellung & Layout

**Format: Zeitstrahl-ähnlich, aber Narrations-fokussiert**

```
[Timeline der Geschichte]

--- DAY 1 ---
→ Alice kehrt in ihre Heimatstadt zurück
  (Text: 2-3 Sätze Zusammenfassung)
→ Sie trifft Bob am Markt
  (Text: 1-2 Sätze)
→ Sie erzählt Bob von ihrer langen Reise

--- [14 DAYS PASS - nothing recorded] ---

--- DAY 15 ---
→ Alice ist nun älter, erfahrener
  (Attribut-Änderung: age 27 → 28)
→ Sie erhält ein mysteriöses Schwert
  (Neue Besitztum)
→ Bob wird von Drache angegriffen
  (Status-Änderung: Bob alive → injured)

```

#### 3.3.3 Filterung & Fokus

Der Benutzer kann den Story Visualizer **filtern**, um nur Teile der Geschichte zu sehen:

**Filter-Optionen:**
1. **Nach Charakter:** Nur Events mit dieser Person zeigen
   - Z.B. „Alice" → Zeige nur Events, an denen Alice beteiligt ist
   - Andere Events werden als Zusammenfassung („6 Days Pass") angezeigt

2. **Nach Handlungsstrang:** Nur Events eines bestimmten Strang zeigen
   - Z.B. „Main Quest" vs. „Side Quests"

3. **Nach Ort:** Nur Events an diesem Ort
   - Z.B. „Castle" → Zeige nur Szenen im Castle

4. **Nach Wichtigkeit:** Nur wichtige Events zeigen
   - Events können als "Major" oder "Minor" markiert sein

#### 3.3.4 Event-Details im Visualizer

Jedes Event wird angezeigt mit:
- **Zeitmarker** (z.B. „DAY 5" oder „3 WEEKS PASS")
- **Event-Titel** (kurze Überschrift)
- **Beschreibung** (1-3 Sätze oder vollen Text, je nach Einstellung)
- **Charakter-Änderungen** (welche Werte haben sich geändert)
  - Z.B. "→ Alice: age 27→28, status injured"
- **Links** (klickbar zu Details)

#### 3.3.5 Darstellung von Werteänderungen

**Attribute-Tracking:**
Der Visualizer zeigt explizit, wenn sich Attribute eines Charakters ändern:

```
--- DAY 10 ---
→ Alice's birthday
  • Alice.age: 27 → 28
  • Alice.status: healthy → ?
```

Diese Änderungen werden basierend auf den Events in der Aktionen-Datenbank berechnet.

---

### 3.4 Connections-Fenster (Beziehungs-Manager)

Das Connections-Fenster visualisiert Beziehungen, Abhängigkeiten und permanente Verbindungen zwischen Elementen.

#### 3.4.1 Konzept & Unterschied zu Timeline/Events

**Timeline:** Zeigt zeitliche Handlungen und Ereignisse
**Connections:** Zeigt statische oder lang-andauernde Beziehungen

Beispiele für Connections:
- Alice und Bob sind verheiratet (eine permanente Verbindung)
- Das Schwert ist ein Ancient Artifact (Kategorie-Beziehung)
- Der Dragon schützt die Castle (Ort-Besitzer-Beziehung)
- Charakter X ist Feind von Charakter Y (Beziehung)

#### 3.4.2 Visuelle Darstellung

**Graph-Ansicht:**
Das Fenster zeigt einen **Netzwerk-Graph** mit:
- **Knoten** (Nodes) = Elemente (Charaktere, Orte, Objekte)
- **Kanten** (Edges) = Beziehungen/Connections zwischen Knoten

**Beispiel-Graph:**
```
    [Alice] ─── married ─── [Bob]
      │                        │
      │ loves                  │ has
      ↓                        ↓
   [Sword] ◄─── guards ─── [Castle]
      │
      └─── crafted_by ─── [Blacksmith]
```

**Knoten-Styling:**
- Knoten sind Kreise oder Icons
- Farbe folgt der Gruppe-Farbe
- Größe kann basierend auf Wichtigkeit variiert werden
- Hover zeigt Element-Name und Details

**Kanten-Styling:**
- Linie zwischen zwei Knoten
- Beschriftung mit Beziehungstyp (z.B. „married", „owns")
- Farbe kann Beziehungstyp anzeigen (z.B. rot=Konflikt, grün=Verbündeter)
- Dicke kann Stärke/Wichtigkeit anzeigen

#### 3.4.3 Interaktionen

**Knoten manipulieren:**
- Drag-Drop: Knoten verschieben (für bessere Übersicht)
- Klick: Knoten auswählen, Detail-Panel zeigt Infos
- Rechts-Klick: Kontextmenü (Edit, Delete, etc.)

**Kanten manipulieren:**
- Neue Kante hinzufügen:
  1. Benutzer wählt zwei Knoten aus
  2. Klickt auf „Add Connection" oder Rechts-Klick → Connect
  3. Ein Dialog öffnet sich mit Beziehungstyp-Optionen
  4. Benutzer wählt Typ (oder erstellt neuen) und bestätigt
  5. Kante wird gezeichnet

- Kante bearbeiten:
  1. Klick auf Kante
  2. Detail-Panel zeigt Beziehungstyp, Beschreibung, etc.
  3. Benutzer kann Edit klicken

- Kante löschen:
  1. Rechts-Klick auf Kante → Delete
  2. Kante wird gelöscht

#### 3.4.4 Filter & Fokus

**Filter nach Beziehungstyp:**
- Benutzer kann auswählen, welche Arten von Beziehungen angezeigt werden
- Z.B. nur „married" Beziehungen, oder nur „conflict" Beziehungen

**Fokus auf bestimmtes Element:**
- Benutzer klickt auf ein Element
- Graph wird gefiltert, um nur dieses Element und seine direkten Verbindungen zu zeigen
- Optionally: Auch indirekte Verbindungen zeigen (2 Grad entfernt, etc.)

#### 3.4.5 Blöcke (gruppierte Connections)

**Konzept:**
Ein "Block" ist eine Gruppe von verwandten Connections, die zusammen ein Thema oder eine Situation darstellen.

Beispiel-Block: "Triangle Love Affair"
- Connection 1: Alice ←→ Bob (married)
- Connection 2: Alice ← Carol (loves)
- Connection 3: Bob ← Carol (loves)

**Block erstellen:**
1. Benutzer wählt mehrere Elemente aus
2. Klickt auf „Create Block" oder „Group Connections"
3. Ein Dialog fragt nach Block-Name und -Beschreibung
4. Block wird erstellt und mit seinen Connections gespeichert
5. Im Visualizer können Blöcke zusammengeklappt werden (wie ein Ordner)

**Block-UI:**
- Blöcke werden als größere Box mit allen Elementen darin gezeigt
- Toggle-Icon zum Aus-/Einklappen
- Farbe des Blocks basiert auf den Elementen darin

---

### 3.5 Aktionen-Panel (Action Management) – SEPARATES FENSTER VON TIMELINE

**🔴 KRITISCH: Timeline-Fenster und Aktions-Panel sind ZWEI völlig unabhängige Fenster!**

Sie greifen auf die gleichen Daten zu, aber die Darstellung und Funktionalität sind völlig unterschiedlich.

#### 3.5.1 Konzept: Zwei Views auf die gleichen Daten

| Aspekt | **Timeline-Fenster** | **Aktions-Panel** |
|--------|---|---|
| **Visuelle Form** | Punkte auf horizontaler Zeitachse | Tabellenähnliche Listendarstellung |
| **Aufbau** | Vertikal: Gruppen/Spuren; Horizontal: Zeit | Zeilenweise Auflistung aller Aktionen |
| **Zeitanzeige** | Graphisch (Position auf der Achse) | Text in Spalte (Datum + Uhrzeit) |
| **Details** | Tooltip bei Hover; Popup bei Click | Expandierbare Zeile mit vollständigen Details |
| **Bearbeitung** | Drag-Drop zum Verschieben von Events | Formular-Dialoge zum Ändern von Feldern |
| **Filterung** | Nach Gruppen/Elementen/Zeit | Nach Charakter, Ort, Objekt, Zeit, Tags, Suche |
| **Sortierung** | Nicht änderbar (immer zeitlich) | Nach Zeit, Titel, Beteiligten, Art |
| **Gut für** | Sichtbarkeit von zeitlichen Verhältnissen | Organisation, Detailorientierte Verwaltung |
| **Fenster-Status** | Unabhängig öffenbar/schliessbar | Unabhängig öffenbar/schliessbar |

**Wichtig: Beide Fenster sind UNABHÄNGIG:**
- Benutzer kann Timeline öffnen OHNE Aktions-Panel
- Benutzer kann Aktions-Panel öffnen OHNE Timeline
- Benutzer kann beide nebeneinander öffnen
- Änderungen in einem Fenster werden LIVE im anderen synchronisiert

#### 3.5.2 Was ist eine Aktion?

Eine **Aktion** ist ein einzelnes, diskretes Ereignis in der Geschichte mit:
- Titel und Beschreibung
- Zeitpunkt (Datum + Uhrzeit)
- Beteiligte Elemente (Charaktere, Orte, Objekte)
- Optional: Bilder/Anhänge
- Optional: Attribut-Mutationen (Werteänderungen)
- Optional: Tags/Labels

**Unterschied zwischen:**
- **Aktion** = Ein diskretes Ereignis (z.B. "Alice erhält das Schwert") – wird in beiden Fenstern angezeigt
- **Connection** = Eine permanente oder lang-andauernde Beziehung (z.B. "Alice besitzt das Schwert") – wird im Connections-Fenster verwaltet
- **Timeline-Punkt** = Die visuelle Repräsentation einer Aktion im Timeline-Fenster (der Kreis/Marker)

#### 3.5.3 Aktionen-Panel Darstellung (Listenformat)

**Layout:**
```
[Filter-Bar]
┌──────────────────────────────────────────────────────────────────────┐
│ Charakter: [▼ All]  Ort: [▼ All]  Zeit: [____] - [____]  Suche: [__] │
└──────────────────────────────────────────────────────────────────────┘

┌────┬──────────────────────────┬──────────────┬──────────────┬────────┐
│ ID │ Titel                    │ Zeit         │ Beteiligte   │ Expand │
├────┼──────────────────────────┼──────────────┼──────────────┼────────┤
│ #1 │ Alice erhält das Schwert │ Day 5, 14:00 │ Alice, Sword │   ▼    │
│ #2 │ Alice treffen Bob im Wald│ Day 7, 10:30 │ Alice, Bob   │   ▼    │
│ #3 │ Dragon greift Castle an  │ Day 12, 06:00│ Dragon, ..   │   ▼    │
│    │                          │              │              │        │
│    │ [+ New Action]           │              │              │        │
└────┴──────────────────────────┴──────────────┴──────────────┴────────┘
```

**Click auf [▼] → Zeile expandiert mit Details:**
```
─────────────────────────────────────────────────────────────────────
#1 | Alice erhält das Schwert
─────────────────────────────────────────────────────────────────────
Zeit:        Day 5, 14:00 Uhr
Beschreibung: Alice findet ein uraltes Schwert in einer Ruine. Das 
             Schwert ist magisch und glüht schwach.
Beteiligte:
  ✓ Charaktere: Alice, (Ghost of Previous Owner)
  ✓ Orte: Dungeon
  ✓ Objekte: Sword (Artifact)
Tags: Magic, Discovery, Important
Anhänge: 3 Bilder
Attribut-Änderungen:
  - Alice.inventory: [] → [Sword]
  - Alice.power_level: 1 → 3
[Edit] [Delete] [Duplicate] [Go to Timeline]
─────────────────────────────────────────────────────────────────────
```

**Sortierung (Klickbar auf Spalten-Header):**
- Nach Zeit (Datum/Uhrzeit)
- Nach Titel (Alphabetisch)
- Nach beteiligten Elementen
- Nach Anzahl der Anhänge

**Filterung (Top-Bar):**
- **Nach Charakter:** Dropdown zeigt alle Charaktere; Benutzer wählt einen aus
- **Nach Ort:** Dropdown zeigt alle Orte
- **Nach Zeit-Bereich:** Start-Datum und End-Datum eingeben
- **Nach Objekten:** Multi-Select für Objekte
- **Nach Tags:** Dropdown mit Tag-Auswahl
- **Suche:** Volltextsuch über Titel und Beschreibung
- **[Reset Filters]** Button

#### 3.5.4 Live-Synchronisation zwischen Timeline und Aktions-Panel

**Beispiel 1: Event in Timeline verschieben**
```
Timeline-Fenster: Benutzer macht Drag-Drop von Event "Alice gets Sword"
                  von Day 5 zu Day 6

↓ (Programm erkennt Zeitpunkt-Änderung)

Aktions-Panel: Zeit-Spalte für diese Aktion aktualisiert sich sofort:
                OLD: "Day 5, 14:00"
                NEW: "Day 6, 14:00"

↓ (Event wird in Obsidian gespeichert)

Beide Fenster sind LIVE synchronisiert
```

**Beispiel 2: Event im Aktions-Panel bearbeiten**
```
Aktions-Panel: Benutzer expandiert Aktion #2, klickt [Edit]
               Dialog öffnet sich, ändert Beschreibung

↓ (Benutzer speichert)

Timeline: Falls das Fenster offen ist, aktualisiert sich der 
          Tooltip dieser Aktion sofort mit neuer Beschreibung
          (beim nächsten Hover)

Beide Fenster sind LIVE synchronisiert
```

**Beispiel 3: Neue Aktion in Timeline erstellen**
```
Timeline-Fenster: Benutzer klickt auf leeren Bereich in Alice-Spur
                  Dialog "New Event" öffnet sich
                  Benutzer füllt aus: Titel, Zeit, Beteiligte
                  Speichern

↓ (Event wird erstellt)

Timeline: Punkt erscheint sofort bei Day 7 in Alice-Spur
Aktions-Panel: Neue Zeile erscheint sofort in der Liste
               (Wenn Aktions-Panel offen ist)

Beide Fenster sind LIVE synchronisiert
```

#### 3.5.5 Aktion erstellen & bearbeiten

**Aktion erstellen:**
1. Benutzer klickt auf „New Action" oder + -Button
2. Ein **Aktion-Dialog** öffnet sich mit Feldern:
   - Titel (Text)
   - Beschreibung (Rich-Text-Editor)
   - Zeitpunkt (Date + Time Picker)
   - Beteiligte Elemente (Multi-Select):
     - Charaktere
     - Orte
     - Objekte
   - Tags/Labels (optional)
   - Anhänge (Bilder, Dateien)
3. Benutzer füllt aus und speichert
4. Aktion wird erstellt und:
   - In der Aktionen-Liste eingefügt
   - Als Event in der Timeline angezeigt
   - Automatisch mit Elementen verlinkt

**Aktion bearbeiten:**
1. Benutzer klickt auf Aktion in der Liste oder Timeline
2. Aktion wird ausgewählt (Highlight)
3. Ein Detail-Panel zeigt alle Informationen
4. Benutzer klickt auf „Edit"
5. Dialog öffnet sich, Benutzer ändert Werte
6. Speichern → Änderungen werden synchronisiert

#### 3.5.6 Werteänderungen durch Aktionen

**Problem:** Wenn ein Charakter älter wird oder verletzt wird, muss das System wissen, dass sich ein Attribut geändert hat.

**Lösung: Attribute-Mutation durch Aktionen**

Beim Erstellen einer Aktion kann der Benutzer auch **Attribut-Änderungen** definieren:

**Aktion-Beispiel:**
```
Titel: Alice's Birthday
Beschreibung: Alice turns one year older
Zeitpunkt: Day 365
Beteiligte Charaktere: [Alice]

Attribut-Änderungen:
  - Alice.age: 27 → 28
  - Alice.status: healthy (bleib unverändert)
```

**Implementierung:**
- Jede Aktion kann optional **Mutations** (Attribut-Änderungen) definieren
- Diese Mutations werden mit der Aktion gespeichert
- Wenn das System den aktuellen Wert eines Attributes berechnet, iteriert es über alle Aktionen bis zu einem bestimmten Zeitpunkt und wendet die Mutations an

**Beispiel-Berechnung:**
```
Query: Was ist Alices Alter am Day 10?

1. Start: Alice.age = 28 (Standard aus Template)
2. Iteration über alle Aktionen bis Day 10:
   - Day 5: Alice.age 27 → 28 (Mutation angewendet)
3. Ergebnis: Alice.age = 28 am Day 10
```

#### 3.5.7 Aktion-Typen (optional)

Der Benutzer kann Aktion-Typen definieren, um sie zu kategorisieren:

**Vordefinierte Typen (optional):**
- Dialogue (Dialog)
- Action (Handlung)
- Scene Change (Szenenwechsel)
- Discovery (Enthüllung)
- Conflict (Konflikt)
- Custom (Benutzer-definiert)

Beim Erstellen einer Aktion kann der Benutzer einen Typ auswählen. Dieser wird verwendet für Filterung und Visualisierung.

---

### 3.6 Dateimanager (File Manager)

Ein vollständig integrierter Dateimanager für die Obsidian-Vault.

#### 3.6.1 Zwei Ansichts-Modi

**Modus 1: Strukturierte Ansicht**
- Zeigt Dateien so an, wie sie logisch im Programm organisiert sind
- Ordnerstruktur folgt den Gruppen
- Beispiel:
  ```
  Characters/
    Main/
      Alice.md
      Bob.md
    NPCs/
      Guard.md
  Locations/
    Castle.md
  ```
- Benutzer kann auf eine Datei klicken um sie anzusehen/zu bearbeiten

**Modus 2: Rohe Datei-Ansicht**
- Zeigt die tatsächliche Dateistruktur der Obsidian-Vault im Dateisystem
- Erlaubt direkten Zugriff auf alle `.md`-Dateien und andere Dateitypen (Bilder, etc.)
- Benutzer kann Dateien manuell umbenennen, verschieben, löschen
- Hilfreich, wenn der Benutzer die Vault später außerhalb des Programms bearbeiten möchte

#### 3.6.2 Datei-Operationen

**Datei öffnen/anschauen:**
1. Benutzer navigiert zu Datei in strukturierter oder roher Ansicht
2. Klickt auf Datei → Ein Viewer öffnet sich
3. Für `.md`-Dateien: Markdown Renderer (oder Raw-Text)
4. Für Bilder: Image Viewer
5. Für andere: Standard-Viewer oder Hexdump

**Datei hochladen/hinzufügen:**
1. Benutzer klickt auf „Upload File" oder Drag-Drop-Zone
2. File-Dialog öffnet sich (oder Drag-Drop wird akzeptiert)
3. Benutzer wählt Datei aus
4. Dialog fragt nach:
   - Zielordner (strukturiert oder roh)
   - Neue Datei-Name (optional)
5. Datei wird hochgeladen und in Obsidian-Vault gespeichert

**Datei verlinken:**
1. Benutzer wählt eine Datei (z.B. Bild)
2. Klickt auf „Link" oder „Associate with Element"
3. Ein Dialog fragt nach Element (Charakter, Ort, etc.)
4. Datei wird mit Element verlinkt
5. Link wird in der Element-Datei (.md) eingetragen

**Datei löschen:**
1. Benutzer wählt Datei
2. Rechts-Klick → Delete oder Delete-Button
3. Bestätigungs-Dialog
4. Datei wird aus Obsidian-Vault gelöscht
5. Verweise auf gelöschte Datei werden angezeigt (Warnung)

**Datei umbenennen/verschieben:**
1. Benutzer wählt Datei
2. Rechts-Klick → Rename oder Move
3. Ein Dialog öffnet sich
4. Benutzer ändert Namen oder Zielordner
5. Datei wird umbenannt/verschoben in Obsidian

#### 3.6.3 Bilder & Media-Verwaltung

**Bilder anschauen:**
- Thumbnails der Bilddateien in strukturierter Ansicht
- Klick auf Thumbnail öffnet Bild in größerer Ansicht
- Bilder können in Element-Details eingebettet werden (z.B. Charakter-Portrait)

**Bilder hochladen:**
- Benutzer kann Bild-Datei hochladen
- Bild wird in Obsidian-Vault gespeichert (z.B. in `Assets/Images/`)
- Benutzer kann Bild mit Element verlinken (z.B. Alice.md erhält Image-Link)

---

## 4. Daten-Persistenz & Obsidian-Integration

### 4.1 Dateistruktur in Obsidian

```
MyStory/
├── metadata.json              # Projektmetadaten, Gruppen-Definitionen
├── Characters/
│   ├── Main/
│   │   ├── Alice.md
│   │   ├── Bob.md
│   │   └── Alice_portrait.png
│   └── NPCs/
│       └── Guard.md
├── Locations/
│   ├── Castle.md
│   └── Forest.md
├── Objects/
│   ├── Sword.md
│   └── Shield.md
├── Timeline/
│   ├── events.json            # oder events.md mit strukturiertem Format
│   └── event_details/
│       ├── event_001.md
│       ├── event_002.md
│       └── ...
├── Connections/
│   └── relationships.json     # Speichert Beziehungen zwischen Elementen
├── Actions/
│   └── actions.json           # Speichert alle Aktionen mit Mutations
└── Assets/
    ├── Images/
    │   ├── character_portraits/
    │   ├── maps/
    │   └── ...
    └── Documents/
        └── ...
```

### 4.2 Datenformat-Beispiele

**Element-Datei (z.B. Alice.md):**
```markdown
# Alice

## Basic Info
- Name: Alice
- Age: 28
- Occupation: Knight
- Birthday: Day 15, Month 3

## Backstory
A brave knight with a troubled past...

## Personality Traits
- Courageous
- Stubborn
- Loyal

## Relations
- married_to: [[Bob]]
- friend_of: [[Carol]]
- enemy_of: [[Dragon]]

## Linked Events
- Event #1: Alice erhält das Schwert (Day 5)
- Event #3: Alice's Birthday (Day 365)
```

**Timeline-Datei (events.json):**
```json
{
  "events": [
    {
      "id": "event_001",
      "title": "Alice erhält das Schwert",
      "description": "Alice finds an ancient sword in the dungeon.",
      "timestamp": "Day 5, 14:00",
      "elements": ["Characters/Main/Alice", "Objects/Sword"],
      "tags": ["Discovery", "Magic"],
      "mutations": [
        {
          "element": "Characters/Main/Alice",
          "field": "inventory",
          "old_value": [],
          "new_value": ["Objects/Sword"]
        }
      ]
    },
    ...
  ]
}
```

**Connections-Datei (relationships.json):**
```json
{
  "connections": [
    {
      "id": "conn_001",
      "source": "Characters/Main/Alice",
      "target": "Characters/Main/Bob",
      "type": "married_to",
      "description": "Alice and Bob are married.",
      "start_date": "Day 1",
      "end_date": null
    },
    ...
  ]
}
```

---

## 5. Benutzerinteraktions-Flows

### 5.1 Neue Geschichte starten

1. Benutzer öffnet Programm
2. Klickt auf „New Project" oder „File → New"
3. Dialog fragt nach:
   - Projekt-Name (z.B. „Epic Fantasy Novel")
   - Speicherort (Folder für Obsidian-Vault)
4. Ein neues Obsidian-Vault wird erstellt mit Standard-Struktur
5. Programm öffnet leeres Projekt
6. Benutzer sieht leere Timeline, leerer Gruppen-Manager, etc.

### 5.2 Neue Gruppe erstellen

1. Benutzer klickt im Gruppen-Manager auf „New Group"
2. Dialog: Name eingeben (z.B. „Characters")
3. Optional Template auswählen oder neu erstellen
4. Optional Farbe wählen
5. Gruppe wird erstellt und in Hierarchie eingefügt

### 5.3 Neuen Charakter hinzufügen

1. Benutzer navigiert zu Gruppe „Characters" im Gruppen-Manager
2. Klickt auf „New Element" oder Rechts-Klick → New
3. Dialog öffnet sich mit Template-Feldern:
   - Name: Alice (eingeben)
   - Age: 28 (eingeben)
   - Occupation: Knight (eingeben)
4. Optional: Beschreibung, Bild hinzufügen
5. Speichern → Alice.md wird in Obsidian erstellt
6. Alice erscheint sofort in der Hierarchie

### 5.4 Szene schreiben & in Timeline eintragen

1. Benutzer öffnet Timeline-Fenster
2. Klickt auf einen Punkt in einer Spur oder einen leeren Bereich der Timeline
3. Dialog „New Event" öffnet sich
4. Benutzer gibt ein:
   - Titel: "Alice meets Bob"
   - Beschreibung/Szene-Text
   - Zeitpunkt: Day 5, 14:00
   - Beteiligte Charaktere: [Alice, Bob]
5. Speichern → Event wird erstellt
6. Ein Punkt erscheint in der Timeline bei Day 5
7. Alice und Bob Spuren zeigen jetzt diesen Event

### 5.5 Charakter-Attribut ändern durch Event

1. Benutzer erstellt Event „Alice's Birthday"
2. Im Event-Dialog, zusätzlicher Abschnitt: „Attribute Changes"
3. Benutzer klickt auf „Add Mutation"
4. Wählt Element (Alice) und Feld (age)
5. Gibt ein: 27 → 28
6. Event wird gespeichert mit dieser Mutation
7. Das System merkt sich: Ab diesem Event ist Alice 28 Jahre alt

### 5.6 Beziehung zwischen Charakteren erstellen

1. Benutzer öffnet Connections-Fenster
2. Klickt auf Alice (auswählen)
3. Klickt auf Bob (auswählen)
4. Dialog „Create Connection" öffnet sich
5. Benutzer wählt Beziehungstyp: „married_to"
6. Gibt optional Start- und End-Datum an
7. Speichern → Verbindung wird erstellt
8. Graph zeigt Linie zwischen Alice und Bob mit Label „married to"

---

## 6. Technische Anforderungen (Zusammenfassung)

### 6.1 Performance
- Timeline muss auch mit 100+ Events noch flüssig laufen
- Gruppen-Manager muss auch mit 500+ Elementen responsiv sein
- Fenster-Wechsel soll unter 100ms sein

### 6.2 Datensicherheit
- Automatisches Speichern nach jeder Änderung in Obsidian-Vault
- Keine in-Memory-Daten sollten verloren gehen (persistiert alles)
- Backup-Optionen (optional)

### 6.3 Erweiterbarkeit
- Plugin-System (optional, aber nützlich)
- Benutzer-definierte Feld-Typen (möglich durch Template-System)
- Benutzer-definierte Beziehungstypen (möglich durch Connections-System)

### 6.4 Bedienbarkeit
- Keyboard-Shortcuts für häufige Aktionen (z.B. Ctrl+N = New Element)
- Undo/Redo-Unterstützung
- Tooltips und Hilfe-Text
- Intuitive Menüs und Dialog-Fenster

---

## 7. UI/UX Details

### 7.1 Farbsystem
- Alle Farben müssen aus Variablen definiert sein
- Beispiel-Palette:
  ```
  Primärfarbe (Accent): #4A90E2
  Erfolg: #7ED321
  Warnung: #F5A623
  Fehler: #D0021B
  Text: #222222
  Hintergrund: #FFFFFF
  Panel: #F5F5F5
  Gruppe Characters: #2E7D32
  Gruppe Locations: #1976D2
  Gruppe Objects: #FF6F00
  ```

### 7.2 Fenster-Status speichern
- Fenster-Positionen, -Größen, -Zustände (geöffnet/geschlossen) sollten beim Schließen gespeichert werden
- Beim Neu-Öffnen des Programms sollten Fenster in den letzten bekannten Zustand zurückkehren

### 7.3 Fehlerbehandlung
- Wenn Obsidian-Datei nicht lesbar ist: Fehler-Dialog mit Optionen
- Wenn Datei gleichzeitig extern bearbeitet wird: Reload/Merge Dialog
- Wenn Löschen würde Links brechen: Warnung mit Details

---

## 8. Priorisierung (Phasen)

### Phase 1 (MVP - Minimum Viable Product)
1. Gruppen-Manager (basic)
2. Timeline-Fenster (basic, einfache Skalierung)
3. Aktionen-Management (basic)
4. Obsidian-Integration (Lesen/Schreiben)
5. UI-Grundlayout

### Phase 2
1. Story Visualizer
2. Connections-Fenster
3. Erweiterte Timeline-Features (Filter, Drag-Drop)
4. Template-System (vollständig)

### Phase 3
1. Dateimanager
2. Export-Optionen
3. Backup/Version-Control
4. Plugin-System

---

## Zusammenfassung

Der Story Editor ist ein umfassendes, projektambitioniertes Software-System, das Schriftsteller bei der komplexen Verwaltung großer narrativer Werke unterstützt. Mit der Timeline als Hauptnavigationsmittel, dem hierarchischen Gruppen-Manager, dem intelligenten Template-System, und der visuellen Verbindungs-Verwaltung ermöglicht es strukturiertes, fehlerfreies Geschichtenerzählen.