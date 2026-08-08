#include <gtest/gtest.h>

#include <string>

#include "DashboardMarkdown.h"

namespace {

DashboardBlock parse(const std::string& line) {
  return parseDashboardLine(line);
}

TEST(DashboardMarkdown, BlankAndRule) {
  EXPECT_EQ(parse("").type, DashboardBlockType::Blank);
  EXPECT_EQ(parse("   ").type, DashboardBlockType::Blank);
  EXPECT_EQ(parse("---").type, DashboardBlockType::Rule);
  EXPECT_EQ(parse("*****").type, DashboardBlockType::Rule);
  // Two dashes is not a rule; it is text.
  EXPECT_EQ(parse("--").type, DashboardBlockType::Text);
}

TEST(DashboardMarkdown, Headings) {
  const DashboardBlock h1 = parse("# Today");
  EXPECT_EQ(h1.type, DashboardBlockType::Heading1);
  EXPECT_EQ(h1.text, "Today");

  EXPECT_EQ(parse("## Tasks").type, DashboardBlockType::Heading2);
  EXPECT_EQ(parse("### Deeper").type, DashboardBlockType::Heading2);
  EXPECT_EQ(parse("###   Padded  ").text, "Padded");
}

TEST(DashboardMarkdown, Bullets) {
  EXPECT_EQ(parse("- Ship the thing").type, DashboardBlockType::Bullet);
  EXPECT_EQ(parse("* Ship the thing").text, "Ship the thing");
  EXPECT_EQ(parse("+ Ship the thing").type, DashboardBlockType::Bullet);
  EXPECT_EQ(parse("1. First").type, DashboardBlockType::Bullet);
  EXPECT_EQ(parse("12) Twelfth").text, "Twelfth");
  // A bare dash with no text is not a bullet.
  EXPECT_EQ(parse("-").type, DashboardBlockType::Text);
}

TEST(DashboardMarkdown, KeyValue) {
  const DashboardBlock kv = parse("Updated: 2026-08-08 09:00");
  EXPECT_EQ(kv.type, DashboardBlockType::KeyValue);
  EXPECT_EQ(kv.label, "Updated");
  EXPECT_EQ(kv.text, "2026-08-08 09:00");
}

TEST(DashboardMarkdown, UrlsAndTimesAreNotKeyValues) {
  EXPECT_EQ(parse("https://example.com/dash.md").type, DashboardBlockType::Text);
  // No space after the colon, so this is prose, not a key.
  EXPECT_EQ(parse("Standup at 09:30 sharp").type, DashboardBlockType::Text);
}

TEST(DashboardMarkdown, LongLabelIsProseNotKeyValue) {
  const std::string longKey(DASHBOARD_MAX_KEY_LENGTH + 1, 'k');
  EXPECT_EQ(parse(longKey + ": value").type, DashboardBlockType::Text);
}

TEST(DashboardMarkdown, TableRows) {
  const DashboardBlock row = parse("| Inbox | 12 |");
  EXPECT_EQ(row.type, DashboardBlockType::KeyValue);
  EXPECT_EQ(row.label, "Inbox");
  EXPECT_EQ(row.text, "12");

  // The |---|---| rule row carries no content.
  EXPECT_EQ(parse("|---|---|").type, DashboardBlockType::Blank);
  EXPECT_EQ(parse("| :--- | ---: |").type, DashboardBlockType::Blank);

  const DashboardBlock wide = parse("| A | B | C |");
  EXPECT_EQ(wide.type, DashboardBlockType::Text);
  EXPECT_EQ(wide.text, "A  B  C");
}

TEST(DashboardMarkdown, BlockQuoteBecomesText) {
  const DashboardBlock quote = parse("> Remember the milk");
  EXPECT_EQ(quote.type, DashboardBlockType::Text);
  EXPECT_EQ(quote.text, "Remember the milk");
}

TEST(DashboardMarkdown, StripsInlineMarkers) {
  EXPECT_EQ(stripInlineMarkdown("**bold** and `code`"), "bold and code");
  EXPECT_EQ(stripInlineMarkdown("[CrossPoint](https://example.com)"), "CrossPoint");
  EXPECT_EQ(stripInlineMarkdown("__also bold__"), "also bold");
  // Single markers are preserved so identifiers survive intact.
  EXPECT_EQ(stripInlineMarkdown("snake_case_name"), "snake_case_name");
  EXPECT_EQ(stripInlineMarkdown("2 * 3 = 6"), "2 * 3 = 6");
  // An unterminated link is left as written rather than swallowed.
  EXPECT_EQ(stripInlineMarkdown("[broken](unclosed"), "[broken](unclosed");
}

TEST(DashboardMarkdown, StripsMarkersInsideBlocks) {
  EXPECT_EQ(parse("- **Ship** the `thing`").text, "Ship the thing");
  EXPECT_EQ(parse("# **Today**").text, "Today");
}

TEST(DashboardMarkdown, TrailingCarriageReturnIgnored) {
  EXPECT_EQ(parse("# Today\r").text, "Today");
  EXPECT_EQ(parse("\r").type, DashboardBlockType::Blank);
}

}  // namespace
