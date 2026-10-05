// bug_report::Draft — one report being written, and the directory that holds its files.
//
// Lifecycle: `open` creates `<root>/<timestamp>.draft/`; the application writes its captures into
// `pathFor(name)` and records them with `attach`; `commit` writes report.json and README.md and
// renames the directory to `<root>/<timestamp>-<slug>/`. A draft that is destroyed without being
// committed removes its directory, so a cancelled report leaves nothing behind. A draft directory
// left by a process that died mid-report is removed by `pruneAbandoned`, which the application
// calls once at startup, before it can have a draft of its own open.
#pragma once

#include "bug_report/report.h"

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace bug_report {

class Draft {
public:
  // Create a draft under `root` (created if missing). Returns nullopt and sets `error` on failure.
  static std::optional<Draft> open(const std::filesystem::path &root, std::string_view application,
                                   std::chrono::system_clock::time_point now, std::string &error);

  // Remove every draft directory this library created under `root`. Call only when no draft under
  // `root` can be open (application startup); a committed report is never touched.
  static void pruneAbandoned(const std::filesystem::path &root);

  Draft(Draft &&other) noexcept;
  Draft &operator=(Draft &&other) noexcept;
  Draft(const Draft &) = delete;
  Draft &operator=(const Draft &) = delete;
  ~Draft();

  // True until commit succeeds or discard runs. Every other accessor requires an open draft.
  bool isOpen() const;
  const std::filesystem::path &directory() const;

  // The path an attachment named `file` is written to. `file` must be one plain path component
  // (no separators, not "." or "..", not a report file name); otherwise nullopt and `error` is set.
  std::optional<std::filesystem::path> pathFor(std::string_view file, std::string &error) const;

  // Record a file the application already wrote at pathFor(file). Refuses a missing file or a name
  // attached twice.
  bool attach(std::string_view file, AttachmentRole role, std::string_view label,
              std::string &error);
  void addFact(std::string_view name, std::string_view value);
  void setReproduction(Reproduction reproduction);

  const Report &report() const;

  // Write the report with the player's words and move it to its final directory, whose path is
  // returned. Refuses an empty summary. After a successful commit the draft no longer owns a
  // directory; after a failed one it still does and may be committed again or discarded.
  std::optional<std::filesystem::path> commit(const PlayerText &text, std::string &error);

  // Remove the draft directory and everything in it. Idempotent.
  void discard();

private:
  struct State;
  explicit Draft(std::unique_ptr<State> state);

  std::unique_ptr<State> mState;
};

} // namespace bug_report
