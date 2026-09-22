[<kbd> &nbsp; ← Overview &nbsp; </kbd>](../../README.en.md)
[<kbd> &nbsp; ← Back: 4 · Characters, places, things &nbsp; </kbd>](04-groups.md)
[<kbd> &nbsp; Next: 6 · Events → &nbsp; </kbd>](06-actions.md)

---

# ⏳ Chapter 5 · How time works

> **The shortest and most important chapter.** If you read only one page, read this one.
> Without it a lot in the program looks arbitrary; with it everything makes sense.

---

## 5.1 The one rule

> ### ⏸️ Time does not move on by itself.
>
> It stays put until **you** move it.

Writing "many years later" into the text changes **nothing** for the program.
Only a **time marker** moves the clock.

```
   your text                                     what applies
 ┌───────────────────────────────────────┐    ┌─────────────┐
 │ #Day 1, 09:00                         │───▶│ Day 1, 09:00│
 │ Alice came back.                      │    │      ║      │
 │ Bob stood at the market.              │    │      ║      │
 │                                       │    │      ║      │
 │ Many years later he went to school.   │    │      ║  ←── changes nothing!
 │                                       │    │      ║      │
 │ #Day 11, 08:00                        │───▶│ Day 11,08:00│
 │ A year older she stood at the castle. │    │      ║      │
 └───────────────────────────────────────┘    └─────────────┘
```

---

## 5.2 Where you see when you are

The right-hand side of the manuscript bar always shows the point in time the story is
at **wherever your cursor is**:

```
                                    ┌──────────────────────┐
  … View  ?                         │ Time: Day 5, 14:00   │  ← clickable
                                    └──────────────────────┘
```

Click the cursor somewhere else in the text – the display changes with it.

---

## 5.3 Moving time on

Click the display (or **Insert → Move time on**):

```
┌─ Point in time ──────────────────────────────────────────┐
│                                                          │
│ Time does not move on by itself: it stays put until you  │
│ move it. From the marker on, the new point in time       │
│ applies to everything that follows in the text.          │
│                                                          │
│ Currently in effect here: Day 5, 14:00                   │
│ ──────────────────────────────────────────────────────── │
│ Move on by:                                              │
│   [ + 1 hour   ]  Day 5, 15:00      ← the result is      │
│   [ + 6 hours  ]  Day 5, 20:00         written next to   │
│   [ + 1 day    ]  Day 6, 14:00         it, so you never  │
│   [ + 3 days   ]  Day 8, 14:00         have to do the    │
│   [ + 1 week   ]  Day 12, 14:00        arithmetic        │
│   [ + 1 month  ]  Day 35, 14:00                          │
│ ──────────────────────────────────────────────────────── │
│ Or a fixed point in time:                                │
│   [ Day 12, 09:00       ]  [ Insert ]                    │
│   e.g. "Day 5, 14:00" or "Year 2, Month 3, Day 15"       │
└──────────────────────────────────────────────────────────┘
```

What gets inserted is a marker like `#Day 12, 09:00`, always alone on its own line.
It is invisible in the finished text.

---

## 5.4 Why this is so useful

Because you write it **once** and the program adapts the rest:

```
  Chapter 1                              Chapter 9
  #Day 1, 09:00                          #Day 400, 09:00
  "Alice was @Alice.age years old."      "Alice was @Alice.age years old."
                  │                                      │
                  ▼                                      ▼
            "was 27 years old"                    "was 28 years old"
```

Same sentence, same marker – two different numbers. If you later decide Alice should
start at 25, you change **one** value and the whole novel is consistent again.

How a value changes along the way is in
[Chapter 6 · Events & value changes](06-actions.md).

---

## 5.5 The calendar

The program works in a simple invented calendar – suitable for fantasy, science
fiction or anything that does not need a real one:

| | |
|:--|:--|
| 1 day | 24 hours |
| 1 month | 30 days |
| 1 year | 12 months = 360 days |
| Start | Day 1, 00:00 |

Internally everything is stored as **minutes since the start of the story**. That is
why you can freely mix notations.

### What you may type

| Input | Result |
|:--|:--|
| `Day 5, 14:30` | Day 5, 14:30 |
| `Day 5` | Day 5, 00:00 |
| `14:30` | Day 1, 14:30 |
| `Year 2, Month 3, Day 15` | = day 435 |
| `Tag 5, 14:30` | the same – German works too |

> 💡 Both languages are always accepted when **reading**. Switching the language
> therefore never breaks an existing project.

---

## 5.6 Relative points in time

Sometimes you do not know the date but you know the gap: *"two hours after Alice
arrives"*. Events may therefore refer to other events:

```
┌─ Edit action ──────────────────────────────────────┐
│ Title:     Bob talks about the dragon              │
│                                                    │
│ ☑ Relative to another action                       │
│   Reference: [ Alice meets Bob at the market ▼ ]   │
│   Offset:    [ 20 ] minutes after                  │
│                                                    │
│   → yields: Day 1, 11:20                           │
└────────────────────────────────────────────────────┘
```

If you later move the reference event, the dependent one moves along automatically.

---

## ✅ In short

- Time **stays put** until you move it on with a marker
- The right-hand side of the manuscript bar always says **when** you are
- Click it → move on, with quick steps and the arithmetic done for you
- Calendar: 30-day months, 360-day years, minutes internally
- Events may also sit **relative** to one another

---

[<kbd> &nbsp; ← Overview &nbsp; </kbd>](../../README.en.md)
[<kbd> &nbsp; ← Back: 4 · Characters, places, things &nbsp; </kbd>](04-groups.md)
[<kbd> &nbsp; Next: 6 · Events → &nbsp; </kbd>](06-actions.md)
