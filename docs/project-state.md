# Project state

Current focus: the first consumer (Tomba! 2 through psxport) in a real window.

| Item | Status | Evidence / gap |
|---|---|---|
| Report folder lifecycle (open, attach, commit, discard, prune) | verified | `tests/test_bug_report.cpp` |
| `report.json` and `README.md` renderings | verified | `tests/test_bug_report.cpp` |
| Form: load, picture paths, Save/Cancel/keys, rejection | verified | `tests/test_bug_report.cpp` (null renderer and font engine) |
| Programmatic fill for keyboardless drivers (`Form::fill`) | verified | `tests/test_bug_report.cpp` |
| Marking where the bug shows (drag on a picture; fractions in `report.json`, spans in `README.md`) | verified | `tests/test_bug_report.cpp` (real mouse drag through the context, click-is-not-a-mark, Clear, commit refusals); not yet used in a real window |
| Form laid out at full width (styled scrollbar) | verified | `tests/test_bug_report.cpp` layout checks; reported collapsed in Tomba! 2 before the fix |
| First consumer: psxport / Tomba! 2 | partial | headless REPL report saved and replayed byte-identical (psxport `docs/project-state.md` S024); the form drawn over a real window is not yet run |
| Gamepad navigation of the form | missing | keyboard and mouse only |
| Hosted CI (Linux, Windows, macOS) | missing | no workflow yet |

## Comparison baseline

Before this library there was no in-app bug report: a player described a bug in chat, with a
screenshot taken by hand and no reproduction.
