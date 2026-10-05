// bug_report::Report — what one bug report says, independent of where it is stored or shown.
//
// A report is the player's own words (summary, description) plus what the application captured at
// the moment the report was opened: facts (frame number, area, build, settings), files (pictures,
// recordings, saves), and how to reproduce it without the player. The application decides what to
// capture; this library decides how a report is laid out on disk and presented for editing.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace bug_report {

// Inline constants for the on-disk layout. `kFormat` is written into every report.json so a reader
// can refuse a layout it does not understand.
inline constexpr std::string_view kFormat = "bug-report/1";
inline constexpr std::string_view kReportFileName = "report.json";
inline constexpr std::string_view kReadmeFileName = "README.md";

// What an attached file is for. Pictures are shown in the editing form; the other roles are listed.
enum class AttachmentRole : std::uint8_t {
  Screenshot, // the picture the player saw
  Reference,  // a picture to compare against (an oracle or reference renderer's view of the frame)
  Reproduction, // an input recording, save, or other file a reproduction needs
  Context,      // anything else the application wants a reader to have (logs, dumps)
};

std::string_view roleName(AttachmentRole role);
bool isPicture(AttachmentRole role);

struct Attachment {
  std::string file; // one path component inside the report's directory
  AttachmentRole role = AttachmentRole::Context;
  std::string label; // what a reader is looking at, e.g. "PSX renderer"
};

struct Fact {
  std::string name;
  std::string value;
};

// How to get back to the reported moment without the player.
struct Reproduction {
  std::string summary;            // one line: what replaying does
  std::vector<std::string> steps; // ordered instructions a person or agent follows
  std::string command;            // a single command line that replays it, or empty
};

// A region the player marked on one picture, as fractions of that picture's width and height
// measured from its top-left corner, so it holds at whatever size the picture is shown or stored.
struct Mark {
  std::string file; // the picture attachment it is drawn on
  double x = 0;
  double y = 0;
  double width = 0;
  double height = 0;
};

// The player's input: what the form collects and what a commit records.
struct PlayerText {
  std::string summary; // one line, required
  std::string description;
  std::vector<Mark> marks; // where on the pictures the bug shows, possibly none
};

struct Report {
  std::string application;
  std::string created; // ISO-8601 UTC, e.g. 2026-10-05T12:34:56Z
  std::string summary;
  std::string description;
  std::vector<Fact> facts;
  std::vector<Attachment> attachments;
  std::vector<Mark> marks;
  Reproduction reproduction;
};

// The two on-disk renderings of one report. report.json is the machine-readable authority; the
// README is the same content for a person browsing the folder.
std::string toJson(const Report &report);
std::string toMarkdown(const Report &report);

} // namespace bug_report
