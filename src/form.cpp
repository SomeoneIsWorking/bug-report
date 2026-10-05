#include "bug_report/form.h"

#include "bug_report/draft.h"

#include <RmlUi/Core.h>

#include <string>
#include <vector>

namespace bug_report {

namespace {

// Text and attribute values carried into RML. The player's own text never goes through here: it
// lives in form controls and is read back with GetValue.
std::string escapeRml(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (const char ch : text) {
    switch (ch) {
    case '&':
      out += "&amp;";
      break;
    case '<':
      out += "&lt;";
      break;
    case '>':
      out += "&gt;";
      break;
    case '"':
      out += "&quot;";
      break;
    case '\'':
      out += "&#39;";
      break;
    default:
      out += ch;
      break;
    }
  }
  return out;
}

constexpr std::string_view kStyle = R"(
body {
  width: 100%;
  height: 100%;
  background-color: #000000b8;
  color: #e9e9ee;
  font-size: 16dp;
}
#panel {
  position: absolute;
  left: 3%;
  right: 3%;
  top: 3%;
  bottom: 3%;
  padding: 16dp 20dp;
  background-color: #1b1c21f4;
  border: 1dp #4b4e5c;
  border-radius: 8dp;
  overflow-y: auto;
}
h1 {
  display: block;
  font-size: 22dp;
  font-weight: bold;
  margin-bottom: 12dp;
}
#pictures {
  display: flex;
  flex-direction: row;
  flex-wrap: wrap;
  justify-content: space-between;
}
.picture {
  display: block;
  width: 49%;
  margin-bottom: 10dp;
}
.picture img {
  display: block;
  width: 100%;
  border: 1dp #4b4e5c;
}
.caption {
  display: block;
  color: #a9abb8;
  font-size: 14dp;
  margin-top: 4dp;
}
label {
  display: block;
  color: #a9abb8;
  font-size: 14dp;
  margin-top: 10dp;
  margin-bottom: 4dp;
}
input.text, textarea {
  display: block;
  box-sizing: border-box;
  width: 100%;
  padding: 6dp 8dp;
  background-color: #0e0f13;
  border: 1dp #565a69;
  border-radius: 4dp;
  color: #f2f2f6;
}
input.text:focus, textarea:focus {
  border-color: #7fa8ff;
}
textarea {
  height: 7.5em;
}
#attached {
  display: block;
  color: #a9abb8;
  font-size: 14dp;
  margin-top: 10dp;
}
#error {
  display: block;
  color: #ff8a80;
  margin-top: 8dp;
}
#actions {
  display: flex;
  flex-direction: row;
  justify-content: flex-end;
  align-items: center;
  margin-top: 12dp;
}
#hint {
  color: #8b8d99;
  font-size: 14dp;
  margin-right: 16dp;
}
button {
  display: inline-block;
  padding: 7dp 18dp;
  margin-left: 8dp;
  border-radius: 4dp;
  background-color: #33353f;
  color: #f2f2f6;
  cursor: pointer;
}
button:hover, button:focus {
  background-color: #454857;
}
button#save {
  background-color: #2f66d6;
}
button#save:hover, button#save:focus {
  background-color: #4078ea;
}
)";

std::string buildDocument(const Report &report, const FormOptions &options) {
  std::string rml = "<rml><head><title>";
  rml += escapeRml(options.heading);
  rml += "</title><style>";
  rml += kStyle;
  rml += "body { font-family: \"";
  rml += escapeRml(options.fontFamily);
  rml += "\"; }</style></head><body><div id=\"panel\"><h1>";
  rml += escapeRml(options.heading);
  rml += "</h1><div id=\"pictures\">";
  std::vector<const Attachment *> others;
  for (const Attachment &attachment : report.attachments) {
    if (!isPicture(attachment.role)) {
      others.push_back(&attachment);
      continue;
    }
    rml += "<div class=\"picture\"><img src=\"";
    rml += escapeRml(attachment.file);
    rml += "\"/><span class=\"caption\">";
    rml += escapeRml(attachment.label);
    rml += "</span></div>";
  }
  rml += "</div>"
         "<label for=\"summary\">Summary</label>"
         "<input type=\"text\" class=\"text\" id=\"summary\" maxlength=\"200\"/>"
         "<label for=\"description\">What happened, and what you expected</label>"
         "<textarea id=\"description\"></textarea>";
  if (!others.empty()) {
    rml += "<span id=\"attached\">Also attached: ";
    for (std::size_t i = 0; i < others.size(); ++i) {
      if (i != 0) {
        rml += ", ";
      }
      rml += escapeRml(others[i]->label.empty() ? others[i]->file : others[i]->label);
    }
    rml += "</span>";
  }
  rml += "<span id=\"error\"></span>"
         "<div id=\"actions\"><span id=\"hint\">Esc cancel &#183; Ctrl+Enter save</span>"
         "<button id=\"cancel\">Cancel</button><button id=\"save\">Save</button></div>"
         "</div></body></rml>";
  return rml;
}

