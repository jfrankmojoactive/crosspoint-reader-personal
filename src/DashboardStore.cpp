#include "DashboardStore.h"

#include <Logging.h>

void DashboardStore::toJson(JsonDocument& doc) const {
  doc["url"] = url;
}

bool DashboardStore::fromJson(const JsonVariantConst doc) {
  if (!doc.is<JsonObjectConst>()) return false;
  const char* stored = doc["url"] | "";
  url.assign(stored);
  if (url.size() > MAX_URL_LENGTH) url.clear();
  return true;
}

bool DashboardStore::setUrl(const std::string& newUrl) {
  if (newUrl.size() > MAX_URL_LENGTH) {
    LOG_ERR("DASH", "URL too long: %u", static_cast<unsigned>(newUrl.size()));
    return false;
  }
  // Empty clears the configuration (the menu entry then prompts to set one).
  if (!newUrl.empty() && newUrl.rfind("http://", 0) != 0 && newUrl.rfind("https://", 0) != 0) {
    LOG_ERR("DASH", "URL must start with http:// or https://");
    return false;
  }
  if (newUrl == url) return true;  // Avoid a needless SD write (SPIFFS/SD erase cycles)
  url = newUrl;
  return saveToFile();
}
