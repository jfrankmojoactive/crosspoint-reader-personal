#include "DashboardActivity.h"

#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <WiFi.h>

#include <algorithm>
#include <string_view>

#include "DashboardStore.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/HttpDownloader.h"
#include "util/DashboardMarkdown.h"

namespace {

// Body/heading pairs per DashboardStore font-size index. A heading always reads
// at least as large as its body text, and is drawn bold, so the largest size
// still separates the two.
struct FontPair {
  int body;
  int heading;
};
constexpr FontPair FONT_PAIRS[DashboardStore::FONT_SIZE_COUNT] = {
    {UI_12_FONT_ID, NOTOSANS_16_FONT_ID},        // Small — the original pairing
    {NOTOSANS_14_FONT_ID, NOTOSANS_18_FONT_ID},  // Medium (default)
    {NOTOSANS_16_FONT_ID, NOTOSANS_18_FONT_ID},  // Large
    {NOTOSANS_18_FONT_ID, NOTOSANS_18_FONT_ID},  // Extra Large
};
constexpr int BULLET_INDENT = 18;
constexpr int KEY_VALUE_GAP = 12;

std::string_view lineAt(const std::string& content, const size_t offset, size_t& nextOffset) {
  const size_t eol = content.find('\n', offset);
  const size_t end = eol == std::string::npos ? content.size() : eol;
  nextOffset = eol == std::string::npos ? content.size() : eol + 1;
  return std::string_view(content.data() + offset, end - offset);
}

}  // namespace

void DashboardActivity::onEnter() {
  Activity::onEnter();

  DASHBOARD_STORE.loadFromFile();

  // Snapshot the configured size for this visit, after the load above: the
  // layout maths must not change between paginate() and render().
  const FontPair& fonts = FONT_PAIRS[DASHBOARD_STORE.getFontSize() % DashboardStore::FONT_SIZE_COUNT];
  bodyFont = fonts.body;
  headingFont = fonts.heading;
  if (!DASHBOARD_STORE.isConfigured()) {
    state = State::NoUrl;
    requestUpdate();
    return;
  }

  // Show the cached copy immediately if the fetch fails; loading it up front
  // costs one SD read and removes the "error screen with nothing on it" case.
  loadCachedContent();
  state = State::CheckWifi;
  requestUpdate();
}

void DashboardActivity::onExit() {
  content.clear();
  content.shrink_to_fit();
  pageOffsets.clear();
  pageOffsets.shrink_to_fit();
  Activity::onExit();
}

void DashboardActivity::loop() {
  if (state == State::CheckWifi) {
    checkAndConnectWifi();
    return;
  }

  if (state == State::Loading) {
    if (!fetchStarted) {
      fetchStarted = true;
      // Paint "Loading..." before the blocking fetch, or the screen sits on the
      // previous frame for the whole request.
      requestUpdateAndWait();
      fetchContent();
      requestUpdate();
    }
    return;
  }

  if (state == State::WifiSelection) return;  // The subactivity owns input

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome(HomeMenuItem::NONE);
    return;
  }

  // Confirm re-fetches from every terminal state, so a failed load is one
  // button press away from a retry.
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (state == State::NoUrl) return;
    startFetch();
    return;
  }

  if (state != State::Viewing || pageOffsets.size() <= 1) return;

  buttonNavigator.onNext([this] {
    if (currentPage + 1 >= pageOffsets.size()) return;
    ++currentPage;
    requestUpdate();
  });

  buttonNavigator.onPrevious([this] {
    if (currentPage == 0) return;
    --currentPage;
    requestUpdate();
  });
}

void DashboardActivity::checkAndConnectWifi() {
  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
    startFetch();
    return;
  }
  launchWifiSelection();
}

void DashboardActivity::launchWifiSelection() {
  state = State::WifiSelection;
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) { onWifiSelectionComplete(!result.isCancelled); });
}

void DashboardActivity::onWifiSelectionComplete(const bool connected) {
  if (connected) {
    startFetch();
    return;
  }
  // No network: fall back to whatever the SD card still holds.
  if (!content.empty()) {
    contentFromCache = true;
    paginate();
    state = State::Viewing;
  } else {
    errorMessage = tr(STR_DASHBOARD_NO_WIFI);
    state = State::Error;
  }
  requestUpdate();
}

