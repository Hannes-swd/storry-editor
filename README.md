<div align="center">

# 📖 Story Editor

**Schreib‑Software für große Geschichten.**
Du schreibst – das Programm merkt sich, wer wann wo war, wie alt jemand gerade ist
und was schon passiert ist.

[<kbd> &nbsp; 🚀 &nbsp; Loslegen &nbsp; </kbd>](docs/de/01-loslegen.md)
[<kbd> &nbsp; ✍️ &nbsp; Schreiben lernen &nbsp; </kbd>](docs/de/03-manuskript.md)
[<kbd> &nbsp; 🗺️ &nbsp; Alle Kapitel &nbsp; </kbd>](#-handbuch)
[<kbd> &nbsp; 🇬🇧 &nbsp; English &nbsp; </kbd>](README.en.md)

<br>

![Das Programmfenster](docs/bilder/ueberblick.png)

</div>

---

## 🤔 Wofür ist das?

Bei einer langen Geschichte verliert man irgendwann den Überblick:

> *War Alice in Kapitel 12 eigentlich schon 28?
> Hat Bob zu dem Zeitpunkt überhaupt schon von dem Schwert gewusst?
> Und wo war der Drache, als die Burg angegriffen wurde?*

Der Story Editor beantwortet solche Fragen, **während du schreibst**. Du tippst deinen Text
ganz normal; wo eine Figur vorkommt, setzt du sie mit einem Klick ein. Daraus baut das
Programm im Hintergrund eine Zeitleiste, eine Figurendatenbank und ein Beziehungsnetz.

```
              Du schreibst                        Das Programm baut daraus
   ┌────────────────────────────────┐        ┌──────────────────────────────┐
   │  "@Alice war inzwischen        │        │  📅 Zeitleiste               │
   │   @Alice.age Jahre alt."       │  ───▶  │  👥 Figuren mit Werten       │
   │                                │        │  🔗 Beziehungen              │
   │  #Tag 11, 08:00                │        │  📋 Ereignisliste            │
   └────────────────────────────────┘        └──────────────────────────────┘
              ein Text                          alles andere entsteht daraus
```

Gespeichert wird alles als **lesbare Markdown‑Dateien in einem Obsidian‑Vault** – kein
geheimes Dateiformat, kein Cloud‑Konto, und mit Git versionierbar.

---

## ⚡ In 5 Minuten loslegen

```sh
# 1. Bauen (einmalig, braucht Visual Studio 2022 + CMake)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release

# 2. Ein Beispielprojekt zum Ausprobieren anlegen
./build/bin/StoryEditor.exe --demo C:/Temp/MeineDemo

# 3. Starten und den Ordner öffnen (Datei → Projekt öffnen…)
./build/bin/StoryEditor.exe
```

Ausführlich – mit Erklärung jedes Schritts – steht das in
**[Kapitel 1 · Loslegen](docs/de/01-loslegen.md)**.

---

## 📚 Handbuch

Jedes Kapitel ist eine eigene Seite. Fang oben links an und arbeite dich durch –
oder spring direkt zu dem, was du gerade brauchst.

### Für den Anfang

| | |
|:--|:--|
| **[📦 1 · Loslegen](docs/de/01-loslegen.md)**<br>Bauen, starten, erstes Projekt anlegen. Was ist ein „Vault"? | **[🪟 2 · Das Programmfenster](docs/de/02-ueberblick.md)**<br>Welches Fenster macht was, wie schiebt man sie herum, wie holt man ein geschlossenes zurück. |
| **[✍️ 3 · Schreiben](docs/de/03-manuskript.md)**<br>Das Herzstück. Text tippen, Figuren einsetzen, Kapitel, Fett/Kursiv, Suchen & Ersetzen. | **[👥 4 · Figuren, Orte, Dinge](docs/de/04-gruppen.md)**<br>Gruppen anlegen, Elemente erstellen, Vorlagen und Feldtypen. |

### Das Zeit‑System

| | |
|:--|:--|
| **[⏳ 5 · Wie die Zeit funktioniert](docs/de/05-zeit.md)**<br>Der wichtigste Begriff im ganzen Programm. Unbedingt lesen. | **[⚡ 6 · Ereignisse & Werteänderungen](docs/de/06-aktionen.md)**<br>Aktionen anlegen, Alter hochzählen, Zustände ändern. |
| **[📅 7 · Die Timeline](docs/de/07-timeline.md)**<br>Alles auf einer Zeitachse: Spuren, Zoom, verschieben, filtern. | **[🔗 8 · Beziehungen](docs/de/08-connections.md)**<br>Wer ist mit wem verheiratet, wer ist gerade wo – mit eigenen Vorlagen. |

### Ansehen, herausgeben, einstellen

| | |
|:--|:--|
| **[📜 9 · Chronik & Detailpanel](docs/de/09-chronik.md)**<br>Die Geschichte als Erzählung lesen; alles über ein einzelnes Element sehen. | **[💾 10 · Dateien, Bilder & Export](docs/de/10-dateien.md)**<br>Bilder einbinden, Word‑Datei erzeugen, in Obsidian weiterlesen. |
| **[⚙️ 11 · Einstellen & Bedienen](docs/de/11-einstellungen.md)**<br>Design, Sprache, eigene Listen, Tastenkürzel, Speichern & Undo. | **[🔧 12 · Unter der Haube](docs/de/12-technik.md)**<br>Für Neugierige und Entwickler: Dateiformat, Quellcode, Selbsttest, Grenzen. |

---

## 🧭 Die vier Bausteine

Wenn du nur eine Sache aus diesem Handbuch mitnimmst, dann diese vier Begriffe –
alles andere baut darauf auf:

| Baustein | Was es ist | Beispiel | Kapitel |
|:--|:--|:--|:--|
| 🧩 **Element** | Ein Ding in deiner Welt. Hat Felder mit Werten. | Alice, die Burg, das Schwert | [4](docs/de/04-gruppen.md) |
| ⏳ **Zeitpunkt** | Wo in der Geschichte du gerade bist. Bleibt stehen, bis du ihn weiterstellst. | „Tag 11, 08:00" | [5](docs/de/05-zeit.md) |
| ⚡ **Aktion** | Etwas passiert. Kann Werte von Elementen ändern. | „Alice' Geburtstag" ändert `age: 27 → 28` | [6](docs/de/06-aktionen.md) |
| 🔗 **Verbindung** | Ein Zustand, der eine Weile gilt. | „Alice ist an Ort: Burg" ab Tag 1 | [8](docs/de/08-connections.md) |

```
                        ┌──────────────┐
                        │   ELEMENT    │   Alice
                        │  name, age…  │   age = 27
                        └──────┬───────┘
                               │
            ┌──────────────────┼──────────────────┐
            │                  │                  │
     ┌──────▼──────┐    ┌──────▼──────┐    ┌──────▼───────┐
     │   AKTION    │    │ VERBINDUNG  │    │  MANUSKRIPT  │
     │ Geburtstag  │    │ ist an Ort: │    │  "@Alice ist │
     │ Tag 11      │    │ Burg        │    │  @Alice.age" │
     │ age 27 → 28 │    │ ab Tag 1    │    │              │
     └─────────────┘    └─────────────┘    └──────┬───────┘
            │                  │                  │
            └──────────────────┴──────────────────┘
                               │
                      ab Tag 11 ist age = 28
                   — überall im Programm gleichzeitig
```

---

## 💡 Was das Programm anders macht

| | |
|:--|:--|
| **Keine festen Begriffe** | Das Programm kennt kein „Person" oder „Ort". Du legst selbst fest, welche Gruppen es gibt und welche Felder sie haben. → [Kapitel 4](docs/de/04-gruppen.md) |
| **Werte kennen die Zeit** | `@Alice.age` steht in Kapitel 1 für 27 und in Kapitel 9 für 28 – im selben Text, ohne dass du etwas nachträgst. → [Kapitel 5](docs/de/05-zeit.md) |
| **Struktur fällt nebenbei ab** | Du schreibst einen Absatz und machst daraus mit einem Klick ein Ereignis. Die Timeline füllt sich beim Schreiben, nicht durch Formulare. → [Kapitel 6](docs/de/06-aktionen.md) |
| **Deine Dateien bleiben deine** | Alles liegt als Markdown + JSON in einem Ordner. Du kannst es mit Obsidian lesen und mit Git versionieren. → [Kapitel 12](docs/de/12-technik.md) |
| **Zweisprachig** | Die ganze Oberfläche gibt es auf Deutsch und Englisch, umschaltbar im laufenden Betrieb. → [Kapitel 11](docs/de/11-einstellungen.md) |

---

## 🛠️ Technisches auf einen Blick

| | |
|:--|:--|
| **Sprache** | C++17 |
| **Oberfläche** | [Dear ImGui](https://github.com/ocornut/imgui) (Immediate‑Mode‑GUI) |
| **Plattform** | Windows, Win32 + DirectX 11 |
| **Speicherung** | Obsidian‑Vault: Markdown‑Dateien + JSON |
| **Abhängigkeiten** | ImGui, nlohmann/json, stb_image – werden von CMake automatisch geholt |
| **Tests** | `StoryEditor.exe --selftest` – 122 Prüfungen, Exitcode 0 = alles gut |

Die vollständige Anforderungsspezifikation, nach der gebaut wurde, steht in
[`claude.md`](claude.md).

---

<div align="center">

[<kbd> &nbsp; 🚀 &nbsp; Weiter zu Kapitel 1 · Loslegen &nbsp; </kbd>](docs/de/01-loslegen.md)

</div>
