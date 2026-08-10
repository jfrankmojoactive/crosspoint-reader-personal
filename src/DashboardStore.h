#pragma once
#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <string>

/**
 * Stores the URLs of the MoJo Active pages published by an external tool (see
 * docs/dashboard.md). Two documents, fetched in order and shown as one
 * front-to-back sequence of screens: `url` holds the priorities page, and the
 * optional `clientsUrl` holds the per-client summaries.
 *
 * Public HTTPS URLs only — no credentials are kept here, so a URL itself is the
 * secret. Keep them unguessable.
 */
class DashboardStore : public PersistableStore<DashboardStore> {
 private:
  std::string url;
  std::string clientsUrl;

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

  const std::string& getUrl() const { return url; }
  const std::string& getClientsUrl() const { return clientsUrl; }
  // The priorities page alone is a usable screen; the clients page is optional.
  bool isConfigured() const { return !url.empty(); }

  // Both reject anything that is not an absolute http(s) URL, so the fetch path
  // never sees a scheme SecureHttpClient would refuse to parse. An empty value
  // clears the field.
  bool setUrl(const std::string& newUrl);
  bool setClientsUrl(const std::string& newUrl);
};

#define DASHBOARD_STORE DashboardStore::getInstance()
