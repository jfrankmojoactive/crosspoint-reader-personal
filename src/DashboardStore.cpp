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
  std::string candidate = newUrl;

  // Trim: the web form and the on-device keyboard both make stray spaces easy.
  const size_t first = candidate.find_first_not_of(" \t");
  if (first == std::string::npos) {
    candidate.clear();
  } else {
    candidate = candidate.substr(first, candidate.find_last_not_of(" \t") - first + 1);
  }

  // A scheme-less host is the common way to type a URL ("example.com/d.md").
  // Assume https rather than rejecting it — the fetch path requires a scheme.
  if (!candidate.empty() && candidate.find("://") == std::string::npos) {
    candidate.insert(0, "https://");
  }

  if (candidate.size() > MAX_URL_LENGTH) {
    LOG_ERR("DASH", "URL too long: %u", static_cast<unsigned>(candidate.size()));
    return false;
  }
  // Empty clears the configuration (the Dashboard screen then prompts to set one).
  if (!candidate.empty() && candidate.rfind("http://", 0) != 0 && candidate.rfind("https://", 0) != 0) {
    LOG_ERR("DASH", "URL must start with http:// or https://");
    return false;
  }
  if (candidate == url) return true;  // Avoid a needless SD write (SD erase cycles)
  url = std::move(candidate);
  return saveToFile();
}