void DashboardActivity::startFetch() {
  fetchStarted = false;
  currentPage = 0;
  state = State::Loading;
  requestUpdate();
}

bool DashboardActivity::appendDocument(const std::string& url, std::string& body, bool& hitCap) const {
  if (url.empty()) return true;  // Optional document: nothing to fetch is not a failure

  // Every document starts a screen, so a blank line before the next one keeps
  // its first heading from being treated as a continuation of the previous.
  if (!body.empty() && body.back() != '\n') body.push_back('\n');

  return HttpDownloader::fetchUrl(url, [&body, &hitCap](const uint8_t* data, const size_t len) {
    const size_t room = DashboardActivity::MAX_CONTENT_BYTES - body.size();
    if (room == 0) {
      // Stop the transfer: the rest of the page can never be displayed.
      hitCap = true;
      return false;
    }
    body.append(reinterpret_cast<const char*>(data), std::min(len, room));
    return true;
  });
}

void DashboardActivity::fetchContent() {
  const std::string& url = DASHBOARD_STORE.getUrl();
  const std::string& clientsUrl = DASHBOARD_STORE.getClientsUrl();
  std::string body;
  // One allocation covering a typical page: append-driven doubling would
  // otherwise realloc-and-copy several times mid-fetch, fragmenting DRAM.
  body.reserve(4096);
  bool hitCap = false;
  clientsFetchFailed = false;

  // Priorities first, then the client summaries: the reading order IS the
  // screen order. A clients page that fails leaves the priorities usable, so
  // only a failed priorities fetch counts as a failure worth falling back for.
  const bool ok = appendDocument(url, body, hitCap);
  if (ok && !hitCap && !clientsUrl.empty() && !appendDocument(clientsUrl, body, hitCap)) {
    LOG_ERR("DASH", "Clients fetch failed: %s", clientsUrl.c_str());
    clientsFetchFailed = true;
  }

  // Aborting at the cap surfaces as a failed fetch, but the bytes we kept are
  // a perfectly good (if clipped) page.
  if (ok || hitCap) {
    content = std::move(body);
    contentFromCache = false;
    contentTruncated = hitCap;
    saveCachedContent();
    paginate();
    state = pageOffsets.empty() ? State::Error : State::Viewing;
    if (pageOffsets.empty()) errorMessage = tr(STR_DASHBOARD_EMPTY);
    return;
  }

  LOG_ERR("DASH", "Fetch failed: %s", url.c_str());
  if (!content.empty()) {
    // Keep showing the cached copy rather than replacing it with an error.
    contentFromCache = true;
    paginate();
    state = State::Viewing;
    return;
  }
  errorMessage = tr(STR_DASHBOARD_FAILED);
  state = State::Error;
}

bool DashboardActivity::loadCachedContent() {
  HalFile file;
  if (!Storage.openFileForRead("DASH", CACHE_PATH, file) || !file) return false;

  const size_t size = std::min(static_cast<size_t>(file.fileSize()), MAX_CONTENT_BYTES);
  if (size == 0) return false;

  content.assign(size, '\0');
  const int read = file.read(reinterpret_cast<uint8_t*>(&content[0]), size);
  if (read <= 0) {
    content.clear();
    return false;
  }
  content.resize(static_cast<size_t>(read));
  contentFromCache = true;
  paginate();
  return true;
}

void DashboardActivity::saveCachedContent() const {
  if (content.empty()) return;
  Storage.ensureDirectoryExists("/.crosspoint");  // Takes a directory, not the file path
  HalFile file;
  if (!Storage.openFileForWrite("DASH", CACHE_PATH, file)) {
    LOG_ERR("DASH", "Could not cache dashboard");
    return;
  }
  file.write(reinterpret_cast<const uint8_t*>(content.data()), content.size());
}

Rect DashboardActivity::getBodyRect() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true);
  const int top = safe.y + metrics.headerHeight + metrics.verticalSpacing;
  // Inset the text the way every other content screen does: the bezel-safe area
  // alone runs glyphs right up to the edges, which is unreadable on the X3.
  const int pad = metrics.contentSidePadding;
  return Rect{safe.x + pad, top, safe.width - 2 * pad, safe.y + safe.height - top};
}

