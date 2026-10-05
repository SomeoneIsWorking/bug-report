#include "bug_report/draft.h"
#include "bug_report/form.h"
#include "bug_report/report.h"

#include <RmlUi/Core.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

// Exercises the shipping draft lifecycle, the report renderings, and the form inside a real RmlUi
// context: the outcome the player's Save / Cancel / keys produce, and the values read back.
namespace {

namespace fs = std::filesystem;

int g_failures = 0;

#define CHECK(condition)                                                                           \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #condition);                    \
      ++g_failures;                                                                                \
    }                                                                                              \
  } while (0)

fs::path makeRoot(const char *name) {
  const fs::path root = fs::current_path() / "test-output" / name;
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root, ec);
  return root;
}

void writeFile(const fs::path &path, const std::string &contents) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
}

std::string readFile(const fs::path &path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

std::size_t entriesIn(const fs::path &root) {
  return static_cast<std::size_t>(
      std::distance(fs::directory_iterator(root), fs::directory_iterator()));
}

std::chrono::system_clock::time_point fixedNow() {
  return std::chrono::system_clock::time_point(std::chrono::seconds(1790000000));
}

// Where `file` goes in `draft`; a refused name fails the test and yields a path nothing writes to.
fs::path attachmentPath(const bug_report::Draft &draft, const char *file) {
  std::string error;
  const std::optional<fs::path> path = draft.pathFor(file, error);
  CHECK(path.has_value());
  return path.value_or(fs::path());
}

std::optional<bug_report::Draft> openDraft(const fs::path &root) {
  std::string error;
  std::optional<bug_report::Draft> draft =
      bug_report::Draft::open(root, "Test Title", fixedNow(), error);
  CHECK(draft.has_value());
  CHECK(error.empty());
  return draft;
}

void testCommitWritesReportAndRenames() {
  const fs::path root = makeRoot("commit");
  std::optional<bug_report::Draft> draft = openDraft(root);
  if (!draft) {
    return;
  }
  CHECK(draft->directory().filename().string().ends_with(".draft"));

  std::string error;
  writeFile(attachmentPath(*draft, "screen.png"), "png");
  CHECK(draft->attach("screen.png", bug_report::AttachmentRole::Screenshot, "Native", error));
  draft->addFact("frame", "1234");
  draft->setReproduction({"Replay the pad", {"step one"}, "replay \"repro.pad\""});

  const std::optional<fs::path> committed =
      draft->commit({"Tomba falls | through the floor", "line one\nline \"two\""}, error);
  CHECK(committed.has_value());
  CHECK(!draft->isOpen());
  if (!committed) {
    return;
  }
  CHECK(committed->filename() == "20260921-141320-tomba-falls-through-the-floor");
  CHECK(fs::is_regular_file(*committed / "screen.png"));
  CHECK(!fs::exists(*committed / ".bug-report-draft"));

  const std::string json = readFile(*committed / bug_report::kReportFileName);
  CHECK(json.find("\"format\": \"bug-report/1\"") != std::string::npos);
  CHECK(json.find("\"created\": \"2026-09-21T14:13:20Z\"") != std::string::npos);
  CHECK(json.find("\"description\": \"line one\\nline \\\"two\\\"\"") != std::string::npos);
  CHECK(json.find("\"role\": \"screenshot\"") != std::string::npos);
  CHECK(json.find("\"command\": \"replay \\\"repro.pad\\\"\"") != std::string::npos);

  const std::string readme = readFile(*committed / bug_report::kReadmeFileName);
  CHECK(readme.starts_with("# Tomba falls | through the floor\n"));
  CHECK(readme.find("![Native](screen.png)") != std::string::npos);
  CHECK(readme.find("| frame | 1234 |") != std::string::npos);
  CHECK(entriesIn(root) == 1);
}

void testRefusals() {
  const fs::path root = makeRoot("refusals");
  std::optional<bug_report::Draft> draft = openDraft(root);
  if (!draft) {
    return;
  }
  std::string error;
  CHECK(!draft->pathFor("../escape.png", error).has_value());
  CHECK(!draft->pathFor("sub/dir.png", error).has_value());
  CHECK(!draft->pathFor("..", error).has_value());
  CHECK(!draft->pathFor(bug_report::kReportFileName, error).has_value());
  CHECK(!draft->attach("missing.png", bug_report::AttachmentRole::Screenshot, "x", error));

  writeFile(attachmentPath(*draft, "a.pad"), "pad");
  CHECK(draft->attach("a.pad", bug_report::AttachmentRole::Reproduction, "pad", error));
  CHECK(!draft->attach("a.pad", bug_report::AttachmentRole::Reproduction, "pad", error));

  error.clear();
  CHECK(!draft->commit({"  \n", ""}, error).has_value());
  CHECK(!error.empty());
  CHECK(draft->isOpen());
}

void testDiscardAndStalePruning() {
  const fs::path root = makeRoot("discard");
  fs::path first;
  {
    std::optional<bug_report::Draft> draft = openDraft(root);
    if (!draft) {
      return;
    }
    first = draft->directory();
    CHECK(fs::is_directory(first));
  }
  CHECK(!fs::exists(first)); // destroyed uncommitted = discarded

  // A draft left by a process that died, and a look-alike this library did not create.
  fs::create_directories(root / "20200101-000000.draft");
  writeFile(root / "20200101-000000.draft" / ".bug-report-draft", "");
  fs::create_directories(root / "keep.draft");
  bug_report::Draft::pruneAbandoned(root);
  CHECK(!fs::exists(root / "20200101-000000.draft"));
  CHECK(fs::is_directory(root / "keep.draft"));

  // Opening a draft never prunes: another open draft in this process is not abandoned.
  std::optional<bug_report::Draft> draft = openDraft(root);

  // Two drafts in the same second do not share a directory.
  std::optional<bug_report::Draft> second = openDraft(root);
  if (draft && second) {
    CHECK(draft->directory() != second->directory());
  }
}

// RmlUi needs a render interface to create a context; nothing is drawn in these tests.
class NullRenderInterface final : public Rml::RenderInterface {
public:
  Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex>,
                                              Rml::Span<const int>) override {
    return 1;
  }
  void RenderGeometry(Rml::CompiledGeometryHandle, Rml::Vector2f, Rml::TextureHandle) override {
  }
  void ReleaseGeometry(Rml::CompiledGeometryHandle) override {
  }
  Rml::TextureHandle LoadTexture(Rml::Vector2i &dimensions, const Rml::String &source) override {
    lastTexture = source;
    dimensions = {4, 3};
    return 1;
  }
  Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>, Rml::Vector2i) override {
    return 1;
  }
  void ReleaseTexture(Rml::TextureHandle) override {
  }
  void EnableScissorRegion(bool) override {
  }
  void SetScissorRegion(Rml::Rectanglei) override {
  }

  Rml::String lastTexture;
};

