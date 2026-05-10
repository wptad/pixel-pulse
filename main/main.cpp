#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <algorithm>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "led_strip.h"

// ==========================================
// Arduino Compatibility Layer (ESP-IDF Shim)
// ==========================================

#define HIGH 1
#define LOW 0
#define INPUT_PULLUP 2
#define OUTPUT 3

typedef uint8_t byte;

static inline void pinMode(int pin, int mode) {
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << pin);
    if (mode == INPUT_PULLUP) {
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.intr_type = GPIO_INTR_DISABLE;
    } else if (mode == OUTPUT) {
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.intr_type = GPIO_INTR_DISABLE;
    }
    gpio_config(&io_conf);
}

static inline int digitalRead(int pin) {
    return gpio_get_level((gpio_num_t)pin);
}

static inline void digitalWrite(int pin, int level) {
    gpio_set_level((gpio_num_t)pin, level);
}

static inline void delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static inline uint32_t millis() {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

// ESP32 hardware random
static inline uint32_t random(uint32_t max_val) {
    if (max_val == 0) return 0;
    return esp_random() % max_val;
}
static inline void randomSeed(uint32_t seed) { (void)seed; }
static inline int analogRead(int pin) { return 0; }

#ifndef constrain
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#endif

// NeoPixel Shim
#define NEO_GRB 0
#define NEO_KHZ800 0
typedef int neoPixelType;

class Adafruit_NeoPixel {
private:
    uint16_t numLeds;
    int gpioPin;
    uint32_t *buffer;
    uint8_t brightness;
    led_strip_handle_t led_strip;

public:
    Adafruit_NeoPixel(uint16_t n, int16_t p, neoPixelType t = 0) 
        : numLeds(n), gpioPin(p), brightness(255), led_strip(NULL) {
        buffer = new uint32_t[n]();
    }
    ~Adafruit_NeoPixel() {
        delete[] buffer;
    }

    void begin() {
        led_strip_config_t strip_config = {};
        strip_config.strip_gpio_num = gpioPin;
        strip_config.max_leds = numLeds;
        strip_config.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
        strip_config.led_model = LED_MODEL_WS2812;

        led_strip_rmt_config_t rmt_config = {};
        rmt_config.clk_src = RMT_CLK_SRC_DEFAULT;
        rmt_config.resolution_hz = 10 * 1000 * 1000; // 10MHz
        
        esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
        if (err != ESP_OK) {
            printf("Failed to initialize LED strip: %d\n", err);
        }
    }

    static uint32_t Color(uint8_t r, uint8_t g, uint8_t b) {
        return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }

    void setBrightness(uint8_t b) {
        brightness = b;
    }

    void setPixelColor(uint16_t n, uint32_t c) {
        if (n < numLeds) buffer[n] = c;
    }

    void setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b) {
        setPixelColor(n, Color(r, g, b));
    }

    uint32_t getPixelColor(uint16_t n) const {
        if (n < numLeds) return buffer[n];
        return 0;
    }

    void fill(uint32_t c) {
        for (int i = 0; i < numLeds; i++) buffer[i] = c;
    }

    void clear() {
        for (int i = 0; i < numLeds; i++) buffer[i] = 0;
    }

    void show() {
        if (!led_strip) return;
        for (int i = 0; i < numLeds; i++) {
            uint32_t c = buffer[i];
            uint8_t r = (uint8_t)(c >> 16);
            uint8_t g = (uint8_t)(c >> 8);
            uint8_t b = (uint8_t)c;
            
            // Apply master brightness destructively on writing to the physical bus
            if (brightness < 255) {
                r = (uint16_t)((uint16_t)r * brightness) >> 8;
                g = (uint16_t)((uint16_t)g * brightness) >> 8;
                b = (uint16_t)((uint16_t)b * brightness) >> 8;
            }
            led_strip_set_pixel(led_strip, i, r, g, b);
        }
        led_strip_refresh(led_strip);
    }
};

// Forward declarations of Business Logic Functions
void setup();
void loop();
void checkModeSwitch();
void modeFeedback();
void runGameMode();
void spawnEnemy();
void checkFire();
void levelUp();
void renderGame();
void startGame();
void runLampMode();
void runShowMode();
void adjustBrightness(int delta);
void beep(int ms);
uint32_t Wheel(byte WheelPos);
void dimPixel(int i, uint8_t factor);
void showRainbow();
void showFire();
void showMeteor();
void showSparkle();
void showOcean();
void showCollide();
void showBreath();
void showScanner();
void showPlasma();
void showFirework();