void DashboardActivity::paginate() {
  pageOffsets.clear();
  if (content.empty()) return;

  pageOffsets.reserve(8);
  const Rect body = getBodyRect();
  size_t offset = 0;
  while (offset < content.size() && pageOffsets.size() < MAX_PAGES) {
    pageOffsets.push_back(static_cast<uint32_t>(offset));
    const size_t next = layoutPage(offset, false, body);
    // A line too tall for an empty page would otherwise loop forever.
    if (next <= offset) break;
    offset = next;
  }
  if (currentPage >= pageOffsets.size()) currentPage = pageOffsets.empty() ? 0 : pageOffsets.size() - 1;
}

size_t DashboardActivity::layoutPage(const size_t byteOffset, const bool draw, const Rect body) const {
  const int bodyBottom = body.y + body.height;
  const int bodyLineHeight = renderer.getLineHeight(bodyFont);
  const int headingLineHeight = renderer.getLineHeight(headingFont);

  int y = body.y;
  size_t pos = byteOffset;
  bool firstOnPage = true;

  while (pos < content.size()) {
    size_t next = pos;
    const DashboardBlock block = parseDashboardLine(lineAt(content, pos, next));

    // A top-level heading opens a screen: '# Priorities' then '# Acme Corp'
    // gives one screen per section without any page-break syntax. Only when it
    // is not already first, or every screen would be a single heading.
    if (block.type == DashboardBlockType::Heading1 && !firstOnPage) return pos;

    // Height first: a block that does not fit ends the page untouched.
    int blockHeight = 0;
    int fontId = bodyFont;
    EpdFontFamily::Style style = EpdFontFamily::REGULAR;
    int indent = 0;
    std::vector<std::string> wrapped;

    switch (block.type) {
      case DashboardBlockType::Blank:
        blockHeight = bodyLineHeight / 2;
        break;
      case DashboardBlockType::Rule:
        blockHeight = bodyLineHeight / 2;
        break;
      case DashboardBlockType::Heading1:
      case DashboardBlockType::Heading2: {
        const bool isH1 = block.type == DashboardBlockType::Heading1;
        fontId = isH1 ? headingFont : bodyFont;
        style = EpdFontFamily::BOLD;
        wrapped = renderer.wrappedText(fontId, block.text.c_str(), body.width, MAX_WRAPPED_LINES, style);
        blockHeight = static_cast<int>(wrapped.size()) * (isH1 ? headingLineHeight : bodyLineHeight);
        // Breathing room above a heading, but not when it opens the page.
        if (!firstOnPage) blockHeight += bodyLineHeight / 2;
        break;
      }
      case DashboardBlockType::Bullet:
        indent = BULLET_INDENT;
        wrapped = renderer.wrappedText(fontId, block.text.c_str(), body.width - indent, MAX_WRAPPED_LINES, style);
        blockHeight = static_cast<int>(wrapped.size()) * bodyLineHeight;
        break;
      case DashboardBlockType::KeyValue: {
        const int labelWidth = renderer.getTextWidth(fontId, block.label.c_str(), EpdFontFamily::BOLD);
        const int valueWidth = renderer.getTextWidth(fontId, block.text.c_str(), style);
        if (labelWidth + KEY_VALUE_GAP + valueWidth <= body.width) {
          blockHeight = bodyLineHeight;  // Label left, value right-aligned on one line
        } else {
          indent = BULLET_INDENT;
          wrapped = renderer.wrappedText(fontId, block.text.c_str(), body.width - indent, MAX_WRAPPED_LINES, style);
          blockHeight = bodyLineHeight + static_cast<int>(wrapped.size()) * bodyLineHeight;
        }
        break;
      }
      case DashboardBlockType::Text:
        wrapped = renderer.wrappedText(fontId, block.text.c_str(), body.width, MAX_WRAPPED_LINES, style);
        blockHeight = static_cast<int>(wrapped.size()) * bodyLineHeight;
        break;
    }

    if (!firstOnPage && y + blockHeight > bodyBottom) return pos;

    if (draw) {
      switch (block.type) {
        case DashboardBlockType::Blank:
          break;
        case DashboardBlockType::Rule:
          renderer.drawLine(body.x, y + blockHeight / 2, body.x + body.width, y + blockHeight / 2, true);
          break;
        case DashboardBlockType::Heading1:
        case DashboardBlockType::Heading2: {
          const bool isH1 = block.type == DashboardBlockType::Heading1;
          int lineY = y + (firstOnPage ? 0 : bodyLineHeight / 2);
          for (const std::string& line : wrapped) {
            renderer.drawText(fontId, body.x, lineY, line.c_str(), true, style);
            lineY += isH1 ? headingLineHeight : bodyLineHeight;
          }
          break;
        }
        case DashboardBlockType::Bullet: {
          renderer.drawText(fontId, body.x, y, "\xE2\x80\xA2", true, style);  // U+2022 bullet
          int lineY = y;
          for (const std::string& line : wrapped) {
            renderer.drawText(fontId, body.x + indent, lineY, line.c_str(), true, style);
            lineY += bodyLineHeight;
          }
          break;
        }
        case DashboardBlockType::KeyValue: {
          renderer.drawText(fontId, body.x, y, block.label.c_str(), true, EpdFontFamily::BOLD);
          if (wrapped.empty()) {
            const int valueWidth = renderer.getTextWidth(fontId, block.text.c_str(), style);
            renderer.drawText(fontId, body.x + body.width - valueWidth, y, block.text.c_str(), true, style);
          } else {
            int lineY = y + bodyLineHeight;
            for (const std::string& line : wrapped) {
              renderer.drawText(fontId, body.x + indent, lineY, line.c_str(), true, style);
              lineY += bodyLineHeight;
            }
          }
          break;
        }
        case DashboardBlockType::Text: {
          int lineY = y;
          for (const std::string& line : wrapped) {
            renderer.drawText(fontId, body.x, lineY, line.c_str(), true, style);
            lineY += bodyLineHeight;
          }
          break;
        }
      }
    }

    y += blockHeight;
    firstOnPage = false;
    pos = next;
  }

  return pos;
}

void DashboardActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const Rect body = getBodyRect();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_DASHBOARD));

  const char* centred = nullptr;
  switch (state) {
    case State::NoUrl:
      centred = tr(STR_DASHBOARD_NO_URL);
      break;
    case State::CheckWifi:
    case State::WifiSelection:
      centred = tr(STR_CONNECTING);
      break;
    case State::Loading:
      centred = tr(STR_LOADING);
      break;
    case State::Error:
      centred = errorMessage ? errorMessage : tr(STR_DASHBOARD_FAILED);
      break;
    case State::Viewing:
      break;
  }

  if (centred != nullptr) {
    UITheme::drawCenteredWrappedText(renderer, body, UI_12_FONT_ID, centred, 4);
  } else {
    layoutPage(pageOffsets[currentPage], true, body);

    // Status line: only shown when it tells the reader something they need —
    // that the page is stale, clipped, or has more pages behind it.
    char status[64] = {0};
    if (pageOffsets.size() > 1) {
      snprintf(status, sizeof(status), "%u/%u", static_cast<unsigned>(currentPage + 1),
               static_cast<unsigned>(pageOffsets.size()));
    }
    const char* note = "";
    if (contentFromCache) {
      note = tr(STR_DASHBOARD_OFFLINE);
    } else if (contentTruncated) {
      note = tr(STR_DASHBOARD_CLIPPED);
    } else if (clientsFetchFailed) {
      // The priorities screens are intact; say so rather than showing nothing.
      note = tr(STR_DASHBOARD_CLIENTS_FAILED);
    }
    if (note[0] != '\0') {
      const int noteWidth = renderer.getTextWidth(SMALL_FONT_ID, note);
      renderer.drawText(SMALL_FONT_ID, body.x + body.width - noteWidth, body.y + body.height, note);
    }
    if (status[0] != '\0') {
      renderer.drawText(SMALL_FONT_ID, body.x, body.y + body.height, status);
    }
  }

  const bool paged = state == State::Viewing && pageOffsets.size() > 1;
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), state == State::NoUrl ? "" : tr(STR_REFRESH),
                                            paged ? tr(STR_DIR_UP) : "", paged ? tr(STR_DIR_DOWN) : "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
