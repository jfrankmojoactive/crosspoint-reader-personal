#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

struct Rect;

/**
 * Renders a Markdown page published by an external tool (see docs/dashboard.md).
 *
 * Fetches on entry over Wi-Fi and caches the body to the SD card, so opening
 * the screen without a network shows the last copy instead of an error. The
 * page is never fully materialised as laid-out lines: `content` holds the raw
 * bytes (capped) and each page is re-parsed line by line while drawing, which
 * keeps memory flat regardless of page length.
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

  // 8 KB is ~130 lines of dashboard: far more than fits on screen, and small
  // enough to hold alongside a reading session's heap. Anything beyond it is
  // dropped rather than risking an allocation failure mid-fetch.
  static constexpr size_t MAX_CONTENT_BYTES = 8192;
  static constexpr size_t MAX_PAGES = 32;
  static constexpr int MAX_WRAPPED_LINES = 6;
  static constexpr const char* CACHE_PATH = "/.crosspoint/dashboard.md";

  ButtonNavigator buttonNavigator;
  State state = State::CheckWifi;
  std::string content;
  // Byte offset into `content` of the first line of each page.
  std::vector<uint32_t> pageOffsets;
  size_t currentPage = 0;
  bool contentFromCache = false;
  bool contentTruncated = false;
  bool fetchStarted = false;
  const char* errorMessage = nullptr;

  void checkAndConnectWifi();
  void launchWifiSelection();
  void onWifiSelectionComplete(bool connected);
  void startFetch();
  void fetchContent();
  bool loadCachedContent();
  void saveCachedContent() const;
  void paginate();

  Rect getBodyRect() const;
  // Walks lines from `byteOffset`, drawing them when `draw` is set. Returns the
  // offset of the first line that did not fit, i.e. the next page's start.
  size_t layoutPage(size_t byteOffset, bool draw, Rect body) const;

  // Only while the radio is up: the viewer itself should sleep like any screen.
  bool preventAutoSleep() override { return state == State::Loading || state == State::CheckWifi; }
};
