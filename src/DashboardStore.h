#pragma once
#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <cstdint>
#include <string>

/**
 * Stores the URLs of the MoJo Active pages published by an external tool (see
 * docs/dashboard.md). Three documents, each shown as its own tabbed section:
 * `url` holds the priorities page, `clientsUrl` the per-client summaries, and
 * `newsUrl` the client-relevant news. Only the priorities page is required; a
 * section with no URL has no tab.
 *
 * Public HTTPS URLs only — no credentials are kept here, so a URL itself is the
 * secret. Keep them unguessable.
 */
class DashboardStore : public PersistableStore<DashboardStore> {
 private:
  std::string url;
  std::string clientsUrl;
  std::string newsUrl;
  uint8_t fontSize = 1;  // Index into FONT_SIZE_COUNT; 1 ("Medium") reads well at arm's length.

  DashboardStore() = default;

  friend class PersistableStore<DashboardStore>;

  // Shared trim / scheme-defaulting / validation for both URL fields. Returns
  // false (leaving `out` untouched) when the input cannot be a fetchable URL.
  static bool normalise(const std::string& in, std::string& out);

 public:
  // Cap matches the keyboard entry limit; also bounds the JSON we write.
  static constexpr size_t MAX_URL_LENGTH = 255;

  static const char* getFilePath() { return "/.crosspoint/dashboard.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  // Number of selectable sizes; the activity maps the index to a font pair.
  static constexpr uint8_t FONT_SIZE_COUNT = 4;

  const std::string& getUrl() const { return url; }
  const std::string& getClientsUrl() const { return clientsUrl; }
  const std::string& getNewsUrl() const { return newsUrl; }

  // Section-indexed access, so the activity can loop over sections instead of
  // repeating a three-way switch. Index order matches SectionId in
  // DashboardActivity; out-of-range returns the empty string.
  static constexpr uint8_t SECTION_COUNT = 3;
  const std::string& getSectionUrl(uint8_t section) const;
  // The priorities page alone is a usable screen; the clients page is optional.
  bool isConfigured() const { return !url.empty(); }

  // Both reject anything that is not an absolute http(s) URL, so the fetch path
  // never sees a scheme SecureHttpClient would refuse to parse. An empty value
  // clears the field.
  bool setUrl(const std::string& newUrl);
  bool setClientsUrl(const std::string& newUrl);
  bool setNewsUrl(const std::string& newUrl);

  uint8_t getFontSize() const { return fontSize; }
  // Out-of-range values are clamped rather than rejected: the web API hands us
  // whatever index the page sent.
  bool setFontSize(uint8_t size);
};

#define DASHBOARD_STORE DashboardStore::getInstance()