// ==========================================
// Business Logic Original Code (from .ino)
// ==========================================

// --- 引脚定义 (终极纯净版：已跳过 GPIO 9 和 8 避开一切干扰) ---
#define BTN_RED     21  // 右侧最顶部
#define BTN_YELLOW  20
#define BTN_BLUE    10
#define BTN_MODE    7   // 避开了 GPIO 8 (板载蓝灯)
#define BEEP_PIN    6
#define LED_PIN     5   // 灯带控制放到最终位 (底部另一角，完全独立)
#define NUM_LEDS    144
#define MAX_BRIGHT  150

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// --- 状态与变量 ---
enum AppMode { MODE_GAME, MODE_LAMP, MODE_SHOW };
AppMode currentMode = MODE_GAME;

int brightnessLevel = 3;    // 亮度档位 0-9
int globalBrightness = 52;  // 10 + 3*14 = 52
int lampColorIndex = 0;
int showPatternIndex = 0;
bool inGame = false;
int health = 3;
int score = 0;
float currentSpeed = 0.4f;
float speedInc = 0.1f;

// 游戏物体
float enemyPos, playerPos;
uint32_t enemyColor, playerColor;
bool enemyActive = false, playerActive = false;
unsigned long lastUpdate = 0;

void setup() {
  strip.begin();
  strip.setBrightness(globalBrightness);
  strip.show();
  pinMode(BEEP_PIN, OUTPUT);
  pinMode(BTN_RED, INPUT_PULLUP);
  pinMode(BTN_YELLOW, INPUT_PULLUP);
  pinMode(BTN_BLUE, INPUT_PULLUP);
  pinMode(BTN_MODE, INPUT_PULLUP);
  randomSeed(analogRead(0));
  printf("Pixel Pulse initialized.\n");
}

void loop() {
  checkModeSwitch();
  switch (currentMode) {
    case MODE_GAME: runGameMode(); break;
    case MODE_LAMP: runLampMode(); break;
    case MODE_SHOW: runShowMode(); break;
  }
}

// --- 模式切换模块 ---
void checkModeSwitch() {
  if (digitalRead(BTN_MODE) == LOW) {
    if (digitalRead(BTN_BLUE) == LOW) {
      if (currentMode == MODE_GAME && inGame) return;
      delay(300);
      currentMode = (AppMode)((currentMode + 1) % 3);
      modeFeedback();
      while(digitalRead(BTN_BLUE) == LOW) {
          delay(10); // wait with release
      }
    }
  }
}

void modeFeedback() {
  strip.clear();
  uint32_t c = (currentMode == MODE_GAME) ? strip.Color(200,0,0) :
               (currentMode == MODE_LAMP) ? strip.Color(150,150,150) : strip.Color(0,200,0);
  for(int i=0; i<5; i++) strip.setPixelColor(i, c);
  strip.show();
  digitalWrite(BEEP_PIN, HIGH); delay(100); digitalWrite(BEEP_PIN, LOW);
  delay(400);
}

// --- 游戏模式模块 ---
void runGameMode() {
  if (!inGame) {
    strip.clear();
    strip.setPixelColor(0, strip.Color(100,0,0));
    strip.setPixelColor(1, strip.Color(100,100,0));
    strip.setPixelColor(2, strip.Color(0,0,100));
    strip.show();
    // 当 D5(MODE) 被按下时，忽略难度选择，避免与模式切换冲突
    if (digitalRead(BTN_MODE) == LOW) return;
    if (digitalRead(BTN_RED) == LOW)    { speedInc = 0.05f; startGame(); }
    if (digitalRead(BTN_YELLOW) == LOW) { speedInc = 0.12f; startGame(); }
    if (digitalRead(BTN_BLUE) == LOW)   { speedInc = 0.22f; startGame(); }
  } else {
    if (!enemyActive) spawnEnemy();
    if (!playerActive) checkFire();
    if (millis() - lastUpdate > 20) {
      lastUpdate = millis();
      enemyPos -= currentSpeed;
      if (enemyPos < 3) { health--; enemyActive = false; beep(300); if (health <= 0) inGame = false; }
      if (playerActive) {
        playerPos += (currentSpeed * 2.5f);
        if (playerPos >= enemyPos) {
          if (playerColor == enemyColor) { score++; enemyActive = false; playerActive = false; beep(10); if(score%10==0) levelUp(); }
          else playerActive = false;
        }
        if (playerPos >= NUM_LEDS) playerActive = false;
      }
      renderGame();
    }
  }
}

