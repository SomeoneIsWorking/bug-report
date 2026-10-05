# Project state

Current focus: first consumer, Tomba! 2 through psxport.

| Item | Status | Evidence / gap |
|---|---|---|
| Report folder lifecycle (open, attach, commit, discard, prune) | verified | `tests/test_bug_report.cpp` |
| `report.json` and `README.md` renderings | verified | `tests/test_bug_report.cpp` |
| Form: load, picture paths, Save/Cancel/keys, rejection | verified | `tests/test_bug_report.cpp` (null renderer and font engine) |
| Form drawn in a real product window | missing | first consumer (psxport/Tomba! 2) not yet wired |
| Gamepad navigation of the form | missing | keyboard and mouse only |
| Hosted CI (Linux, Windows, macOS) | missing | no workflow yet |

## Comparison baseline

Before this library there was no in-app bug report: a player described a bug in chat, with a
screenshot taken by hand and no reproduction.
