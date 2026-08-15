#include "DashboardStore.h"

#include <Logging.h>

void DashboardStore::toJson(JsonDocument& doc) const {
  doc["url"] = url;
  doc["clientsUrl"] = clientsUrl;
  doc["newsUrl"] = newsUrl;
  doc["fontSize"] = fontSize;
}

bool DashboardStore::fromJson(const JsonVariantConst doc) {
  if (!doc.is<JsonObjectConst>()) return false;
  url.assign(doc["url"] | "");
  if (url.size() > MAX_URL_LENGTH) url.clear();
  // Absent in files written before the clients page existed: an empty string
  // is the correct upgrade, so no resave is needed.
  clientsUrl.assign(doc["clientsUrl"] | "");
  if (clientsUrl.size() > MAX_URL_LENGTH) clientsUrl.clear();
  newsUrl.assign(doc["newsUrl"] | "");
  if (newsUrl.size() > MAX_URL_LENGTH) newsUrl.clear();
  fontSize = doc["fontSize"] | static_cast<uint8_t>(1);
  if (fontSize >= FONT_SIZE_COUNT) fontSize = 1;
  return true;
}

bool DashboardStore::normalise(const std::string& in, std::string& out) {
  std::string candidate = in;

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
  // Empty clears the field.
  if (!candidate.empty() && candidate.rfind("http://", 0) != 0 && candidate.rfind("https://", 0) != 0) {
    LOG_ERR("DASH", "URL must start with http:// or https://");
    return false;
  }

  out = std::move(candidate);
  return true;
}

bool DashboardStore::setUrl(const std::string& newUrl) {
  std::string candidate;
  if (!normalise(newUrl, candidate)) return false;
  if (candidate == url) return true;  // Avoid a needless SD write (SD erase cycles)
  url = std::move(candidate);
  return saveToFile();
}

bool DashboardStore::setClientsUrl(const std::string& newUrl) {
  std::string candidate;
  if (!normalise(newUrl, candidate)) return false;
  if (candidate == clientsUrl) return true;
  clientsUrl = std::move(candidate);
  return saveToFile();
}

bool DashboardStore::setNewsUrl(const std::string& newUrl) {
  std::string candidate;
  if (!normalise(newUrl, candidate)) return false;
  if (candidate == newsUrl) return true;
  newsUrl = std::move(candidate);
  return saveToFile();
}

const std::string& DashboardStore::getSectionUrl(const uint8_t section) const {
  static const std::string empty;
  switch (section) {
    case 0:
      return url;
    case 1:
      return clientsUrl;
    case 2:
      return newsUrl;
    default:
      return empty;
  }
}

bool DashboardStore::setFontSize(const uint8_t size) {
  const uint8_t clamped = size >= FONT_SIZE_COUNT ? static_cast<uint8_t>(FONT_SIZE_COUNT - 1) : size;
  if (clamped == fontSize) return true;
  fontSize = clamped;
  return saveToFile();
}
