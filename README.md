# bug-report

An in-app bug report for RmlUi C++ applications. The player presses a key, the application
freezes and captures what it knows, and a form inside the application's own window lets the player
describe the bug. The report is saved as a folder anyone (or any agent) can read:

```text
<root>/20261005-142233-tomba-falls-through-the-floor/
  report.json      machine-readable report (format "bug-report/1")
  README.md        the same report for a person
  screen.png ...   whatever the application attached: pictures, input recordings, saves
```

- **`Draft`** owns one report's folder from the moment it is opened until it is committed
  (renamed to its final name) or discarded (removed). The application writes its captures into
  `draft.pathFor(name)` and records them with `attach`.
- **`Form`** is a modal RmlUi document loaded into the application's *own* context. It shows every
  picture attachment side by side, lists the rest, and collects a summary and description.
- **The application owns** the hotkey, freezing, what to capture, the render interface (which must
  decode the pictures it attaches through `LoadTexture`), fonts, and where `<root>` is.

## Consuming

CMake target `bug_report::bug_report` (C++20). Add it after your own RmlUi so it links
`RmlUi::Core` from the same build. Standard RmlUi version: **6.3**. Built on its own (for its
tests) it takes RmlUi from `BUG_REPORT_RMLUI_DIR` or a sibling `../RmlUi` / `../../RmlUi`.

```cpp
bug_report::Draft::pruneAbandoned(root);                 // once, at startup

std::string error;
auto draft = bug_report::Draft::open(root, "My Game", std::chrono::system_clock::now(), error);
write_png(*draft->pathFor("screen.png", error), pixels);
draft->attach("screen.png", bug_report::AttachmentRole::Screenshot, "What you saw", error);
draft->addFact("frame", std::to_string(frame));
draft->setReproduction({"Replay the recorded input", {}, "my-game --replay repro.pad"});

bug_report::Form form(*context, *draft, {.fontFamily = "Fira Sans"});
// each frame, while frozen: feed events, update and render the context, then:
switch (form.outcome()) {
case bug_report::Form::Outcome::SaveRequested:
  if (auto saved = draft->commit(form.text(), error)) { /* done: *saved */ }
  else { form.rejectSave(error); }
  break;
case bug_report::Form::Outcome::Cancelled:
  draft->discard();
  break;
case bug_report::Form::Outcome::Editing:
  break;
}
```

Keys in the form: Escape cancels, Ctrl+Enter saves, Enter in the summary moves to the description.

## Verifying

`python3 tools/verify.py` — clang-format check, Clang build, clang-tidy, CTest.