void spawnEnemy() {
  uint32_t colors[] = {strip.Color(200,0,0), strip.Color(200,200,0), strip.Color(0,0,200), strip.Color(0,200,0)};
  enemyColor = colors[random(4)];
  enemyPos = NUM_LEDS - 1; enemyActive = true;
}

void checkFire() {
  if (digitalRead(BTN_RED) == LOW) { playerColor = strip.Color(200,0,0); playerPos=3; playerActive=true; }
  else if (digitalRead(BTN_YELLOW) == LOW) { playerColor = strip.Color(200,200,0); playerPos=3; playerActive=true; }
  else if (digitalRead(BTN_BLUE) == LOW) { playerColor = strip.Color(0,0,200); playerPos=3; playerActive=true; }
  else if (digitalRead(BTN_MODE) == LOW) { playerColor = strip.Color(0,200,0); playerPos=3; playerActive=true; } // D5 = 绿色
}

void levelUp() {
  currentSpeed += speedInc;
  for(int i=0; i<3; i++) { strip.fill(strip.Color(200,150,0)); strip.show(); beep(50); strip.clear(); strip.show(); delay(50); }
}

void renderGame() {
  strip.clear();
  for(int i=0; i<health; i++) strip.setPixelColor(i, strip.Color(50,50,50));
  if(enemyActive) strip.setPixelColor((int)enemyPos, enemyColor);
  if(playerActive) strip.setPixelColor((int)playerPos, playerColor);
  strip.show();
}

void startGame() { inGame = true; health = 3; score = 0; currentSpeed = 0.4f; enemyActive = playerActive = false; beep(100); }

// --- 台灯模式模块 ---
void runLampMode() {
  if (digitalRead(BTN_RED) == LOW) adjustBrightness(1);
  if (digitalRead(BTN_YELLOW) == LOW) adjustBrightness(-1);
  if (digitalRead(BTN_BLUE) == LOW) { lampColorIndex = (lampColorIndex + 1) % 5; beep(50); delay(200); }
  uint32_t colors[] = {strip.Color(255,255,255), strip.Color(255,160,50), strip.Color(100,255,100), strip.Color(80,80,255), strip.Color(200,0,200)};
  strip.fill(colors[lampColorIndex]);
  strip.show();
}

// --- 灯光秀模块 ---
void runShowMode() {
  if (digitalRead(BTN_RED) == LOW) adjustBrightness(1);
  if (digitalRead(BTN_YELLOW) == LOW) adjustBrightness(-1);
  if (digitalRead(BTN_BLUE) == LOW) { showPatternIndex = (showPatternIndex + 1) % 10; beep(50); delay(300); }

  switch (showPatternIndex) {
    case 0: showRainbow(); break;
    case 1: showFire(); break;
    case 2: showMeteor(); break;
    case 3: showSparkle(); break;
    case 4: showOcean(); break;
    case 5: showCollide(); break;
    case 6: showBreath(); break;
    case 7: showScanner(); break;
    case 8: showPlasma(); break;
    case 9: showFirework(); break;
  }
}

// --- 通用工具 ---
void adjustBrightness(int delta) {
  int newLevel = constrain(brightnessLevel + delta, 0, 9);
  if (newLevel == brightnessLevel) { beep(20); delay(50); return; } // 已到极值
  brightnessLevel = newLevel;
  globalBrightness = 10 + brightnessLevel * ((MAX_BRIGHT - 10) / 9);
  if (brightnessLevel == 9) globalBrightness = MAX_BRIGHT; // 确保最高档精确
  strip.setBrightness(globalBrightness);
  beep(10); delay(120);
}

void beep(int ms) { digitalWrite(BEEP_PIN, HIGH); delay(ms); digitalWrite(BEEP_PIN, LOW); }

uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  if(WheelPos < 170) { WheelPos -= 85; return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3); }
  WheelPos -= 170; return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

// 像素衰减辅助：将单个像素的 RGB 各乘以 factor/256
void dimPixel(int i, uint8_t factor) {
  uint32_t c = strip.getPixelColor(i);
  uint8_t r = (uint8_t)(c >> 16);
  uint8_t g = (uint8_t)(c >> 8);
  uint8_t b = (uint8_t)c;
  r = (uint16_t)((uint16_t)r * factor) >> 8;
  g = (uint16_t)((uint16_t)g * factor) >> 8;
  b = (uint16_t)((uint16_t)b * factor) >> 8;
  strip.setPixelColor(i, strip.Color(r, g, b));
}

