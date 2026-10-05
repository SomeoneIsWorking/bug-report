// bug_report::Form — the in-window editor for one Draft, as an RmlUi document.
//
// The form lives in the CONSUMER's RmlUi context: the application keeps its own render interface,
// system interface, fonts, event routing and frame loop, and this class only loads, drives and
// unloads one modal document. It shows every picture attachment (Screenshot / Reference) side by
// side, lists the other attachments, and takes a one-line summary and a free-text description.
//
// Pictures are loaded through the context's render interface by file path, so that interface's
// `LoadTexture` must decode the image format the application wrote (PNG in practice).
//
// Keys: Escape cancels; Ctrl+Enter saves (Enter in the summary moves to the description).
// Mouse: dragging on a picture marks the region where the bug shows (several are allowed);
// "Clear marks" removes them. Marks come back in text() with the typed words.
#pragma once

#include "bug_report/report.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace Rml {
class Context;
} // namespace Rml

namespace bug_report {

class Draft;

struct FormOptions {
  std::string fontFamily;               // a family the consumer already loaded into RmlUi
  std::string heading = "Report a bug"; // the form's title line
};

class Form {
public:
  enum class Outcome : std::uint8_t {
    Editing,       // the player is still writing
    SaveRequested, // the player pressed Save; the consumer commits the draft
    Cancelled,     // the player cancelled; the consumer discards the draft
  };

  // Load and show the form for `draft`, focused on the summary. The draft must stay open and the
  // context must stay alive while the form exists; destroying the form closes its document. When
  // the document cannot be loaded, loaded() is false and outcome() is Cancelled, so a consumer's
  // ordinary cancel path cleans up.
  Form(Rml::Context &context, const Draft &draft, const FormOptions &options);
  ~Form();
  Form(const Form &) = delete;
  Form &operator=(const Form &) = delete;

  bool loaded() const;
  Outcome outcome() const;
  PlayerText text() const;

  // A save the consumer could not complete (an empty summary, a disk error): show why and return
  // to Editing so the player can fix it.
  void rejectSave(std::string_view reason);

  // Replace what the player typed and marked, for keyboardless drivers (a control channel, a test).
  void fill(const PlayerText &text);

  // Programmatic Save / Cancel, for keyboardless drivers (a control channel, a test).
  void requestSave();
  void requestCancel();

private:
  class Impl;
  std::unique_ptr<Impl> mImpl;
};

} // namespace bug_report
