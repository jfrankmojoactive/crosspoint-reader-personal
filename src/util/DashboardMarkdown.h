#pragma once
#include <cstdint>
#include <string>
#include <string_view>

/**
 * Minimal, line-based Markdown subset for the Dashboard screen.
 *
 * Deliberately NOT a Markdown parser: there is no document model, no inline
 * tree, and no lookahead across lines. Each line maps to exactly one block, so
 * a page of any size renders in constant memory and unsupported syntax
 * degrades to plain text instead of failing.
 *
 * Recognised per line:
 *   # Heading            -> Heading1
 *   ## / ### Heading     -> Heading2
 *   - item / * item      -> Bullet
 *   1. item              -> Bullet
 *   > quote              -> Text (marker stripped)
 *   Label: value         -> KeyValue
 *   | a | b |            -> KeyValue (2 cells) or Text; separator rows are Blank
 *   --- / *** / ___      -> Rule
 *   (empty)              -> Blank
 *   anything else        -> Text
 */

enum class DashboardBlockType : uint8_t {
  Blank,
  Rule,
  Heading1,
  Heading2,
  Bullet,
  KeyValue,
  Text,
};

struct DashboardBlock {
  DashboardBlockType type = DashboardBlockType::Blank;
  // Display text. For KeyValue this is the value only; `label` holds the key.
  std::string text;
  std::string label;
};

// Longest label still treated as a "Label: value" key rather than prose that
// happens to contain a colon.
inline constexpr size_t DASHBOARD_MAX_KEY_LENGTH = 32;

/// Removes the inline markers Claude reliably emits (`**bold**`, `` `code` ``,
/// `[text](url)`), leaving the visible text. Single `*`/`_` are left alone:
/// stripping them mangles snake_case identifiers far more often than it helps.
std::string stripInlineMarkdown(std::string_view text);

/// Classifies one raw line (trailing CR and surrounding spaces are ignored).
DashboardBlock parseDashboardLine(std::string_view rawLine);
