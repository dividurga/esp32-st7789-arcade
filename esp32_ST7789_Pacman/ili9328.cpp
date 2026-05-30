/*
 * ST7789 SPI driver — custom implementation replacing the original ILI9328.
 * Copyright (c) 2026 dividuraga (durgadivija@gmail.com)
 * MIT License — see LICENSE.txt
 *
 * Public interface (class name, method signatures) is unchanged so the
 * rest of the sketch compiles without modification.
 */

#include "ili9328.h"

// ST7789 accepts up to ~80 MHz; 40 MHz gives a comfortable margin on most
// ESP32 boards.  SPI_MODE3 is correct for ST7789.
static SPISettings st7789Settings(40000000, MSBFIRST, SPI_MODE3);

// ── ST7789 command bytes ───────────────────────────────────────────────────
#define ST7789_SWRESET  0x01
#define ST7789_SLPOUT   0x11
#define ST7789_NORON    0x13
#define ST7789_INVON    0x21
#define ST7789_DISPON   0x29
#define ST7789_CASET    0x2A
#define ST7789_RASET    0x2B
#define ST7789_RAMWR    0x2C
#define ST7789_MADCTL   0x36
#define ST7789_COLMOD   0x3A

// ── Fast pin helpers (port-register based) ────────────────────────────────
#define _CS_LO  (*CSPort &= ~CSPinMask)
#define _CS_HI  (*CSPort |=  CSPinMask)
#define _DC_LO  (*DCPort &= ~DCPinMask)
#define _DC_HI  (*DCPort |=  DCPinMask)

// ── Constructor ────────────────────────────────────────────────────────────
ili9328SPI::ili9328SPI(uint8_t cs, uint8_t dc, uint8_t rst)
  : CSpin(cs), DCpin(dc), RSTpin(rst) {}

// ── begin ──────────────────────────────────────────────────────────────────
void ili9328SPI::begin()
{
  CSPinMask = digitalPinToBitMask(CSpin);
  CSPort    = portOutputRegister(digitalPinToPort(CSpin));
  DCPinMask = digitalPinToBitMask(DCpin);
  DCPort    = portOutputRegister(digitalPinToPort(DCpin));

  pinMode(CSpin,  OUTPUT); _CS_HI;
  pinMode(DCpin,  OUTPUT); _DC_HI;
  pinMode(RSTpin, OUTPUT);

  SPI.begin();
  init();
}

// ── Low-level helpers (must be called inside an SPI transaction) ───────────

void ili9328SPI::cmd(uint8_t c)
{
  _DC_LO; _CS_LO;
  SPI.transfer(c);
  _CS_HI;
}

void ili9328SPI::dat8(uint8_t d)
{
  _DC_HI; _CS_LO;
  SPI.transfer(d);
  _CS_HI;
}

void ili9328SPI::dat16(uint16_t d)
{
  _DC_HI; _CS_LO;
  SPI.transfer(d >> 8);
  SPI.transfer(d & 0xFF);
  _CS_HI;
}

// ── Set pixel window and issue RAMWR ──────────────────────────────────────
void ili9328SPI::setWindow(uint16_t x0, uint16_t x1, uint16_t y0, uint16_t y1)
{
  x0 += ST7789_COL_OFFSET;  x1 += ST7789_COL_OFFSET;
  y0 += ST7789_ROW_OFFSET;  y1 += ST7789_ROW_OFFSET;

  cmd(ST7789_CASET);
  dat8(x0 >> 8); dat8(x0 & 0xFF);
  dat8(x1 >> 8); dat8(x1 & 0xFF);

  cmd(ST7789_RASET);
  dat8(y0 >> 8); dat8(y0 & 0xFF);
  dat8(y1 >> 8); dat8(y1 & 0xFF);

  cmd(ST7789_RAMWR);
}

// ── Display initialisation sequence ───────────────────────────────────────
void ili9328SPI::init()
{
  digitalWrite(RSTpin, HIGH); delay(10);
  digitalWrite(RSTpin, LOW);  delay(10);
  digitalWrite(RSTpin, HIGH); delay(120);

  SPI.beginTransaction(st7789Settings);

  cmd(ST7789_SWRESET); _CS_HI; delay(150);
  cmd(ST7789_SLPOUT);  _CS_HI; delay(500);

  cmd(ST7789_COLMOD); dat8(0x55);   // 16-bit RGB565
  cmd(ST7789_MADCTL); dat8(0x00);   // portrait, top→bottom, left→right

  // Inversion on — required by most 170×320 ST7789 modules for correct colours.
  // Remove this line if colours appear inverted on your panel.
  cmd(ST7789_INVON);

  cmd(ST7789_NORON);
  cmd(ST7789_DISPON); _CS_HI; delay(100);

  SPI.endTransaction();
}

// ── Public drawing API ─────────────────────────────────────────────────────

void ili9328SPI::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
  if (w <= 0 || h <= 0) return;
  SPI.beginTransaction(st7789Settings);
  setWindow(x, x + w - 1, y, y + h - 1);
  uint8_t hi = color >> 8, lo = color & 0xFF;
  _DC_HI; _CS_LO;
  for (int32_t n = (int32_t)w * h; n > 0; n--) {
    SPI.transfer(hi);
    SPI.transfer(lo);
  }
  _CS_HI;
  SPI.endTransaction();
}

void ili9328SPI::fillScreen(uint16_t color)
{
  fillRect(0, 0, TFT_WIDTH, TFT_HEIGHT, color);
}

void ili9328SPI::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
{
  if (w <= 0) return;
  SPI.beginTransaction(st7789Settings);
  setWindow(x, x + w - 1, y, y);
  uint8_t hi = color >> 8, lo = color & 0xFF;
  _DC_HI; _CS_LO;
  for (int16_t i = 0; i < w; i++) { SPI.transfer(hi); SPI.transfer(lo); }
  _CS_HI;
  SPI.endTransaction();
}

void ili9328SPI::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
{
  if (h <= 0) return;
  SPI.beginTransaction(st7789Settings);
  setWindow(x, x, y, y + h - 1);
  uint8_t hi = color >> 8, lo = color & 0xFF;
  _DC_HI; _CS_LO;
  for (int16_t i = 0; i < h; i++) { SPI.transfer(hi); SPI.transfer(lo); }
  _CS_HI;
  SPI.endTransaction();
}

void ili9328SPI::drawPixel(int16_t x, int16_t y, uint16_t color)
{
  SPI.beginTransaction(st7789Settings);
  setWindow(x, x, y, y);
  dat16(color);
  SPI.endTransaction();
}

uint16_t ili9328SPI::color565(uint8_t r, uint8_t g, uint8_t b)
{
  return ((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (b >> 3);
}

// Draws w+1 pixels — matches the original ILI9328 driver semantics used by drawIndexedmap.
void ili9328SPI::drawFastHLine1(int16_t x, int16_t y, int16_t w, uint16_t color)
{
  SPI.beginTransaction(st7789Settings);
  setWindow(x, x + w, y, y);
  uint8_t hi = color >> 8, lo = color & 0xFF;
  _DC_HI; _CS_LO;
  for (int16_t i = 0; i <= w; i++) { SPI.transfer(hi); SPI.transfer(lo); }
  _CS_HI;
  SPI.endTransaction();
}
