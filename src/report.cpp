#include "bug_report/report.h"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace bug_report {

namespace {

void appendJsonString(std::string &out, std::string_view text) {
  out += '"';
  for (const char ch : text) {
    switch (ch) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      if (static_cast<unsigned char>(ch) < 0x20) {
        char escaped[8];
        std::snprintf(escaped, sizeof escaped, "\\u%04x", static_cast<unsigned>(ch));
        out += escaped;
      } else {
        out += ch;
      }
      break;
    }
  }
  out += '"';
}

// Pretty-printed JSON with commas and indentation tracked by the writer, so every caller states
// only structure: a key opens a member, a value or container fills it.
class JsonWriter {
public:
  void key(std::string_view name) {
    separate();
    appendJsonString(mOut, name);
    mOut += ": ";
    mPendingKey = true;
  }
  void value(std::string_view text) {
    separate();
    appendJsonString(mOut, text);
  }
  void beginObject() {
    open('{');
  }
  void endObject() {
    close('}');
  }
  void beginArray() {
    open('[');
  }
  void endArray() {
    close(']');
  }
  std::string finish() {
    mOut += '\n';
    return std::move(mOut);
  }

private:
  // Before a member or element: a comma after a sibling, then a new line at the current depth.
  // A value directly after its key stays on the key's line.
  void separate() {
    if (mPendingKey) {
      mPendingKey = false;
      return;
    }
    if (!mHasSibling.empty()) {
      if (mHasSibling.back()) {
        mOut += ',';
      }
      mHasSibling.back() = true;
      mOut += '\n';
      mOut.append(mHasSibling.size() * 2, ' ');
    }
  }
  void open(char bracket) {
    separate();
    mOut += bracket;
    mHasSibling.push_back(false);
  }
  void close(char bracket) {
    const bool hadMembers = mHasSibling.back();
    mHasSibling.pop_back();
    if (hadMembers) {
      mOut += '\n';
      mOut.append(mHasSibling.size() * 2, ' ');
    }
    mOut += bracket;
  }

  std::string mOut;
  std::vector<bool> mHasSibling; // one entry per open container
  bool mPendingKey = false;
};

// A fenced Markdown block that cannot be closed early by the text it holds.
void appendFenced(std::string &out, std::string_view text) {
  std::string fence = "```";
  while (text.find(fence) != std::string_view::npos) {
    fence += '`';
  }
  out += fence;
  out += '\n';
  out += text;
  if (text.empty() || text.back() != '\n') {
    out += '\n';
  }
  out += fence;
  out += '\n';
}

// One Markdown table cell: a pipe would split the cell and a newline would end the row.
void appendCell(std::string &out, std::string_view text) {
  for (const char ch : text) {
    if (ch == '|') {
      out += "\\|";
    } else if (ch == '\n' || ch == '\r') {
      out += ' ';
    } else {
      out += ch;
    }
  }
}

} // namespace

std::string_view roleName(AttachmentRole role) {
  switch (role) {
  case AttachmentRole::Screenshot:
    return "screenshot";
  case AttachmentRole::Reference:
    return "reference";
  case AttachmentRole::Reproduction:
    return "reproduction";
  case AttachmentRole::Context:
    return "context";
  }
  return "context";
}

bool isPicture(AttachmentRole role) {
  return role == AttachmentRole::Screenshot || role == AttachmentRole::Reference;
}

std::string toJson(const Report &report) {
  JsonWriter json;
  const auto field = [&json](std::string_view name, std::string_view text) {
    json.key(name);
    json.value(text);
  };
  json.beginObject();
  field("format", kFormat);
  field("application", report.application);
  field("created", report.created);
  field("summary", report.summary);
  field("description", report.description);

  json.key("facts");
  json.beginArray();
  for (const Fact &fact : report.facts) {
    json.beginObject();
    field("name", fact.name);
    field("value", fact.value);
    json.endObject();
  }
  json.endArray();

  json.key("attachments");
  json.beginArray();
  for (const Attachment &attachment : report.attachments) {
    json.beginObject();
    field("file", attachment.file);
    field("role", roleName(attachment.role));
    field("label", attachment.label);
    json.endObject();
  }
  json.endArray();

  json.key("reproduction");
  json.beginObject();
  field("summary", report.reproduction.summary);
  json.key("steps");
  json.beginArray();
  for (const std::string &step : report.reproduction.steps) {
    json.value(step);
  }
  json.endArray();
  field("command", report.reproduction.command);
  json.endObject();

  json.endObject();
  return json.finish();
}

std::string toMarkdown(const Report &report) {
  std::string out = "# ";
  out += report.summary;
  out += "\n\n";
  out += report.application;
  out += " — ";
  out += report.created;
  out += "\n\n";
  if (!report.description.empty()) {
    out += report.description;
    out += "\n\n";
  }

  if (!report.attachments.empty()) {
    out += "## Attachments\n\n";
    for (const Attachment &attachment : report.attachments) {
      if (isPicture(attachment.role)) {
        out += "### ";
        out += attachment.label;
        out += "\n\n![";
        out += attachment.label;
        out += "](";
        out += attachment.file;
        out += ")\n\n";
      } else {
        out += "- `";
        out += attachment.file;
        out += "` (";
        out += roleName(attachment.role);
        out += ") ";
        out += attachment.label;
        out += "\n";
      }
    }
    if (!out.ends_with("\n\n")) {
      out += "\n";
    }
  }

  const Reproduction &reproduction = report.reproduction;
  if (!reproduction.summary.empty() || !reproduction.steps.empty() ||
      !reproduction.command.empty()) {
    out += "## Reproduction\n\n";
    if (!reproduction.summary.empty()) {
      out += reproduction.summary;
      out += "\n\n";
    }
    for (std::size_t i = 0; i < reproduction.steps.size(); ++i) {
      out += std::to_string(i + 1);
      out += ". ";
      out += reproduction.steps[i];
      out += "\n";
    }
    if (!reproduction.steps.empty()) {
      out += "\n";
    }
    if (!reproduction.command.empty()) {
      appendFenced(out, reproduction.command);
      out += "\n";
    }
  }

  if (!report.facts.empty()) {
    out += "## Facts\n\n| name | value |\n|---|---|\n";
    for (const Fact &fact : report.facts) {
      out += "| ";
      appendCell(out, fact.name);
      out += " | ";
      appendCell(out, fact.value);
      out += " |\n";
    }
  }
  return out;
}

} // namespace bug_report
