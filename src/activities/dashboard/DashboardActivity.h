#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "DashboardStore.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

struct Rect;

/**
 * Renders the MoJo Active pages published by an external tool (see
 * docs/dashboard.md): up to three documents — priorities, per-client
 * summaries, and client-relevant news — each shown as its own tabbed section.
 * Left/Right switch sections, Up/Down page within one, following the tab
 * pattern SettingsActivity uses.
 *
 * All configured sections are fetched on entry, each to its own cache file on
 * the SD card, but only the section being read is held in memory: `content`
 * holds one section's raw bytes (capped) and each page is re-parsed line by
 * line while drawing, so memory stays flat no matter how many sections exist
 * or how long they are. Switching tabs is an SD read, so it works with the
 * radio off.
 */
class DashboardActivity final : public Activity {
 public:
  explicit DashboardActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Dashboard", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class State : uint8_t { NoUrl, CheckWifi, WifiSelection, Loading, Viewing, Error };

  // Index order matches DashboardStore::getSectionUrl().
  enum SectionId : uint8_t { Priorities = 0, Clients = 1, News = 2 };

  // 8 KB per section is ~130 lines: far more than fits on screen, and small
  // enough to hold alongside a reading session's heap. Only one section is
  // resident at a time, so this is the whole content budget.
  static constexpr size_t MAX_CONTENT_BYTES = 8192;
  static constexpr size_t MAX_PAGES = 32;
  static constexpr int MAX_WRAPPED_LINES = 6;
  // Written before sections existed; removed on first run so it does not sit
  // on the card forever as an orphan.
  static constexpr const char* LEGACY_CACHE_PATH = "/.crosspoint/dashboard.md";

  struct SectionState {
    bool configured = false;  // Has a URL, so it gets a tab
    bool fetched = false;     // Refreshed successfully this visit
    bool cached = false;      // A cache file exists to fall back on
    bool truncated = false;   // Hit MAX_CONTENT_BYTES during the fetch
  };

  ButtonNavigator buttonNavigator;
  // Resolved from DashboardStore's font-size index in onEnter().
  int bodyFont = 0;
  int headingFont = 0;
  State state = State::CheckWifi;
  SectionState sections[DashboardStore::SECTION_COUNT];
  uint8_t activeSection = SectionId::Priorities;
  std::string content;  // The active section only
  // Byte offset into `content` of the first line of each page.
  std::vector<uint32_t> pageOffsets;
  size_t currentPage = 0;
  bool fetchStarted = false;
  // True while handling a user-initiated refresh, which may show the Wi-Fi
  // picker; entering the screen never does when a cached copy exists.
  bool interactiveWifi = false;
  const char* errorMessage = nullptr;

  void checkAndConnectWifi();
  void launchWifiSelection();
  void onWifiSelectionComplete(bool connected);
  void startFetch();
  void fetchAllSections();
  // Fetches one section straight to its cache file. Returns false on a
  // transport failure, leaving any previous cache intact.
  bool fetchSection(uint8_t section);
  // Loads a section's cache into `content` and paginates it. Returns false when
  // there is nothing on the card for it.
  bool showSection(uint8_t section);
  // Steps to the next/previous configured section, wrapping. No-op when only
  // one is configured.
  void stepSection(int delta);
  uint8_t firstConfiguredSection() const;
  bool anySectionCached() const;
  static const char* cachePathFor(uint8_t section);
  static const char* tabLabelFor(uint8_t section);

  void paginate();

  Rect getBodyRect() const;
  int tabBarHeight() const;
  // Walks lines from `byteOffset`, drawing them when `draw` is set. Returns the
  // offset of the first line that did not fit, i.e. the next page's start.
  size_t layoutPage(size_t byteOffset, bool draw, Rect body) const;

  // Only while the radio is up: the viewer itself should sleep like any screen.
  bool preventAutoSleep() override { return state == State::Loading || state == State::CheckWifi; }
};
