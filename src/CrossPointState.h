#pragma once
#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <cstdint>
#include <string>

class CrossPointState : public PersistableStore<CrossPointState> {
  CrossPointState() = default;

  friend class PersistableStore<CrossPointState>;

 public:
  static constexpr uint8_t SLEEP_RECENT_COUNT = 16;

  std::string openEpubPath;
  uint16_t recentSleepImages[SLEEP_RECENT_COUNT] = {};  // circular buffer of recent wallpaper indices
  uint8_t recentSleepPos = 0;                           // next write slot
  uint8_t recentSleepFill = 0;                          // valid entries (0..SLEEP_RECENT_COUNT)
  uint8_t readerActivityLoadCount = 0;

  // Which screen was on display when the device last slept, so wake can return
  // to it. Only screens worth resuming are tracked; every other screen records
  // Home, because waking into a half-finished Settings or network picker is
  // never what the user meant.
  enum LAST_SLEEP_ACTIVITY : uint8_t {
    SLEEP_FROM_HOME = 0,
    SLEEP_FROM_READER = 1,
    SLEEP_FROM_DASHBOARD = 2,
    LAST_SLEEP_ACTIVITY_COUNT
  };
  uint8_t lastSleepActivity = SLEEP_FROM_HOME;
  bool lastSleepWasReader() const { return lastSleepActivity == SLEEP_FROM_READER; }

  // MoJo Active view position, restored on wake. Updated in memory as the user
  // moves around; the pre-sleep saveToFile() in enterDeepSleep() is what
  // persists it, so a tab switch or page turn costs no SPIFFS write of its own.
  uint8_t dashboardSection = 0;
  uint8_t dashboardPage = 0;
  // Boot-loop guard for the wake-into-dashboard path, mirroring
  // readerActivityLoadCount: bumped before entering, cleared by the activity
  // once it reaches a stable screen. Non-zero at wake means the last attempt
  // did not get that far, so route to home instead of retrying forever.
  uint8_t dashboardActivityLoadCount = 0;

  bool showBootScreen = true;

  static const char* getFilePath() { return "/.crosspoint/state.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  // Returns true if idx was shown within the last checkCount picks.
  // Walks backwards from the most recently written slot.
  bool isRecentSleep(uint16_t idx, uint8_t checkCount) const;

  void pushRecentSleep(uint16_t idx);
};

// Helper macro to access state
#define APP_STATE CrossPointState::getInstance()
