#include <Arduino_H7_Video.h>
#include <Arduino_GigaDisplay.h>
#include <Arduino_GigaDisplayTouch.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <lvgl.h>
#include <math.h>

#include "van_background.h"

constexpr uint8_t ONE_WIRE_PIN = 2;
constexpr uint8_t FURNACE_PIN = 4;
constexpr uint8_t PUMP_PIN = 5;
constexpr uint8_t LED_RED_PIN = 6;
constexpr uint8_t LED_GREEN_PIN = 7;
constexpr uint8_t LED_BLUE_PIN = 8;

constexpr bool FURNACE_ACTIVE_HIGH = true;
constexpr bool PUMP_ACTIVE_HIGH = true;
constexpr bool LEDS_ACTIVE_HIGH = true;
constexpr bool ENABLE_AUTO_SLEEP_DEFAULT = true;

constexpr uint32_t SCREEN_SLEEP_MS = 60000UL;
constexpr uint32_t TEMP_SAMPLE_INTERVAL_MS = 3000UL;
constexpr uint16_t DS18B20_RESOLUTION_BITS = 10;

constexpr bool TOUCH_SWAP_XY = true;
constexpr bool TOUCH_INVERT_X = false;
constexpr bool TOUCH_INVERT_Y = true;
constexpr int16_t TOUCH_OFFSET_X = 0;
constexpr int16_t TOUCH_OFFSET_Y = 0;
constexpr bool SHOW_TOUCH_CALIBRATION = false;

constexpr int16_t SCREEN_WIDTH = 800;
constexpr int16_t SCREEN_HEIGHT = 480;
constexpr int16_t HEADER_HEIGHT = 58;

constexpr uint8_t MAX_SENSORS = 10;
constexpr int8_t FURNACE_SENSOR_INDEX = 4;
constexpr float DEFAULT_SETPOINT_C = 20.0f;
constexpr float FURNACE_HYSTERESIS_C = 0.5f;
constexpr char APP_VERSION[] = "version 5.7";

constexpr uint32_t COLOR_BG = 0x0C1018;
constexpr uint32_t COLOR_SURFACE = 0x161C28;
constexpr uint32_t COLOR_SURFACE_2 = 0x242D3D;
constexpr uint32_t COLOR_ACCENT = 0xA7F3D0;
constexpr uint32_t COLOR_ACCENT_2 = 0x5EEAD4;
constexpr uint32_t COLOR_TEXT = 0xF8FAFC;
constexpr uint32_t COLOR_MUTED = 0xCBD5E1;
constexpr uint32_t COLOR_RED = 0xFB7185;
constexpr uint32_t COLOR_GREEN = 0x86EFAC;
constexpr uint32_t COLOR_BLUE = 0x7DD3FC;
constexpr uint32_t COLOR_AMBER = 0xFCD34D;

