[<kbd> &nbsp; ← Übersicht &nbsp; </kbd>](../../README.md)
[<kbd> &nbsp; Weiter: 2 · Das Programmfenster → &nbsp; </kbd>](02-ueberblick.md)

---

# 📦 Kapitel 1 · Loslegen

> **Für wen ist dieses Kapitel?** Für alle. Am Ende hast du das Programm gebaut,
> gestartet und ein Beispielprojekt offen, mit dem du alles ausprobieren kannst.

---

## 1.1 Was du brauchst

| | |
|:--|:--|
| 🖥️ **Windows** | 10 oder 11. Es gibt (noch) keine Mac‑ oder Linux‑Version. |
| 🔨 **Visual Studio 2022** | Die kostenlose *Community*‑Ausgabe reicht. Beim Installieren die Arbeitslast **„Desktopentwicklung mit C++"** anhaken. |
| 📐 **CMake ≥ 3.20** | Ist bei Visual Studio meist dabei. Sonst von [cmake.org](https://cmake.org/download/). |
| 🌐 **Internet** | Nur beim allerersten Bauen – CMake lädt ImGui, nlohmann/json und stb_image automatisch nach. |

---

## 1.2 Bauen

Öffne eine Eingabeaufforderung im Projektordner und tippe:

```sh
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Was passiert dabei?

```
  cmake -S . -B build ...          ┌─ liest CMakeLists.txt
                                   ├─ lädt ImGui + json + stb_image herunter
                                   └─ erzeugt den Ordner  build/  mit einer
                                      Visual-Studio-Projektmappe

  cmake --build build ...          ┌─ kompiliert alles
                                   └─ legt  build/bin/StoryEditor.exe  an
```

Der erste Durchlauf dauert ein paar Minuten (ImGui muss mitkompiliert werden), jeder
weitere nur noch Sekunden.

> ✅ **Geklappt?** Dann liegt jetzt `build/bin/StoryEditor.exe` da.
>
> ❌ **Fehler beim ersten Befehl?** Dann fehlt meist die C++‑Arbeitslast in Visual Studio.
> Visual‑Studio‑Installer öffnen → *Ändern* → „Desktopentwicklung mit C++" anhaken.

---

## 1.3 Prüfen, ob alles stimmt

Das Programm bringt einen eingebauten Selbsttest mit. Der startet keine Oberfläche,
sondern rechnet nur durch, ob Zeitlogik, Dateiformate und Export sauber funktionieren:

```sh
./build/bin/StoryEditor.exe --selftest
```

Am Ende steht so etwas:

```
[ ok ] manuscript: value after the mutation
[ ok ] docx: heading becomes a Word heading
---------------------
122 ok, 0 fehlgeschlagen
```

---

## 1.4 Ein Beispielprojekt zum Anfassen

Bevor du dein eigenes Buch anfängst: lass dir ein fertiges kleines Projekt bauen.
Da ist alles schon drin – zwei Figuren, zwei Orte, ein Schwert, sechs Ereignisse und
ein paar Beziehungen.

```sh
./build/bin/StoryEditor.exe --demo C:/Temp/MeineDemo
```

Danach das Programm normal starten und den Ordner öffnen:

```sh
./build/bin/StoryEditor.exe
```

**Datei → Projekt öffnen…** → `C:/Temp/MeineDemo` auswählen.

Ab jetzt merkt sich das Programm den Ordner und öffnet ihn beim nächsten Start von selbst.

---

## 1.5 Was ist dieser „Vault"?

Ein **Vault** ist einfach ein Ordner. Darin liegt deine ganze Geschichte – als Dateien,
die du auch ohne das Programm lesen kannst.

```
MeineDemo/                      ← das ist der Vault
├── metadata.json                  Gruppen, Vorlagen, Farben
├── Characters/
│   ├── Main/
│   │   ├── Alice.md               eine Figur = eine Textdatei
│   │   └── Bob.md
│   └── NPCs/
│       └── Guard.md
├── Locations/
│   ├── Castle.md
│   └── Forest.md
├── Objects/
│   └── Sword.md
├── Actions/actions.json           alle Ereignisse
├── Connections/relationships.json alle Beziehungen
├── Manuscript/
│   ├── manuscript.md              dein Text, mit Marken
│   └── gelesen.md                 derselbe Text, fertig lesbar
└── Assets/                        Bilder und andere Dateien
```

Der Name kommt von **Obsidian** – einem kostenlosen Notiz‑Programm, das genau solche
Ordner liest. Du **brauchst** Obsidian nicht; wenn du es hast, kannst du deine Geschichte
damit aber zusätzlich lesen und durchsuchen.

> ### 🔴 Die eine Regel
>
> **Bearbeite die `.md`‑Dateien niemals von Hand.**
>
> Das Programm ist der einzige Schreiber. Es merkt zwar, wenn du von außen etwas änderst,
> und fragt dann nach (*Reload / Merge / Cancel*) – aber du riskierst, dass Verknüpfungen
> kaputtgehen. Alles, was du ändern willst, änderst du über die Oberfläche.

---

## 1.6 Ein eigenes Projekt anlegen

Wenn du mit der Demo fertig gespielt hast:

1. **Datei → Neues Projekt…** (oder <kbd>Strg</kbd>+<kbd>Umschalt</kbd>+<kbd>N</kbd>)
2. Einen **Namen** eingeben – zum Beispiel `Die Ritterin von Grauenstein`
3. Einen **leeren Ordner** auswählen, in dem der Vault entstehen soll
4. Speichern

Das Programm legt sofort eine Grundstruktur an: die Gruppen `Characters` (mit `Main` und
`NPCs`), `Locations` und `Objects`, dazu passende Vorlagen. Das ist nur ein Vorschlag –
du kannst alles umbenennen, löschen und eigene Gruppen anlegen
(→ [Kapitel 4](04-gruppen.md)).

---

## 1.7 Die Kommandozeile

| Aufruf | Wirkung |
|:--|:--|
| `StoryEditor.exe` | Normale Oberfläche, öffnet das zuletzt benutzte Projekt |
| `StoryEditor.exe --selftest` | 122 Prüfungen, keine Oberfläche. Exitcode `0` = alles gut |
| `StoryEditor.exe --demo <Ordner>` | Legt das Beispielprojekt in diesem Ordner an |

---

## ✅ Kurz gesagt

- Bauen: `cmake -S . -B build -G "Visual Studio 17 2022" -A x64` und `cmake --build build --config Release`
- Zum Ausprobieren: `--demo <Ordner>` erzeugt ein fertiges Beispiel
- Ein **Vault** ist ein ganz normaler Ordner voller Markdown‑Dateien
- **`.md`‑Dateien nur über das Programm ändern**, nie mit einem Texteditor

---

[<kbd> &nbsp; ← Übersicht &nbsp; </kbd>](../../README.md)
[<kbd> &nbsp; Weiter: 2 · Das Programmfenster → &nbsp; </kbd>](02-ueberblick.md)
