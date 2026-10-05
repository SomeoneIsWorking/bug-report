#include "bug_report/draft.h"

#include <cctype>
#include <ctime>
#include <fstream>
#include <system_error>
#include <utility>

namespace bug_report {

namespace {

namespace fs = std::filesystem;

// Written into every draft directory this library creates, so pruning a stale draft can never
// remove a directory that merely has a similar name.
constexpr std::string_view kDraftMarker = ".bug-report-draft";
constexpr std::string_view kDraftSuffix = ".draft";
constexpr std::size_t kSlugLimit = 48;

std::tm utcTime(std::chrono::system_clock::time_point now) {
  const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
  std::tm utc{};
#ifdef _WIN32
  gmtime_s(&utc, &seconds);
#else
  gmtime_r(&seconds, &utc);
#endif
  return utc;
}

std::string formatTime(const std::tm &utc, const char *pattern) {
  char text[32];
  const std::size_t length = std::strftime(text, sizeof text, pattern, &utc);
  return std::string(text, length);
}

// Lowercase ASCII letters and digits, runs of anything else collapsed to one '-'.
std::string slugOf(std::string_view summary) {
  std::string slug;
  bool pendingDash = false;
  for (const char ch : summary) {
    const auto byte = static_cast<unsigned char>(ch);
    if (std::isalnum(byte) != 0 && byte < 0x80) {
      if (pendingDash && !slug.empty()) {
        slug += '-';
      }
      pendingDash = false;
      slug += static_cast<char>(std::tolower(byte));
      if (slug.size() >= kSlugLimit) {
        break;
      }
    } else {
      pendingDash = true;
    }
  }
  return slug.empty() ? std::string("report") : slug;
}

// `base`, or `base-2`, `base-3`, ... — the first name under `root` that does not exist yet.
fs::path freshPath(const fs::path &root, const std::string &base, std::string_view suffix) {
  fs::path candidate = root / (base + std::string(suffix));
  for (int n = 2; fs::exists(candidate); ++n) {
    candidate = root / (base + "-" + std::to_string(n) + std::string(suffix));
  }
  return candidate;
}

bool writeFile(const fs::path &path, std::string_view content, std::string &error) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(content.data(), static_cast<std::streamsize>(content.size()));
  out.close();
  if (!out) {
    error = "cannot write " + path.string();
    return false;
  }
  return true;
}

bool isDraftDirectory(const fs::path &directory) {
  std::error_code ec;
  const std::string name = directory.filename().string();
  return name.size() > kDraftSuffix.size() && name.ends_with(kDraftSuffix) &&
         fs::is_regular_file(directory / kDraftMarker, ec);
}

bool isReservedName(std::string_view file) {
  return file == kReportFileName || file == kReadmeFileName || file == kDraftMarker;
}

// A mark names an attached picture and lies inside it (a little rounding slack at the far edges).
bool marksAPicture(const Report &report, const Mark &mark) {
  constexpr double kSlack = 1e-3;
  bool picture = false;
  for (const Attachment &attachment : report.attachments) {
    picture = picture || (attachment.file == mark.file && isPicture(attachment.role));
  }
  return picture && mark.x >= 0 && mark.y >= 0 && mark.width > 0 && mark.height > 0 &&
         mark.x + mark.width <= 1 + kSlack && mark.y + mark.height <= 1 + kSlack;
}

} // namespace

struct Draft::State {
  fs::path directory;
  std::string stamp; // the directory's timestamp stem, reused for the committed name
  Report report;
};

void Draft::pruneAbandoned(const fs::path &root) {
  std::error_code ec;
  for (const fs::directory_entry &entry : fs::directory_iterator(root, ec)) {
    if (entry.is_directory(ec) && isDraftDirectory(entry.path())) {
      fs::remove_all(entry.path(), ec);
    }
  }
}

Draft::Draft(std::unique_ptr<State> state) : mState(std::move(state)) {
}

Draft::Draft(Draft &&other) noexcept = default;

Draft &Draft::operator=(Draft &&other) noexcept {
  if (this != &other) {
    discard();
    mState = std::move(other.mState);
  }
  return *this;
}

Draft::~Draft() {
  discard();
}

