[<kbd> &nbsp; ← Overview &nbsp; </kbd>](../../README.en.md)
[<kbd> &nbsp; ← Back: 8 · Relationships &nbsp; </kbd>](08-connections.md)
[<kbd> &nbsp; Next: 10 · Files, images & export → &nbsp; </kbd>](10-files.md)

---

# 📜 Chapter 9 · Chronicle & detail panel

> **What is this about?** The two windows for *looking things up*: the **story
> visualizer**, which tells the story as a chronicle, and the **Details** panel, which
> shows everything about a single thing.

---

# Part 1 · The Details panel

The window on the right. It always shows whatever you clicked last – no matter where.

![Details panel](../bilder/details.png)

## 9.1 For an element

```
 ┌───────────────────────────────────────────────┐
 │ ● Alice                                       │ ← name + group colour
 │ Characters/Main/Alice                         │ ← where it lives
 │ [Edit][Duplicate][Add action]                 │
 ├───────────────────────────────────────────────┤
 │ ▼ Fields                                      │
 │   name *                                      │ ← * = required
 │   [ Alice                                   ] │
 │                                               │
 │   age                                         │
 │   [ 27 ]  [ − ] [ + ]  (?)                    │
 │   • Day 11, 08:00: 27 → 28                    │ ← value history!
 │                                               │
 │   description                                 │
 │   [ A brave knight with a troubled past…    ] │
 │                                               │
 │   status                                      │
 │   [ Alive                                 ▼ ] │
 │                                               │
 │   [ + Own field ]                             │
 │ ▶ Free text                                   │
 │ ▼ Relationships                               │
 │   married_to   [Bob] [Edit]                   │
 │   [ + Connection ]                            │
 │ ▼ Mentioned in actions                        │
 │   Day 1, 09:00  Alice returns                 │
 │   Day 5, 14:00  Alice finds the sword         │
 └───────────────────────────────────────────────┘
```

| Section | What is in it |
|:--|:--|
| **Fields** | All values, directly editable. Under each one the **history**: which action changed this value and when |
| **Free text** | A plain text field outside the template – for notes |
| **Relationships** | All connections of this element, clickable |
| **Mentioned in actions** | All events with this element, clickable |

> 💡 The **value history** under a field is the fastest answer to
> *"when did that actually change?"* – clicking it jumps to the action.

## 9.2 For an action, group or connection

The same window shows, depending on the selection:

| Selection | Contents |
|:--|:--|
| ⚡ **Action** | Title, time, description, participants, type, tags, changes, attachments |
| 📁 **Group** | Name, colour, template, number of elements |
| 🔗 **Connection** | Type, roles with elements, period, description |

---

# Part 2 · The story visualizer

**Windows → Story visualizer**

A **read-only view**: here the story is told, not edited.

## 9.3 How it reads

```
 ╔═══════════════════════════════════════════════════════════╗
 ║  Read-only view: here the story is told, not edited.      ║
 ╚═══════════════════════════════════════════════════════════╝

 ── DAY 1 ─────────────────────────────────────────────────────
   → Alice returns
     Alice comes back to her home town after years.
     Involved: Alice · Castle

   → Alice meets Bob at the market
     Involved: Alice · Bob

 ── 4 DAYS PASS ───────────────────────────────────────────────

 ── DAY 5 ─────────────────────────────────────────────────────
   → Alice finds the sword
     Alice finds an ancient sword in a ruin.
     • Alice.power_level: 1 → 3            ← value change

 ── 6 DAYS PASS ───────────────────────────────────────────────

 ── DAY 11 ────────────────────────────────────────────────────
   → Alice's birthday
     • Alice.age: 27 → 28
```

| Element | Meaning |
|:--|:--|
| **── DAY n ──** | A day that has events |
| **── n DAYS PASS ──** | A gap without events |
| **→ Title** | An event |
| **• field: old → new** | A value change caused by this event |

## 9.4 The filters at the top

| Filter | Effect |
|:--|:--|
| **Focus** | Only events with a particular element. Everything else is folded into "… pass" |
| **Strand** | Show only one plot strand, e.g. only the main quest |
| **☑ important only** | Only events marked as *important* |
| **☑ full text** | The whole description instead of the first lines |
| **☑ value changes** | Show or hide the `• field: old → new` lines |

If a filter removes everything, it honestly says
*"No events for this selection."*

## 9.5 What people use it for

| Question | How to answer it |
|:--|:--|
| *What does Alice actually go through?* | **Focus** on Alice – her whole story in one column |
| *Where are my holes?* | Look at the "… pass" blocks |
| *How does a character develop?* | **Value changes** on, **focus** on that character |
| *Does the side plot carry?* | Switch **strand** |

---

## 9.6 Three views on the same story

| | Made for | Editable? |
|:--|:--|:--|
| ✍️ **Manuscript** | Writing the text | yes |
| 📅 **Timeline** | Seeing time relations | yes |
| 📜 **Story visualizer** | Reading the plot in one go | no |

---

## ✅ In short

- **Details** always shows the last thing you clicked – including a **value history** per field
- **Story visualizer** narrates the story chronologically, with gaps as "… pass"
- **Focus** on one character gives you their complete biography
- The visualizer is deliberately read-only

---

[<kbd> &nbsp; ← Overview &nbsp; </kbd>](../../README.en.md)
[<kbd> &nbsp; ← Back: 8 · Relationships &nbsp; </kbd>](08-connections.md)
[<kbd> &nbsp; Next: 10 · Files, images & export → &nbsp; </kbd>](10-files.md)
