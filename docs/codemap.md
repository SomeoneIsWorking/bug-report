# Codemap

bug-report is the in-window editor of a bug report; the report, its folder and its renderings
are Lucent's `lucent/bug_report.h`. Consumers own
capture, freezing, input routing, rendering and storage location.

| Responsibility | Where | Notes |
|---|---|---|
| In-window editor | `include/bug_report/form.h`, `src/form.cpp` | `Form`: embedded RML/RCSS, modal document in the consumer's context, Save/Cancel/key handling, the player's text read back. |
| Tests | `tests/test_bug_report.cpp` | The form in a real RmlUi context; the draft and renderings are tested in Lucent. |
| Verifier | `tools/verify.py` | clang-format, Clang build, clang-tidy, CTest. |
