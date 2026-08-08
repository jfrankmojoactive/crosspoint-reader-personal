#include "DashboardMarkdown.h"

#include <algorithm>
#include <cctype>
#include <vector>

namespace {

std::string_view trim(std::string_view text) {
  while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
  while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) text.remove_suffix(1);
  return text;
}

bool isRule(std::string_view text) {
  if (text.size() < 3) return false;
  const char marker = text.front();
  if (marker != '-' && marker != '*' && marker != '_') return false;
  return std::all_of(text.begin(), text.end(), [marker](char c) { return c == marker; });
}

// "1. item" / "12) item" — an ordered-list marker we render as a plain bullet.
bool startsWithOrderedMarker(std::string_view text, size_t& markerLength) {
  size_t digits = 0;
  while (digits < text.size() && std::isdigit(static_cast<unsigned char>(text[digits]))) ++digits;
  if (digits == 0 || digits + 1 >= text.size()) return false;
  if (text[digits] != '.' && text[digits] != ')') return false;
  if (text[digits + 1] != ' ') return false;
  markerLength = digits + 2;
  return true;
}

// A "|---|:--:|" row carries no content: it only draws a table rule in real
// Markdown, so it is dropped rather than rendered as punctuation.
bool isTableSeparator(const std::vector<std::string>& cells) {
  if (cells.empty()) return false;
  return std::all_of(cells.begin(), cells.end(), [](const std::string& cell) {
    return !cell.empty() && cell.find_first_not_of("-: ") == std::string::npos;
  });
}

std::vector<std::string> splitTableRow(std::string_view row) {
  std::vector<std::string> cells;
  // Drop the delimiting pipes so an empty leading/trailing cell is not produced.
  row.remove_prefix(1);
  if (!row.empty() && row.back() == '|') row.remove_suffix(1);
  cells.reserve(std::count(row.begin(), row.end(), '|') + 1);
  size_t start = 0;
  for (size_t i = 0; i <= row.size(); ++i) {
    if (i == row.size() || row[i] == '|') {
      cells.emplace_back(trim(row.substr(start, i - start)));
      start = i + 1;
    }
  }
  return cells;
}

// True when `text` reads as "Label: value" rather than prose with a colon.
// Rejects URLs (the colon is followed by "//") and long labels.
bool splitKeyValue(std::string_view text, std::string& label, std::string& value) {
  const size_t colon = text.find(':');
  if (colon == std::string_view::npos || colon == 0 || colon > DASHBOARD_MAX_KEY_LENGTH) return false;
  const std::string_view key = text.substr(0, colon);
  if (key.find('/') != std::string_view::npos) return false;
  const std::string_view rest = trim(text.substr(colon + 1));
  if (rest.empty()) return false;
  // "https://host" and "12:30" are not key/value pairs.
  if (text[colon + 1] != ' ') return false;
  label.assign(key);
  value.assign(rest);
  return true;
}

}  // namespace

std::string stripInlineMarkdown(const std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (size_t i = 0; i < text.size();) {
    // [label](target) -> label
    if (text[i] == '[') {
      const size_t close = text.find(']', i + 1);
      if (close != std::string_view::npos && close + 1 < text.size() && text[close + 1] == '(') {
        const size_t target = text.find(')', close + 2);
        if (target != std::string_view::npos) {
          out.append(stripInlineMarkdown(text.substr(i + 1, close - i - 1)));
          i = target + 1;
          continue;
        }
      }
    }
    if (text.compare(i, 2, "**") == 0 || text.compare(i, 2, "__") == 0) {
      i += 2;
      continue;
    }
    if (text[i] == '`') {
      ++i;
      continue;
    }
    out += text[i];
    ++i;
  }
  return out;
}

DashboardBlock parseDashboardLine(const std::string_view rawLine) {
  DashboardBlock block;
  const std::string_view line = trim(rawLine);

  if (line.empty()) return block;  // Blank

  if (isRule(line)) {
    block.type = DashboardBlockType::Rule;
    return block;
  }

  if (line.front() == '#') {
    size_t level = 0;
    while (level < line.size() && line[level] == '#') ++level;
    const std::string_view rest = trim(line.substr(level));
    block.type = level == 1 ? DashboardBlockType::Heading1 : DashboardBlockType::Heading2;
    block.text = stripInlineMarkdown(rest);
    return block;
  }

  if (line.front() == '|') {
    const std::vector<std::string> cells = splitTableRow(line);
    if (isTableSeparator(cells)) return block;  // Blank: the rule carries no content
    if (cells.size() == 2 && !cells[0].empty()) {
      block.type = DashboardBlockType::KeyValue;
      block.label = stripInlineMarkdown(cells[0]);
      block.text = stripInlineMarkdown(cells[1]);
      return block;
    }
    std::string joined;
    for (const std::string& cell : cells) {
      if (cell.empty()) continue;
      if (!joined.empty()) joined += "  ";
      joined += cell;
    }
    block.type = DashboardBlockType::Text;
    block.text = stripInlineMarkdown(joined);
    return block;
  }

  size_t markerLength = 0;
  const bool unordered = line.size() > 2 && (line[0] == '-' || line[0] == '*' || line[0] == '+') && line[1] == ' ';
  if (unordered || startsWithOrderedMarker(line, markerLength)) {
    block.type = DashboardBlockType::Bullet;
    block.text = stripInlineMarkdown(trim(line.substr(unordered ? 2 : markerLength)));
    return block;
  }

  if (line.front() == '>') {
    block.type = DashboardBlockType::Text;
    block.text = stripInlineMarkdown(trim(line.substr(1)));
    return block;
  }

  std::string label;
  std::string value;
  if (splitKeyValue(line, label, value)) {
    block.type = DashboardBlockType::KeyValue;
    block.label = stripInlineMarkdown(label);
    block.text = stripInlineMarkdown(value);
    return block;
  }

  block.type = DashboardBlockType::Text;
  block.text = stripInlineMarkdown(line);
  return block;
}