// The id of the nearest element at or above `element` that has one of the action ids.
std::string actionOf(Rml::Element *element) {
  for (; element != nullptr; element = element->GetParentNode()) {
    const Rml::String &id = element->GetId();
    if (id == "save" || id == "cancel") {
      return id;
    }
  }
  return {};
}

} // namespace

class Form::Impl final : public Rml::EventListener {
public:
  Impl(Rml::Context &context, const Draft &draft, const FormOptions &options) {
    // The document's URL sits inside the draft directory, so each picture's relative src resolves
    // to the file the application wrote there (RmlUi joins src onto the document's directory).
    const std::string url = (draft.directory() / "bug-report.rml").generic_string();
    mDocument = context.LoadDocumentFromMemory(buildDocument(draft.report(), options), url);
    if (mDocument == nullptr) {
      mOutcome = Outcome::Cancelled;
      return;
    }
    mDocument->AddEventListener(Rml::EventId::Click, this);
    mDocument->AddEventListener(Rml::EventId::Keydown, this, true);
    mDocument->Show(Rml::ModalFlag::Modal, Rml::FocusFlag::Document);
    if (Rml::Element *summary = mDocument->GetElementById("summary")) {
      summary->Focus();
    }
  }

  ~Impl() override {
    if (mDocument == nullptr) {
      return;
    }
    mDocument->RemoveEventListener(Rml::EventId::Click, this);
    mDocument->RemoveEventListener(Rml::EventId::Keydown, this, true);
    mDocument->Close();
  }

  Impl(const Impl &) = delete;
  Impl &operator=(const Impl &) = delete;
  Impl(Impl &&) = delete;
  Impl &operator=(Impl &&) = delete;

  void ProcessEvent(Rml::Event &event) override {
    if (mOutcome != Outcome::Editing) {
      return;
    }
    if (event.GetId() == Rml::EventId::Click) {
      const std::string action = actionOf(event.GetTargetElement());
      if (action == "save") {
        mOutcome = Outcome::SaveRequested;
      } else if (action == "cancel") {
        mOutcome = Outcome::Cancelled;
      }
      return;
    }
    const auto key = static_cast<Rml::Input::KeyIdentifier>(
        event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN));
    const bool ctrl = event.GetParameter<int>("ctrl_key", 0) != 0;
    if (key == Rml::Input::KI_ESCAPE) {
      mOutcome = Outcome::Cancelled;
      event.StopPropagation();
    } else if (key == Rml::Input::KI_RETURN && ctrl) {
      mOutcome = Outcome::SaveRequested;
      event.StopPropagation();
    } else if (key == Rml::Input::KI_RETURN && focusedId() == "summary") {
      if (Rml::Element *description = mDocument->GetElementById("description")) {
        description->Focus();
      }
      event.StopPropagation();
    }
  }

  bool loaded() const {
    return mDocument != nullptr;
  }
  Outcome outcome() const {
    return mOutcome;
  }
  void setOutcome(Outcome outcome) {
    if (mDocument != nullptr && mOutcome == Outcome::Editing) {
      mOutcome = outcome;
    }
  }

  std::string valueOf(const char *id) const {
    if (mDocument == nullptr) {
      return {};
    }
    auto *control = rmlui_dynamic_cast<Rml::ElementFormControl *>(mDocument->GetElementById(id));
    return control != nullptr ? control->GetValue() : std::string();
  }

  void setValue(const char *id, const std::string &value) {
    if (mDocument == nullptr) {
      return;
    }
    if (auto *control =
            rmlui_dynamic_cast<Rml::ElementFormControl *>(mDocument->GetElementById(id))) {
      control->SetValue(value);
    }
  }

  void rejectSave(std::string_view reason) {
    if (mDocument == nullptr) {
      return;
    }
    if (Rml::Element *error = mDocument->GetElementById("error")) {
      error->SetInnerRML(escapeRml(reason));
    }
    mOutcome = Outcome::Editing;
  }

private:
  std::string focusedId() const {
    Rml::Context *context = mDocument->GetContext();
    Rml::Element *focus = context != nullptr ? context->GetFocusElement() : nullptr;
    return focus != nullptr ? focus->GetId() : std::string();
  }

  Rml::ElementDocument *mDocument = nullptr;
  Outcome mOutcome = Outcome::Editing;
};

Form::Form(Rml::Context &context, const Draft &draft, const FormOptions &options)
    : mImpl(std::make_unique<Impl>(context, draft, options)) {
}

Form::~Form() = default;

bool Form::loaded() const {
  return mImpl->loaded();
}

Form::Outcome Form::outcome() const {
  return mImpl->outcome();
}

PlayerText Form::text() const {
  return PlayerText{mImpl->valueOf("summary"), mImpl->valueOf("description")};
}

void Form::fill(const PlayerText &text) {
  mImpl->setValue("summary", text.summary);
  mImpl->setValue("description", text.description);
}

void Form::rejectSave(std::string_view reason) {
  mImpl->rejectSave(reason);
}

void Form::requestSave() {
  mImpl->setOutcome(Outcome::SaveRequested);
}

void Form::requestCancel() {
  mImpl->setOutcome(Outcome::Cancelled);
}

} // namespace bug_report
