#include <Adafruit_NeoPixel.h>

// --- 引脚定义 ---
#define LED_PIN     7
#define NUM_LEDS    144
#define BEEP_PIN    6
#define BTN_RED     2
#define BTN_YELLOW  3
#define BTN_BLUE    4
#define BTN_MODE    5
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
float currentSpeed = 0.4;
float speedInc = 0.1;

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
      while(digitalRead(BTN_BLUE) == LOW);
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
    if (digitalRead(BTN_RED) == LOW)    { speedInc = 0.05; startGame(); }
    if (digitalRead(BTN_YELLOW) == LOW) { speedInc = 0.12; startGame(); }
    if (digitalRead(BTN_BLUE) == LOW)   { speedInc = 0.22; startGame(); }
  } else {
    if (!enemyActive) spawnEnemy();
    if (!playerActive) checkFire();
    if (millis() - lastUpdate > 20) {
      lastUpdate = millis();
      enemyPos -= currentSpeed;
      if (enemyPos < 3) { health--; enemyActive = false; beep(300); if (health <= 0) inGame = false; }
      if (playerActive) {
        playerPos += (currentSpeed * 2.5);
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

void startGame() { inGame = true; health = 3; score = 0; currentSpeed = 0.4; enemyActive = playerActive = false; beep(100); }

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
  // 10档: 10, 25, 41, 56, 72, 87, 103, 118, 134, 150
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
  r = (r * factor) >> 8;
  g = (g * factor) >> 8;
  b = (b * factor) >> 8;
  strip.setPixelColor(i, strip.Color(r, g, b));
}

// ============================================================
//  动画 0: 彩虹流光
// ============================================================
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

// ============================================================
//  动画 1: 动态火焰 — 无额外数组，直接读写像素缓冲区
// ============================================================
void showFire() {
  // 冷却：随机选像素降温
  for (int i = 0; i < NUM_LEDS; i++) {
    if (random(3) == 0) dimPixel(i, 180 + random(50)); // 衰减 70-90%
  }
  // 热扩散：从顶端向底端传递（避免额外数组）
  for (int i = NUM_LEDS - 1; i >= 2; i--) {
    uint32_t c1 = strip.getPixelColor(i - 1);
    uint32_t c2 = strip.getPixelColor(i - 2);
    uint8_t r = ((uint8_t)(c1 >> 16) + (uint8_t)(c2 >> 16)) / 2;
    uint8_t g = ((uint8_t)(c1 >> 8) + (uint8_t)(c2 >> 8)) / 2;
    uint8_t b = ((uint8_t)c1 + (uint8_t)c2) / 2;
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  // 底部热源注入
  for (int i = 0; i < 8; i++) {
    if (random(2) == 0) {
      uint8_t heat = 200 + random(55);
      // 火焰颜色：红为主，绿次之，蓝几乎无
      strip.setPixelColor(i, strip.Color(heat, heat / 3, 0));
    }
  }
  strip.show();
  delay(18);
}

// ============================================================
//  动画 2: 流星雨
// ============================================================
void showMeteor() {
  static int head = NUM_LEDS - 1;
  // 全带随机衰减实现拖尾效果
  for (int i = 0; i < NUM_LEDS; i++) {
    if (random(3) != 0) dimPixel(i, 160); // ~62% 保留
  }
  // 绘制流星头部和短尾
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

// ============================================================
//  动画 3: 随机火花
// ============================================================
void showSparkle() {
  // 全带衰减
  for (int i = 0; i < NUM_LEDS; i++) {
    dimPixel(i, 200); // ~78% 保留
  }
  // 随机点亮 2-3 个火花
  for (int k = 0; k < 3; k++) {
    int pos = random(NUM_LEDS);
    // 随机暖白/冷白/金色
    uint8_t style = random(3);
    if (style == 0) strip.setPixelColor(pos, strip.Color(255, 255, 255));
    else if (style == 1) strip.setPixelColor(pos, strip.Color(255, 200, 80));
    else strip.setPixelColor(pos, strip.Color(180, 220, 255));
  }
  strip.show();
  delay(25);
}

// ============================================================
//  动画 4: 海洋波动
// ============================================================
void showOcean() {
  static uint16_t t = 0;
  for (int i = 0; i < NUM_LEDS; i++) {
    // 多频正弦叠加模拟波浪
    float v1 = sin(i * 0.15 + t * 0.04);
    float v2 = sin(i * 0.08 - t * 0.06);
    float v3 = sin(i * 0.22 + t * 0.03);
    uint8_t blue  = constrain((int)(128 + 70 * v1 + 40 * v2 + 20 * v3), 20, 255);
    uint8_t green = constrain((int)(50 + 50 * v2 + 30 * v3), 10, 150);
    uint8_t red   = constrain((int)(10 + 15 * v3), 0, 40);
    strip.setPixelColor(i, strip.Color(red, green, blue));
  }
  strip.show();
  t++;
  delay(15);
}

// ============================================================
//  动画 5: 冷热能量撞击
// ============================================================
void showCollide() {
  static int posR = 0;           // 红色粒子位置（从左）
  static int posB = NUM_LEDS-1;  // 蓝色粒子位置（从右）
  static uint8_t stage = 0;      // 0=approach, 1=explode, 2=fade
  static uint8_t timer = 0;

  strip.clear();

  if (stage == 0) {
    // 红粒子从左向右
    for (int j = 0; j < 8; j++) {
      int p = posR - j;
      if (p >= 0) {
        uint8_t br = 255 - j * 30;
        strip.setPixelColor(p, strip.Color(br, br/8, 0));
      }
    }
    // 蓝粒子从右向左
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
    // 爆炸：从中心向两端扩散紫+白色
    int center = NUM_LEDS / 2;
    int radius = timer * 3;
    for (int i = 0; i < NUM_LEDS; i++) {
      int dist = abs(i - center);
      if (dist <= radius) {
        uint8_t br = constrain(255 - dist * 6, 0, 255);
        // 中心偏白，外围偏紫
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
    // 淡出：全带逐渐变暗
    // 先恢复上一帧再衰减
    int center = NUM_LEDS / 2;
    uint8_t br = constrain(200 - timer * 15, 0, 200);
    for (int i = 0; i < NUM_LEDS; i++) {
      int dist = abs(i - center);
      uint8_t v = constrain(br - dist, 0, 255);
      strip.setPixelColor(i, strip.Color(v/2, 0, v/2));
    }
    timer++;
    if (br == 0) { stage = 0; posR = 0; posB = NUM_LEDS - 1; timer = 0; }
  }
  strip.show();
  delay(15);
}

// ============================================================
//  动画 6: 呼吸脉冲（中心扩散）
// ============================================================
void showBreath() {
  static uint16_t phase = 0;
  float breath = (sin(phase * 0.03) + 1.0) * 0.5; // 0.0 ~ 1.0
  int center = NUM_LEDS / 2;
  for (int i = 0; i < NUM_LEDS; i++) {
    int dist = abs(i - center);
    // 高斯形扩散衰减
    float falloff = 1.0 / (1.0 + (float)(dist * dist) / 400.0);
    uint8_t val = (uint8_t)(breath * falloff * 255);
    // 蓝紫色调
    strip.setPixelColor(i, strip.Color(val / 3, val / 6, val));
  }
  strip.show();
  phase++;
  delay(15);
}

// ============================================================
//  动画 7: 双向扫描 (Knight Rider)
// ============================================================
void showScanner() {
  static int pos = 0;
  static int8_t dir = 1;
  strip.clear();
  for (int i = 0; i < NUM_LEDS; i++) {
    int dist = abs(i - pos);
    if (dist < 16) {
      // 类高斯衰减
      uint8_t br = 255 / (1 + dist * dist / 4);
      strip.setPixelColor(i, strip.Color(0, br, br)); // 青色
    }
  }
  strip.show();
  pos += dir;
  if (pos >= NUM_LEDS - 1) { pos = NUM_LEDS - 1; dir = -1; }
  if (pos <= 0) { pos = 0; dir = 1; }
  delay(10);
}

// ============================================================
//  动画 8: 等离子体
// ============================================================
void showPlasma() {
  static uint16_t t = 0;
  for (int i = 0; i < NUM_LEDS; i++) {
    float v = sin(i * 0.1 + t * 0.03) * 128
            + sin(i * 0.05 - t * 0.05) * 128
            + sin((i + t) * 0.02) * 128;
    uint8_t hue = (uint8_t)((int)v & 255);
    strip.setPixelColor(i, Wheel(hue));
  }
  strip.show();
  t++;
  delay(15);
}

// ============================================================
//  动画 9: 烟花绽放
// ============================================================
void showFirework() {
  static uint8_t fwStage = 0;  // 0=launch, 1=burst, 2=fall
  static int fwPos = 0;        // 上升位置
  static int fwPeak = 100;     // 爆炸高度
  static uint8_t fwTimer = 0;
  static uint8_t fwHue = 0;    // 烟花颜色

  if (fwStage == 0) {
    // 全带衰减（残留消散）
    for (int i = 0; i < NUM_LEDS; i++) dimPixel(i, 150);
    // 上升弹射 — 白色亮点 + 短尾
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
    // 爆炸：从 peak 向两侧扩散彩色粒子
    for (int i = 0; i < NUM_LEDS; i++) dimPixel(i, 180);
    int radius = fwTimer * 2;
    for (int k = 0; k < 8; k++) {
      // 8 个碎片均匀分布
      int offset = (int)((float)radius * sin(k * 0.785));
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
    // 坠落消散
    for (int i = 0; i < NUM_LEDS; i++) dimPixel(i, 170);
    strip.show();
    fwTimer++;
    if (fwTimer > 25) {
      // 重新发射
      fwStage = 0;
      fwPos = 0;
      fwPeak = 70 + random(60); // 随机爆炸高度 70-130
      fwHue = random(256);
      fwTimer = 0;
    }
    delay(20);
  }
}