void sendKey(Rml::Context &context, Rml::Input::KeyIdentifier key, int modifiers) {
  context.ProcessKeyDown(key, modifiers);
  context.ProcessKeyUp(key, modifiers);
}

void testForm(Rml::Context &context, NullRenderInterface &render) {
  const fs::path root = makeRoot("form");
  std::optional<bug_report::Draft> draft = openDraft(root);
  if (!draft) {
    return;
  }
  std::string error;
  writeFile(attachmentPath(*draft, "screen.png"), "png");
  CHECK(
      draft->attach("screen.png", bug_report::AttachmentRole::Screenshot, "Native <wide>", error));
  writeFile(attachmentPath(*draft, "repro.pad"), "pad");
  CHECK(draft->attach("repro.pad", bug_report::AttachmentRole::Reproduction, "Pad", error));

  bug_report::FormOptions options;
  options.fontFamily = "Test Sans";
  {
    bug_report::Form form(context, *draft, options);
    CHECK(form.loaded());
    CHECK(form.outcome() == bug_report::Form::Outcome::Editing);
    context.Update();
    context.Render();
    // The picture's relative src resolved inside the draft directory.
    CHECK(fs::path(render.lastTexture) == draft->directory() / "screen.png");

    Rml::ElementDocument *document = context.GetDocument(0);
    CHECK(document != nullptr);
    if (document != nullptr) {
      auto *summary =
          rmlui_dynamic_cast<Rml::ElementFormControl *>(document->GetElementById("summary"));
      auto *description =
          rmlui_dynamic_cast<Rml::ElementFormControl *>(document->GetElementById("description"));
      CHECK(summary != nullptr && description != nullptr);
      if (summary != nullptr && description != nullptr) {
        summary->SetValue("Floor clip");
        description->SetValue("Fell through");
      }
      const bug_report::PlayerText text = form.text();
      CHECK(text.summary == "Floor clip");
      CHECK(text.description == "Fell through");

      // Enter in the summary moves on; it does not save.
      sendKey(context, Rml::Input::KI_RETURN, 0);
      CHECK(form.outcome() == bug_report::Form::Outcome::Editing);
      sendKey(context, Rml::Input::KI_RETURN, Rml::Input::KM_CTRL);
      CHECK(form.outcome() == bug_report::Form::Outcome::SaveRequested);

      form.rejectSave("disk full");
      CHECK(form.outcome() == bug_report::Form::Outcome::Editing);
      Rml::Element *message = document->GetElementById("error");
      CHECK(message != nullptr && message->GetInnerRML() == "disk full");

      if (Rml::Element *cancel = document->GetElementById("cancel")) {
        cancel->Click();
      }
      CHECK(form.outcome() == bug_report::Form::Outcome::Cancelled);
    }
  }
  context.Update(); // the closed document unloads
  CHECK(context.GetNumDocuments() == 0);

  bug_report::Form escaped(context, *draft, options);
  sendKey(context, Rml::Input::KI_ESCAPE, 0);
  CHECK(escaped.outcome() == bug_report::Form::Outcome::Cancelled);

  bug_report::Form driven(context, *draft, options);
  driven.fill(bug_report::PlayerText{"Scripted", "From a control channel"});
  CHECK(driven.text().summary == "Scripted");
  CHECK(driven.text().description == "From a control channel");
  driven.requestSave();
  CHECK(driven.outcome() == bug_report::Form::Outcome::SaveRequested);
}

} // namespace

int main() {
  testCommitWritesReportAndRenames();
  testRefusals();
  testDiscardAndStalePruning();

  NullRenderInterface render;
  Rml::FontEngineInterface fonts; // no text is measured or drawn in these tests
  Rml::SetRenderInterface(&render);
  Rml::SetFontEngineInterface(&fonts);
  Rml::Initialise();
  Rml::Context *context = Rml::CreateContext("bug_report_test", Rml::Vector2i(1280, 720));
  CHECK(context != nullptr);
  if (context != nullptr) {
    testForm(*context, render);
  }
  Rml::Shutdown();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("bug_report_tests: all checks passed\n");
  return 0;
}