//  动画 0: 彩虹流光
void showRainbow() {
  static uint16_t offset = 0;
  for (int i = 0; i < NUM_LEDS; i++) {
    uint8_t hue = (i * 256 / NUM_LEDS + offset) & 255;
    strip.setPixelColor(i, Wheel(hue));
  }
  strip.show();
  offset++;
  delay(12);
}

//  动画 1: 动态火焰
void showFire() {
  for (int i = 0; i < NUM_LEDS; i++) {
    if (random(3) == 0) dimPixel(i, 180 + random(50));
  }
  for (int i = NUM_LEDS - 1; i >= 2; i--) {
    uint32_t c1 = strip.getPixelColor(i - 1);
    uint32_t c2 = strip.getPixelColor(i - 2);
    uint8_t r = ((uint8_t)(c1 >> 16) + (uint8_t)(c2 >> 16)) / 2;
    uint8_t g = ((uint8_t)(c1 >> 8) + (uint8_t)(c2 >> 8)) / 2;
    uint8_t b = ((uint8_t)c1 + (uint8_t)c2) / 2;
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  for (int i = 0; i < 8; i++) {
    if (random(2) == 0) {
      uint8_t heat = 200 + random(55);
      strip.setPixelColor(i, strip.Color(heat, heat / 3, 0));
    }
  }
  strip.show();
  delay(18);
}

//  动画 2: 流星雨
void showMeteor() {
  static int head = NUM_LEDS - 1;
  for (int i = 0; i < NUM_LEDS; i++) {
    if (random(3) != 0) dimPixel(i, 160);
  }
  for (int j = 0; j < 10; j++) {
    int pos = head + j;
    if (pos >= 0 && pos < NUM_LEDS) {
      uint8_t bright = 255 - j * 25;
      strip.setPixelColor(pos, strip.Color(bright, bright, bright));
    }
  }
  strip.show();
  head--;
  if (head < -10) head = NUM_LEDS - 1;
  delay(18);
}

//  动画 3: 随机火花
void showSparkle() {
  for (int i = 0; i < NUM_LEDS; i++) {
    dimPixel(i, 200);
  }
  for (int k = 0; k < 3; k++) {
    int pos = random(NUM_LEDS);
    uint8_t style = random(3);
    if (style == 0) strip.setPixelColor(pos, strip.Color(255, 255, 255));
    else if (style == 1) strip.setPixelColor(pos, strip.Color(255, 200, 80));
    else strip.setPixelColor(pos, strip.Color(180, 220, 255));
  }
  strip.show();
  delay(25);
}

//  动画 4: 海洋波动
void showOcean() {
  static uint16_t t = 0;
  for (int i = 0; i < NUM_LEDS; i++) {
    float v1 = sinf(i * 0.15f + t * 0.04f);
    float v2 = sinf(i * 0.08f - t * 0.06f);
    float v3 = sinf(i * 0.22f + t * 0.03f);
    uint8_t blue  = constrain((int)(128 + 70 * v1 + 40 * v2 + 20 * v3), 20, 255);
    uint8_t green = constrain((int)(50 + 50 * v2 + 30 * v3), 10, 150);
    uint8_t red   = constrain((int)(10 + 15 * v3), 0, 40);
    strip.setPixelColor(i, strip.Color(red, green, blue));
  }
  strip.show();
  t++;
  delay(15);
}

//  动画 5: 冷热能量撞击
void showCollide() {
  static int posR = 0;
  static int posB = NUM_LEDS-1;
  static uint8_t stage = 0; // 0=approach, 1=explode, 2=fade
  static uint8_t timer = 0;

  strip.clear();

  if (stage == 0) {
    for (int j = 0; j < 8; j++) {
      int p = posR - j;
      if (p >= 0) {
        uint8_t br = 255 - j * 30;
        strip.setPixelColor(p, strip.Color(br, br/8, 0));
      }
    }
    for (int j = 0; j < 8; j++) {
      int p = posB + j;
      if (p < NUM_LEDS) {
        uint8_t br = 255 - j * 30;
        strip.setPixelColor(p, strip.Color(0, br/8, br));
      }
    }
    posR++; posB--;
    if (posR >= posB) { stage = 1; timer = 0; }
  }
  else if (stage == 1) {
    int center = NUM_LEDS / 2;
    int radius = timer * 3;
    for (int i = 0; i < NUM_LEDS; i++) {
      int dist = abs(i - center);
      if (dist <= radius) {
        uint8_t br = constrain(255 - dist * 6, 0, 255);
        if (dist < radius / 2)
          strip.setPixelColor(i, strip.Color(br, br, br));
        else
          strip.setPixelColor(i, strip.Color(br, 0, br));
      }
    }
    timer++;
    if (radius > NUM_LEDS / 2 + 10) { stage = 2; timer = 0; }
  }
  else {
    int center = NUM_LEDS / 2;
    int br_val = 200 - timer * 15;
    uint8_t br = constrain(br_val, 0, 200);
    for (int i = 0; i < NUM_LEDS; i++) {
      int dist = abs(i - center);
      int val = br - dist;
      uint8_t v = constrain(val, 0, 255);
      strip.setPixelColor(i, strip.Color(v/2, 0, v/2));
    }
    timer++;
    if (br == 0) { stage = 0; posR = 0; posB = NUM_LEDS - 1; timer = 0; }
  }
  strip.show();
  delay(15);
}

//  动画 6: 呼吸脉冲
void showBreath() {
  static uint16_t phase = 0;
  float breath = (sinf(phase * 0.03f) + 1.0f) * 0.5f;
  int center = NUM_LEDS / 2;
  for (int i = 0; i < NUM_LEDS; i++) {
    int dist = abs(i - center);
    float falloff = 1.0f / (1.0f + (float)(dist * dist) / 400.0f);
    uint8_t val = (uint8_t)(breath * falloff * 255.0f);
    strip.setPixelColor(i, strip.Color(val / 3, val / 6, val));
  }
  strip.show();
  phase++;
  delay(15);
}

//  动画 7: 双向扫描
void showScanner() {
  static int pos = 0;
  static int8_t dir = 1;
  strip.clear();
  for (int i = 0; i < NUM_LEDS; i++) {
    int dist = abs(i - pos);
    if (dist < 16) {
      uint8_t br = 255 / (1 + dist * dist / 4);
      strip.setPixelColor(i, strip.Color(0, br, br));
    }
  }
  strip.show();
  pos += dir;
  if (pos >= NUM_LEDS - 1) { pos = NUM_LEDS - 1; dir = -1; }
  if (pos <= 0) { pos = 0; dir = 1; }
  delay(10);
}

//  动画 8: 等离子体
void showPlasma() {
  static uint16_t t = 0;
  for (int i = 0; i < NUM_LEDS; i++) {
    float v = sinf(i * 0.1f + t * 0.03f) * 128.0f
            + sinf(i * 0.05f - t * 0.05f) * 128.0f
            + sinf((i + t) * 0.02f) * 128.0f;
    uint8_t hue = (uint8_t)((int)v & 255);
    strip.setPixelColor(i, Wheel(hue));
  }
  strip.show();
  t++;
  delay(15);
}

//  动画 9: 烟花绽放
void showFirework() {
  static uint8_t fwStage = 0; // 0=launch, 1=burst, 2=fall
  static int fwPos = 0;
  static int fwPeak = 100;
  static uint8_t fwTimer = 0;
  static uint8_t fwHue = 0;

  if (fwStage == 0) {
    for (int i = 0; i < NUM_LEDS; i++) dimPixel(i, 150);
    for (int j = 0; j < 5; j++) {
      int p = fwPos - j;
      if (p >= 0 && p < NUM_LEDS) {
        uint8_t br = 255 - j * 50;
        strip.setPixelColor(p, strip.Color(br, br, br));
      }
    }
    strip.show();
    fwPos += 3;
    if (fwPos >= fwPeak) { fwStage = 1; fwTimer = 0; }
    delay(15);
  }
  else if (fwStage == 1) {
    for (int i = 0; i < NUM_LEDS; i++) dimPixel(i, 180);
    int radius = fwTimer * 2;
    for (int k = 0; k < 8; k++) {
      int offset = (int)((float)radius * sinf(k * 0.785f));
      int p = fwPeak + offset;
      if (p >= 0 && p < NUM_LEDS) {
        strip.setPixelColor(p, Wheel(fwHue + k * 32));
      }
    }
    strip.show();
    fwTimer++;
    if (fwTimer > 30) { fwStage = 2; fwTimer = 0; }
    delay(18);
  }
  else {
    for (int i = 0; i < NUM_LEDS; i++) dimPixel(i, 170);
    strip.show();
    fwTimer++;
    if (fwTimer > 25) {
      fwStage = 0;
      fwPos = 0;
      fwPeak = 70 + random(60);
      fwHue = random(256);
      fwTimer = 0;
    }
    delay(20);
  }
}

// ==========================================
// Main Entry Point
// ==========================================
extern "C" void app_main(void) {
    // Call Arduino Setup
    setup();
    
    // Main loop emulation
    while(1) {
        loop();
        // Small yielding to FreeRTOS and system watchdog feeding
        vTaskDelay(1); 
    }
}