#pragma once
#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <string>

/**
 * Stores the URL of the dashboard page published by an external tool (see
 * docs/dashboard.md). Public HTTPS URL only — no credentials are kept here, so
 * the URL itself is the secret. Keep it unguessable.
 */
class DashboardStore : public PersistableStore<DashboardStore> {
 private:
  std::string url;

  DashboardStore() = default;

  friend class PersistableStore<DashboardStore>;

 public:
  // Cap matches the keyboard entry limit; also bounds the JSON we write.
  static constexpr size_t MAX_URL_LENGTH = 255;

  static const char* getFilePath() { return "/.crosspoint/dashboard.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  const std::string& getUrl() const { return url; }
  bool isConfigured() const { return !url.empty(); }

  // Rejects anything that is not an absolute http(s) URL, so the fetch path
  // never sees a scheme SecureHttpClient would refuse to parse.
  bool setUrl(const std::string& newUrl);
};

#define DASHBOARD_STORE DashboardStore::getInstance()
