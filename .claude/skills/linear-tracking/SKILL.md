---
name: linear-tracking
description: Use when filing a Linear issue for this repo, choosing its Work Type label or priority, or setting a ticket's status.
---

# Linear tracking

Moved verbatim from AGENTS.md.

## Filing issues

- Every issue gets the `stopwatch-screensaver` label (in the `Repo` group) and exactly
  one label from the `Work Type` group. The group is single-select, so
  pick the one that fits best (see below).
- Always set a priority (Urgent, High, Medium, Low). Never leave it at
  "No priority".
- Link the draft trail or spec section if one exists. Copy the body from
  `entire trail show N --json`; `entire trail list --json` returns empty
  bodies.

## Choosing a Work Type

| Label         | Use it for                                       |
| ------------- | ------------------------------------------------ |
| `Bug`         | Fixing something that isn't operating correctly. |
| `Feature`     | Adding new functionality to the app.             |
| `Enhancement` | Improving existing functionality.                |
| `Performance` | Improving the performance of code.               |
| `Chore`       | Maintenance, tooling and documentation work.     |

Several of these can fit one ticket. Decide by asking these in order and
taking the first yes:

1. Does the app do something other than what was intended? `Bug`, even if
   the symptom is slowness or the fix is a refactor.
1. Is the behaviour right, but too slow or heavy, and is speed or
   resource use the point of the work? `Performance`.
1. Does it give the user a capability they did not have? `Feature`.
1. Does it make a capability they already have better? `Enhancement`.
1. Does it change nothing a user would notice? `Chore`.

A ticket that bundles small, unrelated tidy-ups takes `Chore`. If the
bundle is mostly one other type, either split it or use that type.
Refactors, test additions, dependency bumps, build scripts, CI and
documentation are `Chore` unless they fix a defect.

## Status names

Use these exactly as Linear spells them: `Backlog`, `Todo`,
`In Progress`, `In Review`, `Done`, `Canceled`, `Duplicate`.
