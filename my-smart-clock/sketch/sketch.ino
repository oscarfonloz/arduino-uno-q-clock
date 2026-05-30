/*
 * sketch.ino  —  STM32 / Real-time side
 * Arduino UNO Q — Clock + USD/MXN exchange rate + Modulino day-of-week pixels
 *
 * Required libraries: Arduino_RouterBridge, Modulino
 */

#include <Arduino_RouterBridge.h>
#include <Modulino.h>

// ─── Low-level matrix API ─────────────────────────────────────────────────────
extern "C" void matrixBegin();
extern "C" void matrixWrite(const uint32_t* buf);

constexpr int      ROWS     = 8;
constexpr int      COLS     = 13;
constexpr uint16_t ROW_MASK = (1u << COLS) - 1;

static uint16_t rowsBits[ROWS] = {0};
static uint32_t matBuf[4]      = {0, 0, 0, 0};

// ─── Font ─────────────────────────────────────────────────────────────────────
struct Glyph { uint8_t w; uint8_t col[6]; };

static const Glyph FONT_DIGITS[] = {
  /* 0 */ {5, {0x3E,0x51,0x49,0x45,0x3E}},
  /* 1 */ {3, {0x42,0x7F,0x40,0x00,0x00}},
  /* 2 */ {5, {0x62,0x51,0x49,0x49,0x46}},
  /* 3 */ {5, {0x22,0x49,0x49,0x49,0x36}},
  /* 4 */ {5, {0x18,0x14,0x12,0x7F,0x10}},
  /* 5 */ {5, {0x2F,0x49,0x49,0x49,0x31}},
  /* 6 */ {5, {0x3E,0x49,0x49,0x49,0x32}},
  /* 7 */ {5, {0x01,0x71,0x09,0x05,0x03}},
  /* 8 */ {5, {0x36,0x49,0x49,0x49,0x36}},
  /* 9 */ {5, {0x26,0x49,0x49,0x49,0x3E}},
};

static const Glyph FONT_LETTERS[] = {
  /* A */ {5, {0x7E,0x09,0x09,0x09,0x7E}},
  /* B */ {5, {0x7F,0x49,0x49,0x49,0x36}},
  /* C */ {5, {0x3E,0x41,0x41,0x41,0x22}},
  /* D */ {5, {0x7F,0x41,0x41,0x41,0x3E}},
  /* E */ {5, {0x7F,0x49,0x49,0x49,0x41}},
  /* F */ {5, {0x7F,0x09,0x09,0x09,0x01}},
  /* G */ {5, {0x3E,0x41,0x41,0x49,0x3A}},
  /* H */ {5, {0x7F,0x08,0x08,0x08,0x7F}},
  /* I */ {3, {0x41,0x7F,0x41,0x00,0x00}},
  /* J */ {5, {0x20,0x40,0x41,0x3F,0x01}},
  /* K */ {5, {0x7F,0x08,0x14,0x22,0x41}},
  /* L */ {5, {0x7F,0x40,0x40,0x40,0x40}},
  /* M */ {5, {0x7F,0x02,0x04,0x02,0x7F}},
  /* N */ {5, {0x7F,0x04,0x08,0x10,0x7F}},
  /* O */ {5, {0x3E,0x41,0x41,0x41,0x3E}},
  /* P */ {5, {0x7F,0x09,0x09,0x09,0x06}},
  /* Q */ {5, {0x3E,0x41,0x51,0x21,0x5E}},
  /* R */ {5, {0x7F,0x09,0x19,0x29,0x46}},
  /* S */ {5, {0x26,0x49,0x49,0x49,0x32}},
  /* T */ {5, {0x01,0x01,0x7F,0x01,0x01}},
  /* U */ {5, {0x3F,0x40,0x40,0x40,0x3F}},
  /* V */ {5, {0x1F,0x20,0x40,0x20,0x1F}},
  /* W */ {5, {0x3F,0x40,0x38,0x40,0x3F}},
  /* X */ {5, {0x63,0x14,0x08,0x14,0x63}},
  /* Y */ {5, {0x07,0x08,0x70,0x08,0x07}},
  /* Z */ {5, {0x61,0x51,0x49,0x45,0x43}},
};

static const Glyph GLYPH_COLON  = {1, {0x14,0x00,0x00,0x00,0x00}};
static const Glyph GLYPH_SPACE  = {3, {0x00,0x00,0x00,0x00,0x00}};
static const Glyph GLYPH_DOLLAR = {5, {0x24,0x2A,0x7F,0x2A,0x12}};
static const Glyph GLYPH_PIPE   = {1, {0x7F,0x00,0x00,0x00,0x00}};
static const Glyph GLYPH_EQUALS = {5, {0x14,0x14,0x14,0x14,0x14}};
static const Glyph GLYPH_DOT    = {2, {0x60,0x60,0x00,0x00,0x00}};