struct Rect {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

struct SensorSlot {
  const char *label;
  Rect rect;
};

enum TabId : uint8_t {
  TAB_TEMPERATURES = 0,
  TAB_FURNACE = 1,
  TAB_LIGHTS = 2,
  TAB_CALIBRATION = 3
};

enum FurnaceMode : uint8_t {
  FURNACE_OFF = 0,
  FURNACE_HEAT = 1
};

enum SliderId : uint8_t {
  SLIDER_RED = 0,
  SLIDER_GREEN = 1,
  SLIDER_BLUE = 2,
  SLIDER_BRIGHTNESS = 3
};

const SensorSlot SENSOR_LAYOUT[MAX_SENSORS] = {
    {"Roof", {102, 105, 74, 22}},
    {"Cab", {195, 194, 74, 22}},
    {"Bed", {395, 220, 74, 22}},
    {"Galley", {473, 198, 74, 22}},
    {"Center", {385, 332, 74, 22}},
    {"Floor", {316, 382, 74, 22}},
    {"Rear", {623, 150, 74, 22}},
    {"Gear", {682, 328, 74, 22}},
    {"Tank", {244, 435, 96, 24}},
    {"Outside", {500, 435, 112, 24}},
};

const char *TAB_LABELS[3] = {"Temperatures", "Furnace", "Lights"};
const char *SLIDER_LABELS[4] = {"Red", "Green", "Blue", "Brightness"};

const Rect FURNACE_MINUS_RECT = {110, 316, 90, 54};
const Rect FURNACE_MODE_RECT = {270, 316, 260, 54};
const Rect FURNACE_PLUS_RECT = {600, 316, 90, 54};
const Rect LIGHT_SLIDER_TOUCH_RECTS[4] = {
    {160, 176, 500, 34},
    {160, 234, 500, 34},
    {160, 292, 500, 34},
    {160, 350, 500, 34},
};

const lv_img_dsc_t kVanBackgroundImage = {
    {LV_IMG_CF_TRUE_COLOR, 0, 0, VAN_BACKGROUND_WIDTH, VAN_BACKGROUND_HEIGHT},
    VAN_BACKGROUND_WIDTH * VAN_BACKGROUND_HEIGHT * sizeof(uint16_t),
    reinterpret_cast<const uint8_t *>(vanBackground565),
};

Arduino_H7_Video Display(800, 480, GigaDisplayShield);
GigaDisplayBacklight backlight;
Arduino_GigaDisplayTouch TouchDetector;
OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature sensors(&oneWire);

DeviceAddress sensorAddresses[MAX_SENSORS];
float sensorValuesC[MAX_SENSORS];
bool sensorPresent[MAX_SENSORS];
uint8_t detectedSensorCount = 0;
bool sensorConversionPending = false;
uint32_t sensorRequestStartedAt = 0;
uint32_t lastSensorCycleAt = 0;

TabId activeTab = TAB_TEMPERATURES;
FurnaceMode furnaceMode = FURNACE_OFF;
bool furnaceOutput = false;
float furnaceSetpointC = DEFAULT_SETPOINT_C;
bool pumpOutput = false;

uint8_t ledRed = 180;
uint8_t ledGreen = 110;
uint8_t ledBlue = 80;
uint8_t ledBrightness = 180;

bool enableAutoSleep = ENABLE_AUTO_SLEEP_DEFAULT;
bool displaySleeping = false;
bool ignoreTouchUntilRelease = false;
uint32_t lastTouchAt = 0;
uint32_t lastLvglTickAt = 0;

lv_obj_t *tabButtons[3];
lv_obj_t *pageContainers[4];
lv_obj_t *sleepButton;
lv_obj_t *sleepLabel;
lv_obj_t *pumpButton;
lv_obj_t *pumpLabel;

lv_obj_t *tempValueLabels[MAX_SENSORS];

lv_obj_t *furnaceCurrentValueLabel;
lv_obj_t *furnaceSetpointValueLabel;
lv_obj_t *furnaceStateValueLabel;
lv_obj_t *furnaceModeButton;
lv_obj_t *furnaceModeButtonLabel;
lv_obj_t *furnaceSensorNameLabel;

lv_obj_t *lightSliders[4];
lv_obj_t *lightValueLabels[4];
lv_obj_t *lightPreview;

lv_obj_t *calibrationInfoLabel;
lv_obj_t *calibrationRawLabel;
lv_obj_t *calibrationMappedLabel;
lv_obj_t *calibrationTargetLabel;
lv_obj_t *calibrationDot;

uint16_t lastRawTouchX = 0;
uint16_t lastRawTouchY = 0;
int16_t lastMappedTouchX = 0;
int16_t lastMappedTouchY = 0;
bool lastTouchPressed = false;
bool lastTopBarTouchHandled = false;
bool lastBodyTouchHandled = false;
uint8_t calibrationTargetIndex = 0;

static void tabButtonEventCb(lv_event_t *event);
static void sleepButtonEventCb(lv_event_t *event);
static void furnaceAdjustEventCb(lv_event_t *event);
static void furnaceModeEventCb(lv_event_t *event);
static void lightSliderEventCb(lv_event_t *event);
static void customTouchReadCb(lv_indev_drv_t *drv, lv_indev_data_t *data);
static void calibrationScreenEventCb(lv_event_t *event);
void handleFurnaceTouchMapped(int16_t x, int16_t y);
void handleLightsTouchMapped(int16_t x, int16_t y);

const lv_point_t CALIBRATION_TARGETS[5] = {
    {60, 88},
    {740, 88},
    {740, 430},
    {60, 430},
    {400, 240},
};

constexpr uint16_t RAW_TOPBAR_X_MIN = 420;
constexpr uint16_t RAW_TOPBAR_X_MAX = 475;
constexpr uint16_t RAW_TEMP_Y_MIN = 40;
constexpr uint16_t RAW_TEMP_Y_MAX = 180;
constexpr uint16_t RAW_FURNACE_Y_MIN = 181;
constexpr uint16_t RAW_FURNACE_Y_MAX = 330;
constexpr uint16_t RAW_LIGHTS_Y_MIN = 331;
constexpr uint16_t RAW_LIGHTS_Y_MAX = 520;
constexpr uint16_t RAW_PUMP_Y_MIN = 521;
constexpr uint16_t RAW_PUMP_Y_MAX = 640;
constexpr uint16_t RAW_SLEEP_Y_MIN = 650;
constexpr uint16_t RAW_SLEEP_Y_MAX = 790;

lv_color_t lvColor(uint32_t hex) {
  return lv_color_hex(hex);
}

bool pointInRect(int16_t x, int16_t y, const Rect &rect) {
  return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
}

void setBacklightPercent(uint8_t percent) {
  if (percent > 100) {
    percent = 100;
  }
  backlight.set(percent);
}

void wakeDisplay() {
  if (!displaySleeping) {
    return;
  }
  displaySleeping = false;
  setBacklightPercent(100);
}

void maybeSleepDisplay() {
  if (!enableAutoSleep || displaySleeping) {
    return;
  }
  if (millis() - lastTouchAt >= SCREEN_SLEEP_MS) {
    displaySleeping = true;
    setBacklightPercent(0);
  }
}

void setFurnaceOutput(bool on) {
  if (furnaceOutput == on) {
    return;
  }

  furnaceOutput = on;
  const bool pinOn = (on && FURNACE_ACTIVE_HIGH) || (!on && !FURNACE_ACTIVE_HIGH);
  digitalWrite(FURNACE_PIN, pinOn ? HIGH : LOW);
}

void setPumpOutput(bool on) {
  if (pumpOutput == on) {
    return;
  }

  pumpOutput = on;
  const bool pinOn = (on && PUMP_ACTIVE_HIGH) || (!on && !PUMP_ACTIVE_HIGH);
  digitalWrite(PUMP_PIN, pinOn ? HIGH : LOW);
}

void writeLedChannel(uint8_t pin, uint8_t value) {
  const uint8_t output = LEDS_ACTIVE_HIGH ? value : (255 - value);
  analogWrite(pin, output);
}

void applyLedOutputs() {
  const uint8_t scaledRed = static_cast<uint16_t>(ledRed) * ledBrightness / 255;
  const uint8_t scaledGreen = static_cast<uint16_t>(ledGreen) * ledBrightness / 255;
  const uint8_t scaledBlue = static_cast<uint16_t>(ledBlue) * ledBrightness / 255;

  writeLedChannel(LED_RED_PIN, scaledRed);
  writeLedChannel(LED_GREEN_PIN, scaledGreen);
  writeLedChannel(LED_BLUE_PIN, scaledBlue);
}

String formatTemperature(float valueC) {
  if (valueC == DEVICE_DISCONNECTED_C || isnan(valueC)) {
    return "--.-";
  }
  return String(valueC, 1);
}

float getSensorValueC(int8_t index) {
  if (index < 0 || index >= static_cast<int8_t>(MAX_SENSORS) || !sensorPresent[index]) {
    return DEVICE_DISCONNECTED_C;
  }
  return sensorValuesC[index];
}

uint32_t conversionDelayMsForResolution() {
  switch (DS18B20_RESOLUTION_BITS) {
    case 9:
      return 100;
    case 10:
      return 200;
    case 11:
      return 400;
    default:
      return 800;
  }
}

void discoverSensors() {
  detectedSensorCount = sensors.getDeviceCount();
  if (detectedSensorCount > MAX_SENSORS) {
    detectedSensorCount = MAX_SENSORS;
  }

  for (uint8_t i = 0; i < MAX_SENSORS; i++) {
    sensorPresent[i] = sensors.getAddress(sensorAddresses[i], i);
    sensorValuesC[i] = DEVICE_DISCONNECTED_C;
    if (sensorPresent[i]) {
      sensors.setResolution(sensorAddresses[i], DS18B20_RESOLUTION_BITS);
    }
  }
}

void updateSensorReadings() {
  const uint32_t now = millis();

  if (!sensorConversionPending && now - lastSensorCycleAt >= TEMP_SAMPLE_INTERVAL_MS) {
    sensors.requestTemperatures();
    sensorRequestStartedAt = now;
    sensorConversionPending = true;
    lastSensorCycleAt = now;
  }

  if (!sensorConversionPending || now - sensorRequestStartedAt < conversionDelayMsForResolution()) {
    return;
  }

  for (uint8_t i = 0; i < MAX_SENSORS; i++) {
    sensorValuesC[i] = sensorPresent[i] ? sensors.getTempC(sensorAddresses[i]) : DEVICE_DISCONNECTED_C;
  }

  sensorConversionPending = false;
}

void updateFurnaceControl() {
  if (furnaceMode == FURNACE_OFF) {
    setFurnaceOutput(false);
    return;
  }

  const float currentC = getSensorValueC(FURNACE_SENSOR_INDEX);
  if (currentC == DEVICE_DISCONNECTED_C || isnan(currentC)) {
    setFurnaceOutput(false);
    return;
  }

  if (!furnaceOutput && currentC <= furnaceSetpointC - FURNACE_HYSTERESIS_C) {
    setFurnaceOutput(true);
  } else if (furnaceOutput && currentC >= furnaceSetpointC + FURNACE_HYSTERESIS_C) {
    setFurnaceOutput(false);
  }
}

void updateSleepButtonUi() {
  lv_label_set_text(sleepLabel, enableAutoSleep ? "Sleep On" : "Sleep Off");
  lv_obj_set_style_bg_color(sleepButton, enableAutoSleep ? lvColor(COLOR_ACCENT) : lvColor(COLOR_SURFACE_2), 0);
  lv_obj_set_style_text_color(sleepButton, enableAutoSleep ? lvColor(COLOR_BG) : lvColor(COLOR_TEXT), 0);
}

void updatePumpButtonUi() {
  lv_label_set_text(pumpLabel, pumpOutput ? "Pump On" : "Pump Off");
  lv_obj_set_style_bg_color(pumpButton, pumpOutput ? lvColor(COLOR_ACCENT) : lvColor(COLOR_SURFACE_2), 0);
  lv_obj_set_style_text_color(pumpButton, pumpOutput ? lvColor(COLOR_BG) : lvColor(COLOR_TEXT), 0);
}

void updateTabUi() {
  for (uint8_t i = 0; i < 3; i++) {
    const bool selected = activeTab == static_cast<TabId>(i);
    lv_obj_set_style_bg_color(tabButtons[i], selected ? lvColor(COLOR_ACCENT) : lvColor(COLOR_SURFACE_2), 0);
    lv_obj_set_style_text_color(tabButtons[i], selected ? lvColor(COLOR_BG) : lvColor(COLOR_TEXT), 0);
    if (selected) {
      lv_obj_clear_flag(pageContainers[i], LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(pageContainers[i], LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (activeTab == TAB_CALIBRATION) {
    for (uint8_t i = 0; i < 3; i++) {
      lv_obj_add_flag(pageContainers[i], LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_clear_flag(pageContainers[TAB_CALIBRATION], LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(pageContainers[TAB_CALIBRATION], LV_OBJ_FLAG_HIDDEN);
  }
}

void updateTemperatureUi() {
  static const char *previewValues[MAX_SENSORS] = {
      "51", "53", "55", "57", "59",
      "61", "63", "65", "67", "69",
  };
  for (uint8_t i = 0; i < MAX_SENSORS; i++) {
    lv_label_set_text(tempValueLabels[i], previewValues[i]);
  }
}

void updateFurnaceUi() {
  lv_label_set_text(furnaceCurrentValueLabel, formatTemperature(getSensorValueC(FURNACE_SENSOR_INDEX)).c_str());
  lv_label_set_text(furnaceSetpointValueLabel, formatTemperature(furnaceSetpointC).c_str());
  lv_label_set_text(furnaceStateValueLabel, furnaceOutput ? "ON" : "OFF");
  lv_label_set_text(furnaceModeButtonLabel, furnaceMode == FURNACE_HEAT ? "Heating Enabled" : "Heating Off");
  lv_label_set_text(furnaceSensorNameLabel, SENSOR_LAYOUT[FURNACE_SENSOR_INDEX].label);
  lv_obj_set_style_bg_color(furnaceModeButton, furnaceMode == FURNACE_HEAT ? lvColor(COLOR_GREEN) : lvColor(COLOR_RED), 0);
  lv_obj_set_style_text_color(furnaceModeButton, lvColor(COLOR_BG), 0);
}

uint8_t sliderValueById(uint8_t sliderId) {
  switch (sliderId) {
    case SLIDER_RED:
      return ledRed;
    case SLIDER_GREEN:
      return ledGreen;
    case SLIDER_BLUE:
      return ledBlue;
    default:
      return ledBrightness;
  }
}

void setSliderValueById(uint8_t sliderId, uint8_t value) {
  switch (sliderId) {
    case SLIDER_RED:
      ledRed = value;
      break;
    case SLIDER_GREEN:
      ledGreen = value;
      break;
    case SLIDER_BLUE:
      ledBlue = value;
      break;
    case SLIDER_BRIGHTNESS:
      ledBrightness = value;
      break;
  }
}

void updateLightsUi() {
  const lv_color_t previewColor = lv_color_make(
      static_cast<uint16_t>(ledRed) * ledBrightness / 255,
      static_cast<uint16_t>(ledGreen) * ledBrightness / 255,
      static_cast<uint16_t>(ledBlue) * ledBrightness / 255);

  for (uint8_t i = 0; i < 4; i++) {
    lv_slider_set_value(lightSliders[i], sliderValueById(i), LV_ANIM_OFF);
    lv_label_set_text_fmt(lightValueLabels[i], "%u", sliderValueById(i));
  }

  lv_obj_set_style_bg_color(lightPreview, previewColor, 0);
}

void updateCalibrationUi() {
  if (calibrationInfoLabel == NULL) {
    return;
  }

  lv_label_set_text_fmt(calibrationRawLabel, "Raw: %u, %u", lastRawTouchX, lastRawTouchY);
  lv_label_set_text_fmt(calibrationMappedLabel, "Mapped: %d, %d", lastMappedTouchX, lastMappedTouchY);
  lv_label_set_text_fmt(calibrationTargetLabel, "Target %u: %d, %d", calibrationTargetIndex + 1,
                        CALIBRATION_TARGETS[calibrationTargetIndex].x, CALIBRATION_TARGETS[calibrationTargetIndex].y);

  if (lastTouchPressed) {
    lv_obj_clear_flag(calibrationDot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(calibrationDot, lastMappedTouchX - 7, lastMappedTouchY - 7);
  } else {
    lv_obj_add_flag(calibrationDot, LV_OBJ_FLAG_HIDDEN);
  }
}

void updateUiFromState() {
  updateTabUi();
  updateSleepButtonUi();
  updatePumpButtonUi();
  updateTemperatureUi();
  updateFurnaceUi();
  updateLightsUi();
  updateCalibrationUi();
}

void handleTopBarTouchRaw(uint16_t rawX, uint16_t rawY) {
  if (rawX < RAW_TOPBAR_X_MIN || rawX > RAW_TOPBAR_X_MAX) {
    return;
  }

  if (rawY >= RAW_TEMP_Y_MIN && rawY <= RAW_TEMP_Y_MAX) {
    activeTab = TAB_TEMPERATURES;
    updateUiFromState();
    return;
  }

  if (rawY >= RAW_FURNACE_Y_MIN && rawY <= RAW_FURNACE_Y_MAX) {
    activeTab = TAB_FURNACE;
    updateUiFromState();
    return;
  }

  if (rawY >= RAW_LIGHTS_Y_MIN && rawY <= RAW_LIGHTS_Y_MAX) {
    activeTab = TAB_LIGHTS;
    updateUiFromState();
    return;
  }

  if (rawY >= RAW_PUMP_Y_MIN && rawY <= RAW_PUMP_Y_MAX) {
    setPumpOutput(!pumpOutput);
    updatePumpButtonUi();
    return;
  }

  if (rawY >= RAW_SLEEP_Y_MIN && rawY <= RAW_SLEEP_Y_MAX) {
    enableAutoSleep = !enableAutoSleep;
    if (!enableAutoSleep) {
      wakeDisplay();
    }
    updateSleepButtonUi();
  }
}

void handleFurnaceTouchMapped(int16_t x, int16_t y) {
  if (pointInRect(x, y, FURNACE_MINUS_RECT)) {
    furnaceSetpointC -= 0.5f;
  } else if (pointInRect(x, y, FURNACE_PLUS_RECT)) {
    furnaceSetpointC += 0.5f;
  } else if (pointInRect(x, y, FURNACE_MODE_RECT)) {
    furnaceMode = furnaceMode == FURNACE_HEAT ? FURNACE_OFF : FURNACE_HEAT;
  } else {
    return;
  }

  if (furnaceSetpointC < 5.0f) {
    furnaceSetpointC = 5.0f;
  }
  if (furnaceSetpointC > 35.0f) {
    furnaceSetpointC = 35.0f;
  }

  updateFurnaceControl();
  updateFurnaceUi();
}

void handleLightsTouchMapped(int16_t x, int16_t y) {
  for (uint8_t i = 0; i < 4; i++) {
    if (!pointInRect(x, y, LIGHT_SLIDER_TOUCH_RECTS[i])) {
      continue;
    }

    const Rect &rect = LIGHT_SLIDER_TOUCH_RECTS[i];
    int16_t clampedX = constrain(x, rect.x, rect.x + rect.w);
    uint8_t value = static_cast<int32_t>(clampedX - rect.x) * 255 / rect.w;
    setSliderValueById(i, value);
    applyLedOutputs();
    updateLightsUi();
    return;
  }
}

void deleteExistingIndevs() {
  lv_indev_t *indev = lv_indev_get_next(NULL);
  while (indev != NULL) {
    lv_indev_t *next = lv_indev_get_next(indev);
    lv_indev_delete(indev);
    indev = next;
  }
}

void registerCustomTouchDriver() {
  deleteExistingIndevs();

  static lv_indev_drv_t indevDrv;
  lv_indev_drv_init(&indevDrv);
  indevDrv.type = LV_INDEV_TYPE_POINTER;
  indevDrv.read_cb = customTouchReadCb;
  lv_indev_drv_register(&indevDrv);
}

lv_obj_t *createTabButton(lv_obj_t *parent, const char *text, int16_t x, int16_t w, TabId tabId) {
  lv_obj_t *button = lv_btn_create(parent);
  lv_obj_set_size(button, w, 34);
  lv_obj_set_pos(button, x, 2);
  (void)tabId;
  lv_obj_set_style_radius(button, 18, 0);
  lv_obj_set_style_border_width(button, 1, 0);
  lv_obj_set_style_border_color(button, lvColor(COLOR_MUTED), 0);
  lv_obj_set_style_shadow_width(button, 0, 0);
  lv_obj_set_style_pad_left(button, 16, 0);
  lv_obj_set_style_pad_right(button, 16, 0);
  lv_obj_set_style_pad_top(button, 0, 0);
  lv_obj_set_style_pad_bottom(button, 0, 0);
  lv_obj_clear_flag(button, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *label = lv_label_create(button);
  lv_label_set_text(label, text);
  lv_obj_center(label);
  return button;
}

lv_obj_t *createPageBase(lv_obj_t *parent) {
  lv_obj_t *page = lv_obj_create(parent);
  lv_obj_set_size(page, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_pos(page, 0, 0);
  lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(page, 0, 0);
  lv_obj_set_style_pad_all(page, 0, 0);
  lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
  return page;
}

lv_obj_t *createCard(lv_obj_t *parent, int16_t x, int16_t y, int16_t w, int16_t h) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_set_size(card, w, h);
  lv_obj_set_pos(card, x, y);
  lv_obj_set_style_radius(card, 18, 0);
  lv_obj_set_style_bg_color(card, lvColor(COLOR_SURFACE), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(card, lvColor(COLOR_SURFACE_2), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_shadow_width(card, 0, 0);
  lv_obj_set_style_pad_all(card, 14, 0);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  return card;
}

void createTemperaturePage(lv_obj_t *parent) {
  lv_obj_t *bg = lv_img_create(parent);
  lv_img_set_src(bg, &kVanBackgroundImage);
  lv_obj_set_pos(bg, 0, 0);

  for (uint8_t i = 0; i < MAX_SENSORS; i++) {
    lv_obj_t *label = lv_label_create(parent);
    tempValueLabels[i] = label;
    lv_label_set_text(label, "--.-");
    lv_obj_set_style_text_color(label, lvColor(0x0F172A), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_bg_opa(label, i >= 8 ? LV_OPA_60 : LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(label, lvColor(COLOR_SURFACE), 0);
    lv_obj_set_style_radius(label, 12, 0);
    lv_obj_set_style_pad_left(label, 6, 0);
    lv_obj_set_style_pad_right(label, 6, 0);
    lv_obj_set_style_pad_top(label, 2, 0);
    lv_obj_set_style_pad_bottom(label, 2, 0);
    lv_obj_set_size(label, SENSOR_LAYOUT[i].rect.w, SENSOR_LAYOUT[i].rect.h);
    lv_obj_set_pos(label, SENSOR_LAYOUT[i].rect.x, SENSOR_LAYOUT[i].rect.y);
  }

  lv_obj_t *versionLabel = lv_label_create(parent);
  lv_label_set_text(versionLabel, APP_VERSION);
  lv_obj_set_style_text_color(versionLabel, lvColor(COLOR_MUTED), 0);
  lv_obj_set_pos(versionLabel, 682, 456);
}

lv_obj_t *createMetricCard(lv_obj_t *parent, int16_t x, const char *title, lv_obj_t **valueLabel) {
  lv_obj_t *card = createCard(parent, x, 162, 220, 118);

  lv_obj_t *titleLabel = lv_label_create(card);
  lv_label_set_text(titleLabel, title);
  lv_obj_set_style_text_color(titleLabel, lvColor(COLOR_MUTED), 0);
  lv_obj_align(titleLabel, LV_ALIGN_TOP_LEFT, 0, 0);

  *valueLabel = lv_label_create(card);
  lv_label_set_text(*valueLabel, "--.-");
  lv_obj_set_style_text_color(*valueLabel, lvColor(COLOR_TEXT), 0);
  lv_obj_align(*valueLabel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  return card;
}

void createFurnacePage(lv_obj_t *parent) {
  lv_obj_t *hero = createCard(parent, 24, 82, 752, 64);

  lv_obj_t *title = lv_label_create(hero);
  lv_label_set_text(title, "Furnace Controller");
  lv_obj_set_style_text_color(title, lvColor(COLOR_TEXT), 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

  lv_obj_t *subtitle = lv_label_create(hero);
  lv_label_set_text(subtitle, "Thermostat output for the van furnace.");
  lv_obj_set_style_text_color(subtitle, lvColor(COLOR_MUTED), 0);
  lv_obj_align(subtitle, LV_ALIGN_BOTTOM_LEFT, 0, 0);

  createMetricCard(parent, 24, "Current", &furnaceCurrentValueLabel);
  createMetricCard(parent, 290, "Setpoint", &furnaceSetpointValueLabel);
  createMetricCard(parent, 556, "Output", &furnaceStateValueLabel);

  lv_obj_t *minusButton = lv_btn_create(parent);
  lv_obj_set_size(minusButton, 90, 54);
  lv_obj_set_pos(minusButton, 110, 316);
  lv_obj_set_style_radius(minusButton, 18, 0);
  lv_obj_set_style_bg_color(minusButton, lvColor(COLOR_SURFACE_2), 0);
  lv_obj_set_style_border_width(minusButton, 0, 0);
  lv_obj_clear_flag(minusButton, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t *minusLabel = lv_label_create(minusButton);
  lv_label_set_text(minusLabel, "-");
  lv_obj_set_style_text_font(minusLabel, &lv_font_montserrat_14, 0);
  lv_obj_center(minusLabel);

  lv_obj_t *plusButton = lv_btn_create(parent);
  lv_obj_set_size(plusButton, 90, 54);
  lv_obj_set_pos(plusButton, 600, 316);
  lv_obj_set_style_radius(plusButton, 18, 0);
  lv_obj_set_style_bg_color(plusButton, lvColor(COLOR_SURFACE_2), 0);
  lv_obj_set_style_border_width(plusButton, 0, 0);
  lv_obj_clear_flag(plusButton, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t *plusLabel = lv_label_create(plusButton);
  lv_label_set_text(plusLabel, "+");
  lv_obj_set_style_text_font(plusLabel, &lv_font_montserrat_14, 0);
  lv_obj_center(plusLabel);

  furnaceModeButton = lv_btn_create(parent);
  lv_obj_set_size(furnaceModeButton, 260, 54);
  lv_obj_set_pos(furnaceModeButton, 270, 316);
  lv_obj_set_style_radius(furnaceModeButton, 18, 0);
  lv_obj_set_style_border_width(furnaceModeButton, 0, 0);
  lv_obj_clear_flag(furnaceModeButton, LV_OBJ_FLAG_CLICKABLE);
  furnaceModeButtonLabel = lv_label_create(furnaceModeButton);
  lv_label_set_text(furnaceModeButtonLabel, "Heating Off");
  lv_obj_center(furnaceModeButtonLabel);

  lv_obj_t *footer = lv_label_create(parent);
  lv_label_set_text(footer, "Sensor");
  lv_obj_set_style_text_color(footer, lvColor(COLOR_MUTED), 0);
  lv_obj_set_pos(footer, 26, 398);

  furnaceSensorNameLabel = lv_label_create(parent);
  lv_label_set_text(furnaceSensorNameLabel, SENSOR_LAYOUT[FURNACE_SENSOR_INDEX].label);
  lv_obj_set_style_text_color(furnaceSensorNameLabel, lvColor(COLOR_TEXT), 0);
  lv_obj_set_pos(furnaceSensorNameLabel, 26, 418);

  lv_obj_t *hysteresis = lv_label_create(parent);
  lv_label_set_text(hysteresis, "Hysteresis +/-0.5C");
  lv_obj_set_style_text_color(hysteresis, lvColor(COLOR_MUTED), 0);
  lv_obj_set_pos(hysteresis, 590, 418);
}

lv_color_t sliderColor(uint8_t sliderId) {
  switch (sliderId) {
    case SLIDER_RED:
      return lvColor(COLOR_RED);
    case SLIDER_GREEN:
      return lvColor(COLOR_GREEN);
    case SLIDER_BLUE:
      return lvColor(COLOR_BLUE);
    default:
      return lvColor(COLOR_AMBER);
  }
}

void createLightsPage(lv_obj_t *parent) {
  lv_obj_t *hero = createCard(parent, 24, 82, 752, 64);

  lv_obj_t *title = lv_label_create(hero);
  lv_label_set_text(title, "Accent Lights");
  lv_obj_set_style_text_color(title, lvColor(COLOR_TEXT), 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

  lv_obj_t *subtitle = lv_label_create(hero);
  lv_label_set_text(subtitle, "Set RGB color and overall brightness.");
  lv_obj_set_style_text_color(subtitle, lvColor(COLOR_MUTED), 0);
  lv_obj_align(subtitle, LV_ALIGN_BOTTOM_LEFT, 0, 0);

  lightPreview = lv_obj_create(parent);
  lv_obj_set_size(lightPreview, 126, 46);
  lv_obj_set_pos(lightPreview, 624, 92);
  lv_obj_set_style_radius(lightPreview, 16, 0);
  lv_obj_set_style_border_color(lightPreview, lvColor(COLOR_MUTED), 0);
  lv_obj_set_style_border_width(lightPreview, 1, 0);
  lv_obj_set_style_shadow_width(lightPreview, 0, 0);

  for (uint8_t i = 0; i < 4; i++) {
    const int16_t y = 184 + (i * 58);

    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, SLIDER_LABELS[i]);
    lv_obj_set_style_text_color(label, lvColor(COLOR_TEXT), 0);
    lv_obj_set_pos(label, 42, y + 4);

    lightSliders[i] = lv_slider_create(parent);
    lv_obj_set_size(lightSliders[i], 470, 10);
    lv_obj_set_pos(lightSliders[i], 170, y + 8);
    lv_slider_set_range(lightSliders[i], 0, 255);
    lv_obj_set_style_bg_color(lightSliders[i], lvColor(COLOR_SURFACE_2), LV_PART_MAIN);
    lv_obj_set_style_bg_color(lightSliders[i], sliderColor(i), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(lightSliders[i], lvColor(COLOR_TEXT), LV_PART_KNOB);
    lv_obj_set_style_shadow_width(lightSliders[i], 0, LV_PART_KNOB);
    lv_obj_clear_flag(lightSliders[i], LV_OBJ_FLAG_CLICKABLE);

    lightValueLabels[i] = lv_label_create(parent);
    lv_label_set_text(lightValueLabels[i], "0");
    lv_obj_set_style_text_color(lightValueLabels[i], lvColor(COLOR_MUTED), 0);
    lv_obj_set_pos(lightValueLabels[i], 664, y + 4);
  }
}

void createCalibrationCrosshair(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, bool active) {
  const lv_color_t color = active ? lvColor(COLOR_RED) : lvColor(COLOR_ACCENT);

  lv_obj_t *h = lv_obj_create(parent);
  lv_obj_set_size(h, 30, 2);
  lv_obj_set_pos(h, x - 15, y - 1);
  lv_obj_set_style_bg_color(h, color, 0);
  lv_obj_set_style_bg_opa(h, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(h, 0, 0);
  lv_obj_set_style_radius(h, 0, 0);
  lv_obj_clear_flag(h, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *v = lv_obj_create(parent);
  lv_obj_set_size(v, 2, 30);
  lv_obj_set_pos(v, x - 1, y - 15);
  lv_obj_set_style_bg_color(v, color, 0);
  lv_obj_set_style_bg_opa(v, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(v, 0, 0);
  lv_obj_set_style_radius(v, 0, 0);
  lv_obj_clear_flag(v, LV_OBJ_FLAG_SCROLLABLE);
}

void createCalibrationPage(lv_obj_t *parent) {
  lv_obj_t *title = lv_label_create(parent);
  lv_label_set_text(title, "Touch + Tab Calibration");
  lv_obj_set_style_text_color(title, lvColor(COLOR_TEXT), 0);
  lv_obj_set_pos(title, 26, 74);

  calibrationInfoLabel = lv_label_create(parent);
  lv_label_set_text(calibrationInfoLabel, "Tap the red target. Watch the blue dot and whether the top tabs react.");
  lv_obj_set_style_text_color(calibrationInfoLabel, lvColor(COLOR_MUTED), 0);
  lv_obj_set_pos(calibrationInfoLabel, 26, 98);

  calibrationRawLabel = lv_label_create(parent);
  lv_label_set_text(calibrationRawLabel, "Raw: --, --");
  lv_obj_set_style_text_color(calibrationRawLabel, lvColor(COLOR_TEXT), 0);
  lv_obj_set_pos(calibrationRawLabel, 26, 126);

  calibrationMappedLabel = lv_label_create(parent);
  lv_label_set_text(calibrationMappedLabel, "Mapped: --, --");
  lv_obj_set_style_text_color(calibrationMappedLabel, lvColor(COLOR_TEXT), 0);
  lv_obj_set_pos(calibrationMappedLabel, 26, 148);

  calibrationTargetLabel = lv_label_create(parent);
  lv_label_set_text(calibrationTargetLabel, "Target 1");
  lv_obj_set_style_text_color(calibrationTargetLabel, lvColor(COLOR_TEXT), 0);
  lv_obj_set_pos(calibrationTargetLabel, 26, 170);

  lv_obj_t *tabHint = lv_label_create(parent);
  lv_label_set_text(tabHint, "Top row should hit: Temperatures | Furnace | Lights");
  lv_obj_set_style_text_color(tabHint, lvColor(COLOR_MUTED), 0);
  lv_obj_set_pos(tabHint, 26, 194);

  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_set_size(box, 748, 210);
  lv_obj_set_pos(box, 26, 234);
  lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(box, lvColor(COLOR_SURFACE_2), 0);
  lv_obj_set_style_border_width(box, 1, 0);
  lv_obj_set_style_radius(box, 18, 0);
  lv_obj_set_style_shadow_width(box, 0, 0);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *boxLabel = lv_label_create(box);
  lv_label_set_text(boxLabel, "Calibration field");
  lv_obj_set_style_text_color(boxLabel, lvColor(COLOR_MUTED), 0);
  lv_obj_align(boxLabel, LV_ALIGN_TOP_MID, 0, 10);

  for (uint8_t i = 0; i < 5; i++) {
    createCalibrationCrosshair(parent, CALIBRATION_TARGETS[i].x, CALIBRATION_TARGETS[i].y, i == calibrationTargetIndex);
  }

  calibrationDot = lv_obj_create(parent);
  lv_obj_set_size(calibrationDot, 14, 14);
  lv_obj_set_style_radius(calibrationDot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(calibrationDot, lvColor(COLOR_BLUE), 0);
  lv_obj_set_style_bg_opa(calibrationDot, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(calibrationDot, 0, 0);
  lv_obj_add_flag(calibrationDot, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(calibrationDot, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_add_event_cb(parent, calibrationScreenEventCb, LV_EVENT_PRESSED, NULL);
}

void createUi() {
  lv_obj_t *screen = lv_scr_act();
  lv_obj_set_style_bg_color(screen, lvColor(COLOR_BG), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(screen, lvColor(COLOR_TEXT), 0);

  pageContainers[TAB_TEMPERATURES] = createPageBase(screen);
  pageContainers[TAB_FURNACE] = createPageBase(screen);
  pageContainers[TAB_LIGHTS] = createPageBase(screen);
  pageContainers[TAB_CALIBRATION] = createPageBase(screen);

  createTemperaturePage(pageContainers[TAB_TEMPERATURES]);
  createFurnacePage(pageContainers[TAB_FURNACE]);
  createLightsPage(pageContainers[TAB_LIGHTS]);
  createCalibrationPage(pageContainers[TAB_CALIBRATION]);

  lv_obj_t *header = lv_obj_create(screen);
  lv_obj_set_size(header, 780, 44);
  lv_obj_set_pos(header, 10, 0);
  lv_obj_set_style_radius(header, 22, 0);
  lv_obj_set_style_bg_color(header, lvColor(COLOR_BG), 0);
  lv_obj_set_style_bg_opa(header, LV_OPA_80, 0);
  lv_obj_set_style_border_color(header, lvColor(COLOR_SURFACE_2), 0);
  lv_obj_set_style_border_width(header, 1, 0);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_set_style_shadow_width(header, 0, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  tabButtons[TAB_TEMPERATURES] = createTabButton(header, TAB_LABELS[TAB_TEMPERATURES], 12, 152, TAB_TEMPERATURES);
  tabButtons[TAB_FURNACE] = createTabButton(header, TAB_LABELS[TAB_FURNACE], 174, 126, TAB_FURNACE);
  tabButtons[TAB_LIGHTS] = createTabButton(header, TAB_LABELS[TAB_LIGHTS], 310, 118, TAB_LIGHTS);

  pumpButton = lv_btn_create(header);
  lv_obj_set_size(pumpButton, 104, 34);
  lv_obj_set_pos(pumpButton, 458, 2);
  lv_obj_set_style_radius(pumpButton, 18, 0);
  lv_obj_set_style_border_width(pumpButton, 1, 0);
  lv_obj_set_style_border_color(pumpButton, lvColor(COLOR_MUTED), 0);
  lv_obj_set_style_shadow_width(pumpButton, 0, 0);
  lv_obj_clear_flag(pumpButton, LV_OBJ_FLAG_CLICKABLE);

  pumpLabel = lv_label_create(pumpButton);
  lv_label_set_text(pumpLabel, "Pump Off");
  lv_obj_center(pumpLabel);

  sleepButton = lv_btn_create(header);
  lv_obj_set_size(sleepButton, 104, 34);
  lv_obj_set_pos(sleepButton, 666, 2);
  lv_obj_set_style_radius(sleepButton, 18, 0);
  lv_obj_set_style_border_width(sleepButton, 1, 0);
  lv_obj_set_style_border_color(sleepButton, lvColor(COLOR_MUTED), 0);
  lv_obj_set_style_shadow_width(sleepButton, 0, 0);
  lv_obj_clear_flag(sleepButton, LV_OBJ_FLAG_CLICKABLE);

  sleepLabel = lv_label_create(sleepButton);
  lv_label_set_text(sleepLabel, "Sleep On");
  lv_obj_center(sleepLabel);

  activeTab = SHOW_TOUCH_CALIBRATION ? TAB_CALIBRATION : TAB_TEMPERATURES;

  updateUiFromState();
}

static void tabButtonEventCb(lv_event_t *event) {
  activeTab = static_cast<TabId>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
  updateTabUi();
}

static void sleepButtonEventCb(lv_event_t *event) {
  (void)event;
  enableAutoSleep = !enableAutoSleep;
  if (!enableAutoSleep) {
    wakeDisplay();
  }
  updateSleepButtonUi();
}

static void furnaceAdjustEventCb(lv_event_t *event) {
  const int delta = reinterpret_cast<intptr_t>(lv_event_get_user_data(event));
  furnaceSetpointC += delta > 0 ? 0.5f : -0.5f;

  if (furnaceSetpointC < 5.0f) {
    furnaceSetpointC = 5.0f;
  }
  if (furnaceSetpointC > 35.0f) {
    furnaceSetpointC = 35.0f;
  }

  updateFurnaceControl();
  updateFurnaceUi();
}

static void furnaceModeEventCb(lv_event_t *event) {
  (void)event;
  furnaceMode = furnaceMode == FURNACE_HEAT ? FURNACE_OFF : FURNACE_HEAT;
  updateFurnaceControl();
  updateFurnaceUi();
}

static void lightSliderEventCb(lv_event_t *event) {
  lv_obj_t *slider = lv_event_get_target(event);
  const uint8_t sliderId = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
  setSliderValueById(sliderId, lv_slider_get_value(slider));
  applyLedOutputs();
  updateLightsUi();
}

static void calibrationScreenEventCb(lv_event_t *event) {
  (void)event;
  calibrationTargetIndex = (calibrationTargetIndex + 1) % 5;
  lv_obj_clean(pageContainers[TAB_CALIBRATION]);
  createCalibrationPage(pageContainers[TAB_CALIBRATION]);
  updateCalibrationUi();
}

static void customTouchReadCb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  (void)drv;

  GDTpoint_t points[5];
  const uint8_t contacts = TouchDetector.getTouchPoints(points);

  if (contacts == 0) {
    data->state = LV_INDEV_STATE_REL;
    lastTouchPressed = false;
    lastTopBarTouchHandled = false;
    lastBodyTouchHandled = false;
    updateCalibrationUi();
    ignoreTouchUntilRelease = false;
    return;
  }

  lastTouchAt = millis();

  if (displaySleeping) {
    wakeDisplay();
    ignoreTouchUntilRelease = true;
  }

  if (ignoreTouchUntilRelease) {
    data->state = LV_INDEV_STATE_REL;
    return;
  }

  int16_t x = points[0].x;
  int16_t y = points[0].y;
  lastRawTouchX = x;
  lastRawTouchY = y;

  if (TOUCH_SWAP_XY) {
    const int16_t temp = x;
    x = y;
    y = temp;
  }

  if (TOUCH_INVERT_X) {
    x = SCREEN_WIDTH - x;
  }

  if (TOUCH_INVERT_Y) {
    y = SCREEN_HEIGHT - y;
  }

  x += TOUCH_OFFSET_X;
  y += TOUCH_OFFSET_Y;

  x = constrain(x, 0, SCREEN_WIDTH - 1);
  y = constrain(y, 0, SCREEN_HEIGHT - 1);
  lastMappedTouchX = x;
  lastMappedTouchY = y;
  lastTouchPressed = true;

  if (!lastTopBarTouchHandled) {
    handleTopBarTouchRaw(lastRawTouchX, lastRawTouchY);
    lastTopBarTouchHandled = true;
  }

  if (activeTab == TAB_FURNACE && !lastBodyTouchHandled) {
    handleFurnaceTouchMapped(x, y);
    lastBodyTouchHandled = true;
  } else if (activeTab == TAB_LIGHTS) {
    handleLightsTouchMapped(x, y);
  }

  updateCalibrationUi();

  data->state = LV_INDEV_STATE_PR;
  data->point.x = x;
  data->point.y = y;
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(FURNACE_PIN, OUTPUT);
  setFurnaceOutput(false);
  pinMode(PUMP_PIN, OUTPUT);
  setPumpOutput(false);

  analogWriteResolution(8);
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);
  applyLedOutputs();

  Display.begin();
  backlight.begin();
  setBacklightPercent(100);

  TouchDetector.begin();
  registerCustomTouchDriver();

  sensors.begin();
  sensors.setWaitForConversion(false);
  discoverSensors();
  sensors.requestTemperatures();
  sensorRequestStartedAt = millis();
  sensorConversionPending = true;
  lastSensorCycleAt = millis();

  lastTouchAt = millis();
  createUi();
}

void loop() {
  updateSensorReadings();
  updateFurnaceControl();
  updateTemperatureUi();
  updateFurnaceUi();
  maybeSleepDisplay();

  const uint32_t now = millis();
  if (now - lastLvglTickAt >= 5) {
    lv_tick_inc(now - lastLvglTickAt);
    lv_timer_handler();
    lastLvglTickAt = now;
  }

  delay(5);
}
