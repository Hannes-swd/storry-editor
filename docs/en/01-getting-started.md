[<kbd> &nbsp; ← Overview &nbsp; </kbd>](../../README.en.md)
[<kbd> &nbsp; Next: 2 · The program window → &nbsp; </kbd>](02-overview.md)

---

# 📦 Chapter 1 · Getting started

> **Who is this chapter for?** Everyone. By the end you will have built the program,
> started it, and have a sample project open to try everything out with.

---

## 1.1 What you need

| | |
|:--|:--|
| 🖥️ **Windows** | 10 or 11. There is (as yet) no Mac or Linux version. |
| 🔨 **Visual Studio 2022** | The free *Community* edition is enough. During installation tick the **"Desktop development with C++"** workload. |
| 📐 **CMake ≥ 3.20** | Usually comes with Visual Studio. Otherwise from [cmake.org](https://cmake.org/download/). |
| 🌐 **Internet** | Only for the very first build – CMake downloads ImGui, nlohmann/json and stb_image automatically. |

---

## 1.2 Building

Open a command prompt in the project folder and type:

```sh
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

What happens there:

```
  cmake -S . -B build ...          ┌─ reads CMakeLists.txt
                                   ├─ downloads ImGui + json + stb_image
                                   └─ creates the folder  build/  with a
                                      Visual Studio solution

  cmake --build build ...          ┌─ compiles everything
                                   └─ produces  build/bin/StoryEditor.exe
```

The first run takes a few minutes (ImGui has to be compiled too); every later one
takes seconds.

> ✅ **Worked?** Then `build/bin/StoryEditor.exe` now exists.
>
> ❌ **Error on the first command?** Usually the C++ workload is missing in Visual
> Studio. Open the Visual Studio Installer → *Modify* → tick "Desktop development
> with C++".

---

## 1.3 Checking that everything is sound

The program ships a built-in self test. It starts no interface, it just verifies that
time logic, file formats and export all work:

```sh
./build/bin/StoryEditor.exe --selftest
```

At the end you get something like:

```
[ ok ] manuscript: value after the mutation
[ ok ] docx: heading becomes a Word heading
---------------------
122 ok, 0 fehlgeschlagen
```

---

## 1.4 A sample project to poke at

Before starting your own book: have a small finished project generated. It already
contains everything – two characters, two places, a sword, six events and a few
relationships.

```sh
./build/bin/StoryEditor.exe --demo C:/Temp/MyDemo
```

Then start the program normally and open the folder:

```sh
./build/bin/StoryEditor.exe
```

**File → Open project…** → pick `C:/Temp/MyDemo`.

From now on the program remembers the folder and opens it on the next start.

---

## 1.5 What is this "vault"?

A **vault** is simply a folder. Your whole story lives inside it – as files you can
read without the program too.

```
MyDemo/                         ← this is the vault
├── metadata.json                  groups, templates, colours
├── Characters/
│   ├── Main/
│   │   ├── Alice.md               one character = one text file
│   │   └── Bob.md
│   └── NPCs/
│       └── Guard.md
├── Locations/
│   ├── Castle.md
│   └── Forest.md
├── Objects/
│   └── Sword.md
├── Actions/actions.json           all events
├── Connections/relationships.json all relationships
├── Manuscript/
│   ├── manuscript.md              your text, with markers
│   └── gelesen.md                 the same text, ready to read
└── Assets/                        images and other files
```

The name comes from **Obsidian** – a free note-taking program that reads exactly this
kind of folder. You do **not** need Obsidian; if you have it, you can read and search
your story with it as well.

> ### 🔴 The one rule
>
> **Never edit the `.md` files by hand.**
>
> The program is the only writer. It does notice outside changes and asks what to do
> (*Reload / Merge / Cancel*) – but you risk breaking links. Everything you want to
> change, you change through the interface.

---

## 1.6 Creating your own project

Once you are done playing with the demo:

1. **File → New project…** (or <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>N</kbd>)
2. Enter a **name** – for example `The Knight of Greystone`
3. Pick an **empty folder** for the vault
4. Save

The program immediately creates a basic structure: the groups `Characters` (with `Main`
and `NPCs`), `Locations` and `Objects`, plus matching templates. That is only a
suggestion – you can rename, delete and add your own groups
(→ [Chapter 4](04-groups.md)).

---

## 1.7 The command line

| Command | Effect |
|:--|:--|
| `StoryEditor.exe` | Normal interface, opens the most recently used project |
| `StoryEditor.exe --selftest` | 122 checks, no interface. Exit code `0` = all good |
| `StoryEditor.exe --demo <folder>` | Creates the sample project in that folder |

---

## ✅ In short

- Build: `cmake -S . -B build -G "Visual Studio 17 2022" -A x64` then `cmake --build build --config Release`
- To try things out: `--demo <folder>` produces a ready-made example
- A **vault** is an ordinary folder full of Markdown files
- **Only change `.md` files through the program**, never with a text editor

---

[<kbd> &nbsp; ← Overview &nbsp; </kbd>](../../README.en.md)
[<kbd> &nbsp; Next: 2 · The program window → &nbsp; </kbd>](02-overview.md)