static const Glyph& glyphFor(char c) {
  if (c >= '0' && c <= '9') return FONT_DIGITS[c - '0'];
  if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
  if (c >= 'A' && c <= 'Z') return FONT_LETTERS[c - 'A'];
  if (c == ':') return GLYPH_COLON;
  if (c == '$') return GLYPH_DOLLAR;
  if (c == '|') return GLYPH_PIPE;
  if (c == '=') return GLYPH_EQUALS;
  if (c == '.') return GLYPH_DOT;
  return GLYPH_SPACE;
}

// ─── Rendering ────────────────────────────────────────────────────────────────
static void updateBuf() {
  matBuf[0] = matBuf[1] = matBuf[2] = matBuf[3] = 0;
  int idx = 0;
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++) {
      if ((rowsBits[r] >> (COLS - 1 - c)) & 1)
        matBuf[idx / 32] |= (1UL << (idx % 32));
      idx++;
    }
}

static void pushColumn(uint8_t colBits) {
  for (int r = 0; r < ROWS; r++) {
    uint16_t bit = (colBits >> r) & 1;
    rowsBits[r] = ((rowsBits[r] << 1) | bit) & ROW_MASK;
  }
}

static void pushAndShow(uint8_t colBits, int speed_ms) {
  pushColumn(colBits);
  updateBuf();
  matrixWrite(matBuf);
  delay(speed_ms);
}

static void scrollOnce(const char* msg, int speed_ms) {
  for (int i = 0; i < COLS; i++) pushAndShow(0x00, speed_ms);
  for (const char* p = msg; *p; p++) {
    const Glyph& g = glyphFor(*p);
    for (int i = 0; i < g.w; i++) pushAndShow(g.col[i], speed_ms);
    pushAndShow(0x00, speed_ms);
  }
  for (int i = 0; i < COLS; i++) pushAndShow(0x00, speed_ms);
}

// ─── Modulino Pixels ──────────────────────────────────────────────────────────
ModulinoPixels leds;

// Brightness levels — reduced from old code to be easier on the eyes
const int DIM_BR    = 3;   // inactive days  (0-100)
const int ACTIVE_BR = 25;  // today          (was 80 before — much gentler now)

// Colors
ModulinoColor ACTIVE_COLOR(0, 180, 50);   // green
ModulinoColor DIM_COLOR(15, 15, 15);      // dim white
ModulinoColor OFF_COLOR(0, 0, 0);

// dayOfWeek: 0=Mon ... 6=Sun. Pass -1 on boot to show all dim.
void updatePixels(int dayOfWeek) {
  leds.clear();
  for (int i = 0; i < 7; i++) {
    if (i == dayOfWeek) {
      leds.set(i, ACTIVE_COLOR, ACTIVE_BR);
    } else {
      leds.set(i, DIM_COLOR, DIM_BR);
    }
  }
  leds.set(7, OFF_COLOR, 0);  // 8th LED always off
  leds.show();
}

// ─── Shared state ─────────────────────────────────────────────────────────────
#define MAX_TEXT 128
static char currentDisplay[MAX_TEXT] = "Loading...";
static char pendingDisplay[MAX_TEXT] = "";
static int  pendingDay               = -1;
static volatile bool newDisplayReady = false;

// ─── Bridge RPC callback ──────────────────────────────────────────────────────
// Python sends one string: "10:28 PM   |   $1 = 17.34 MXN   ;4"
// We split on the last ';' — left = display text, right = day of week (0-6)
String setDisplay(String payload) {
  int sep = payload.lastIndexOf(';');
  if (sep > 0 && sep < (int)payload.length() - 1) {
    String text   = payload.substring(0, sep);
    int dayOfWeek = payload.substring(sep + 1).toInt();
    if (text.length() > 0 && text.length() < MAX_TEXT - 1) {
      text.toCharArray(pendingDisplay, sizeof(pendingDisplay));
      pendingDay      = dayOfWeek;
      newDisplayReady = true;
    }
  }
  return "OK";
}

// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
  Monitor.begin();

  matrixBegin();
  for (int r = 0; r < ROWS; r++) rowsBits[r] = 0;

  Modulino.begin();
  leds.begin();
  updatePixels(-1);   // all dim on boot — signals device is alive

  Bridge.begin();
  Bridge.provide("setDisplay", setDisplay);

  boolean ready = false;
  while (!ready) Bridge.call("linux_started").result(ready);
  Monitor.println("Python ready — scrolling!");
}

// ─── Main loop ────────────────────────────────────────────────────────────────
void loop() {
  if (newDisplayReady) {
    strncpy(currentDisplay, pendingDisplay, MAX_TEXT - 1);
    currentDisplay[MAX_TEXT - 1] = '\0';
    updatePixels(pendingDay);
    newDisplayReady = false;
    Monitor.print("Displaying: "); Monitor.println(currentDisplay);
  }
  scrollOnce(currentDisplay, 80);
}
