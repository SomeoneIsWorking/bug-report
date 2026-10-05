#include "bug_report/form.h"

#include "bug_report/draft.h"

#include <RmlUi/Core.h>

#include <algorithm>
#include <cmath>
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
/* RmlUi draws a scrollbar only from these rules; an overflow box whose scrollbar has no styled size
   gives its content no width at all. */
scrollbarvertical {
  width: 10dp;
}
scrollbarvertical slidertrack {
  background-color: #00000040;
}
scrollbarvertical sliderbar {
  background-color: #4b4e5c;
  border-radius: 4dp;
}
scrollbarvertical sliderarrowdec, scrollbarvertical sliderarrowinc {
  height: 0;
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
.frame {
  display: block;
  position: relative;
  border: 1dp #4b4e5c;
  cursor: crosshair;
}
.frame img {
  display: block;
  width: 100%;
}
.mark {
  position: absolute;
  border: 2dp #ff5050;
  background-color: #ff505033;
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
    rml += "<div class=\"picture\"><div class=\"frame\"><img src=\"";
    rml += escapeRml(attachment.file);
    rml += "\"/></div><span class=\"caption\">";
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
  rml +=
      "<span id=\"error\"></span>"
      "<div id=\"actions\"><span id=\"hint\">Drag on a picture to mark the bug &#183; Esc cancel "
      "&#183; Ctrl+Enter save</span><button id=\"clear\">Clear marks</button>"
      "<button id=\"cancel\">Cancel</button><button id=\"save\">Save</button></div>"
      "</div></body></rml>";
  return rml;
}

// The nearest picture frame at or above `element`, or null.
Rml::Element *frameOf(Rml::Element *element) {
  for (; element != nullptr; element = element->GetParentNode()) {
    if (element->IsClassSet("frame")) {
      return element;
    }
  }
  return nullptr;
}

double clamp01(double value) {
  return value < 0 ? 0 : (value > 1 ? 1 : value);
}

// The id of the nearest element at or above `element` that has one of the action ids.
std::string actionOf(Rml::Element *element) {
  for (; element != nullptr; element = element->GetParentNode()) {
    const Rml::String &id = element->GetId();
    if (id == "save" || id == "cancel" || id == "clear") {
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
    for (const Rml::EventId id : kEvents) {
      mDocument->AddEventListener(id, this, id == Rml::EventId::Keydown);
    }
    // The frames were written in attachment order, one per picture, so each one's picture is known.
    Rml::ElementList frames;
    mDocument->GetElementsByClassName(frames, "frame");
    std::size_t next = 0;
    for (const Attachment &attachment : draft.report().attachments) {
      if (isPicture(attachment.role) && next < frames.size()) {
        mFrames.push_back({frames[next++], attachment.file});
      }
    }
    mDocument->Show(Rml::ModalFlag::Modal, Rml::FocusFlag::Document);
    if (Rml::Element *summary = mDocument->GetElementById("summary")) {
      summary->Focus();
    }
  }

  ~Impl() override {
    if (mDocument == nullptr) {
      return;
    }
    for (const Rml::EventId id : kEvents) {
      mDocument->RemoveEventListener(id, this, id == Rml::EventId::Keydown);
    }
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
    switch (event.GetId()) {
    case Rml::EventId::Mousedown:
      beginMark(event);
      return;
    case Rml::EventId::Mousemove:
      dragMark(event);
      return;
    case Rml::EventId::Mouseup:
      endMark(event);
      return;
    case Rml::EventId::Click: {
      const std::string action = actionOf(event.GetTargetElement());
      if (action == "save") {
        mOutcome = Outcome::SaveRequested;
      } else if (action == "cancel") {
        mOutcome = Outcome::Cancelled;
      } else if (action == "clear") {
        clearMarks();
      }
      return;
    }
    default:
      break;
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
  const std::vector<Mark> &marks() const {
    return mMarks;
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
  static constexpr Rml::EventId kEvents[] = {Rml::EventId::Click, Rml::EventId::Keydown,
                                             Rml::EventId::Mousedown, Rml::EventId::Mousemove,
                                             Rml::EventId::Mouseup};
  // Below this fraction of a picture's width or height a press is a click, not a mark.
  static constexpr double kMinimumMark = 0.01;

  // A picture's frame in the document and the attachment it shows.
  struct PictureFrame {
    Rml::Element *frame;
    std::string file;
  };

  // A region being dragged out on one picture frame; a null frame is no drag.
  struct Drag {
    Rml::Element *frame = nullptr;
    Rml::Element *box = nullptr;
    double startX = 0;
    double startY = 0;
    Mark mark;
  };

  // The pointer as fractions of the frame's content box (which the picture fills exactly).
  static Rml::Vector2f pointerIn(Rml::Element &frame, const Rml::Event &event) {
    const Rml::Vector2f origin = frame.GetAbsoluteOffset(Rml::BoxArea::Content);
    const Rml::Vector2f size = frame.GetBox().GetSize(Rml::BoxArea::Content);
    const Rml::Vector2f pointer{static_cast<float>(event.GetParameter<int>("mouse_x", 0)),
                                static_cast<float>(event.GetParameter<int>("mouse_y", 0))};
    if (size.x <= 0 || size.y <= 0) {
      return {0, 0};
    }
    return {(pointer.x - origin.x) / size.x, (pointer.y - origin.y) / size.y};
  }

  void beginMark(const Rml::Event &event) {
    Rml::Element *frame = frameOf(event.GetTargetElement());
    if (frame == nullptr || event.GetParameter<int>("button", 0) != 0) {
      return;
    }
    const Rml::Vector2f at = pointerIn(*frame, event);
    mDrag = Drag{frame, newBox(*frame), clamp01(at.x), clamp01(at.y),
                 Mark{pictureOf(frame), 0, 0, 0, 0}};
    place(mDrag);
  }

  void dragMark(const Rml::Event &event) {
    if (mDrag.frame == nullptr) {
      return;
    }
    const Rml::Vector2f at = pointerIn(*mDrag.frame, event);
    const double x = clamp01(at.x);
    const double y = clamp01(at.y);
    mDrag.mark.x = std::min(mDrag.startX, x);
    mDrag.mark.y = std::min(mDrag.startY, y);
    mDrag.mark.width = std::abs(x - mDrag.startX);
    mDrag.mark.height = std::abs(y - mDrag.startY);
    place(mDrag);
  }

  void endMark(const Rml::Event &event) {
    if (mDrag.frame == nullptr) {
      return;
    }
    dragMark(event);
    if (mDrag.mark.width < kMinimumMark || mDrag.mark.height < kMinimumMark) {
      mDrag.frame->RemoveChild(mDrag.box);
    } else {
      mMarks.push_back(mDrag.mark);
      mBoxes.push_back(mDrag.box);
    }
    mDrag = Drag{};
  }

  static void place(const Drag &drag) {
    const auto percentOf = [](double fraction) {
      return Rml::Property(static_cast<float>(fraction * 100.0), Rml::Unit::PERCENT);
    };
    drag.box->SetProperty(Rml::PropertyId::Left, percentOf(drag.mark.x));
    drag.box->SetProperty(Rml::PropertyId::Top, percentOf(drag.mark.y));
    drag.box->SetProperty(Rml::PropertyId::Width, percentOf(drag.mark.width));
    drag.box->SetProperty(Rml::PropertyId::Height, percentOf(drag.mark.height));
  }

  std::string pictureOf(const Rml::Element *frame) const {
    for (const PictureFrame &shown : mFrames) {
      if (shown.frame == frame) {
        return shown.file;
      }
    }
    return {};
  }

  Rml::Element *newBox(Rml::Element &frame) {
    Rml::ElementPtr box = mDocument->CreateElement("div");
    box->SetClass("mark", true);
    return frame.AppendChild(std::move(box));
  }

public:
  // Replace the marks, drawing each on the frame of the picture it names. A mark naming no shown
  // picture is kept (the commit refuses it by name) but has nothing to be drawn on.
  void setMarks(const std::vector<Mark> &marks) {
    if (mDocument == nullptr) {
      return;
    }
    clearMarks();
    for (const Mark &mark : marks) {
      mMarks.push_back(mark);
      for (const PictureFrame &shown : mFrames) {
        if (shown.file == mark.file) {
          const Drag drawn{shown.frame, newBox(*shown.frame), 0, 0, mark};
          place(drawn);
          mBoxes.push_back(drawn.box);
        }
      }
    }
  }

private:
  void clearMarks() {
    for (Rml::Element *box : mBoxes) {
      box->GetParentNode()->RemoveChild(box);
    }
    mBoxes.clear();
    mMarks.clear();
  }

  std::string focusedId() const {
    Rml::Context *context = mDocument->GetContext();
    Rml::Element *focus = context != nullptr ? context->GetFocusElement() : nullptr;
    return focus != nullptr ? focus->GetId() : std::string();
  }

  Rml::ElementDocument *mDocument = nullptr;
  Outcome mOutcome = Outcome::Editing;
  std::vector<PictureFrame> mFrames;
  Drag mDrag; // frame is null while nothing is being dragged
  std::vector<Mark> mMarks;
  std::vector<Rml::Element *> mBoxes; // every drawn mark box, removed by clearMarks
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
  return PlayerText{mImpl->valueOf("summary"), mImpl->valueOf("description"), mImpl->marks()};
}

void Form::fill(const PlayerText &text) {
  mImpl->setValue("summary", text.summary);
  mImpl->setValue("description", text.description);
  mImpl->setMarks(text.marks);
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
