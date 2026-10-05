# Codemap

bug-report is one subsystem: the on-disk bug report and its in-window editor. Consumers own
capture, freezing, input routing, rendering and storage location.

| Responsibility | Where | Notes |
|---|---|---|
| Report content and its two renderings | `include/bug_report/report.h`, `src/report.cpp` | `Report`, `Attachment`, `Fact`, `Reproduction`, `PlayerText`; `toJson` (with its structural `JsonWriter`) and `toMarkdown`. One layout, `kFormat` = `bug-report/1`. |
| Report folder lifecycle | `include/bug_report/draft.h`, `src/draft.cpp` | `Draft`: open (`<stamp>.draft` + marker), attachment names, commit (write, rename to `<stamp>-<slug>`), discard, `pruneAbandoned`. Removes only directories carrying its own marker. |
| In-window editor | `include/bug_report/form.h`, `src/form.cpp` | `Form`: embedded RML/RCSS, modal document in the consumer's context, Save/Cancel/key handling, the player's text read back. |
| Tests | `tests/test_bug_report.cpp` | Draft lifecycle and refusals, both renderings, the form in a real RmlUi context. |
| Verifier | `tools/verify.py` | clang-format, Clang build, clang-tidy, CTest. |