std::optional<Draft> Draft::open(const fs::path &root, std::string_view application,
                                 std::chrono::system_clock::time_point now, std::string &error) {
  std::error_code ec;
  fs::create_directories(root, ec);
  if (ec) {
    error = "cannot create " + root.string() + ": " + ec.message();
    return std::nullopt;
  }

  const std::tm utc = utcTime(now);
  auto state = std::make_unique<State>();
  state->stamp = formatTime(utc, "%Y%m%d-%H%M%S");
  state->directory = freshPath(root, state->stamp, kDraftSuffix);
  fs::create_directory(state->directory, ec);
  if (ec) {
    error = "cannot create " + state->directory.string() + ": " + ec.message();
    return std::nullopt;
  }
  if (!writeFile(state->directory / kDraftMarker, "", error)) {
    fs::remove_all(state->directory, ec);
    return std::nullopt;
  }
  state->report.application = std::string(application);
  state->report.created = formatTime(utc, "%Y-%m-%dT%H:%M:%SZ");
  return Draft(std::move(state));
}

bool Draft::isOpen() const {
  return mState != nullptr;
}

const fs::path &Draft::directory() const {
  return mState->directory;
}

std::optional<fs::path> Draft::pathFor(std::string_view file, std::string &error) const {
  const fs::path name(file);
  if (file.empty() || file == "." || file == ".." || name.has_parent_path() ||
      name.has_root_path() || file.find_first_of("/\\") != std::string_view::npos) {
    error = "attachment name must be one plain file name: '" + std::string(file) + "'";
    return std::nullopt;
  }
  if (isReservedName(file)) {
    error = "attachment name is reserved for the report itself: " + std::string(file);
    return std::nullopt;
  }
  return mState->directory / name;
}

bool Draft::attach(std::string_view file, AttachmentRole role, std::string_view label,
                   std::string &error) {
  const std::optional<fs::path> path = pathFor(file, error);
  if (!path) {
    return false;
  }
  for (const Attachment &existing : mState->report.attachments) {
    if (existing.file == file) {
      error = "already attached: " + std::string(file);
      return false;
    }
  }
  std::error_code ec;
  if (!fs::is_regular_file(*path, ec)) {
    error = "attachment was not written: " + path->string();
    return false;
  }
  mState->report.attachments.push_back(Attachment{std::string(file), role, std::string(label)});
  return true;
}

void Draft::addFact(std::string_view name, std::string_view value) {
  mState->report.facts.push_back(Fact{std::string(name), std::string(value)});
}

void Draft::setReproduction(Reproduction reproduction) {
  mState->report.reproduction = std::move(reproduction);
}

const Report &Draft::report() const {
  return mState->report;
}

std::optional<fs::path> Draft::commit(const PlayerText &text, std::string &error) {
  if (!mState) {
    error = "this draft was already committed or discarded";
    return std::nullopt;
  }
  if (text.summary.find_first_not_of(" \t\r\n") == std::string::npos) {
    error = "write a one-line summary first";
    return std::nullopt;
  }
  for (const Mark &mark : text.marks) {
    if (!marksAPicture(mState->report, mark)) {
      error = "a mark must lie inside an attached picture: " + mark.file;
      return std::nullopt;
    }
  }
  Report &report = mState->report;
  report.summary = text.summary;
  report.description = text.description;
  report.marks = text.marks;
  if (!writeFile(mState->directory / kReportFileName, toJson(report), error) ||
      !writeFile(mState->directory / kReadmeFileName, toMarkdown(report), error)) {
    return std::nullopt;
  }

  fs::path finalPath =
      freshPath(mState->directory.parent_path(), mState->stamp + "-" + slugOf(text.summary), "");
  std::error_code ec;
  fs::rename(mState->directory, finalPath, ec);
  if (ec) {
    error = "cannot move the report to " + finalPath.string() + ": " + ec.message();
    return std::nullopt;
  }
  fs::remove(finalPath / kDraftMarker, ec);
  mState.reset();
  return finalPath;
}

void Draft::discard() {
  if (!mState) {
    return;
  }
  std::error_code ec;
  if (isDraftDirectory(mState->directory)) {
    fs::remove_all(mState->directory, ec);
  }
  mState.reset();
}

} // namespace bug_report
