/*
 * ST7789 SPI driver for 170×320 display.
 * Copyright (c) 2026 dividuraga (durgadivija@gmail.com)
 * MIT License — see LICENSE.txt
 *
 * Class name kept as "ili9328SPI" so the rest of the codebase compiles unchanged.
 *
 * Wiring (set GPIOs via #define in the .ino):
 *   MOSI  → GPIO 23  (hardware SPI default)
 *   SCK   → GPIO 18  (hardware SPI default)
 *   CS    → #define CS
 *   DC    → #define DC
 *   RESET → #define RESET
 *   MISO  → not needed (display is write-only)
 */

#ifndef _ILI9328_SPI_H
#define _ILI9328_SPI_H

#if ARDUINO >= 100
 #include "Arduino.h"
#else
 #include "WProgram.h"
#endif

#include <SPI.h>

#define TFT_WIDTH  170
#define TFT_HEIGHT 320

// Column offset: the ST7789 controller is 240 px wide; a 170 px panel is
// typically wired to columns 35-204.  Adjust if your module differs.
#define ST7789_COL_OFFSET  35
#define ST7789_ROW_OFFSET   0

class ili9328SPI {
public:
  ili9328SPI(uint8_t cs, uint8_t dc, uint8_t rst);

  void     begin();
  void     fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void     drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
  void     drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
  void     drawPixel(int16_t x, int16_t y, uint16_t color);
  uint16_t color565(uint8_t r, uint8_t g, uint8_t b);
  void     fillScreen(uint16_t color);

  // Used by drawIndexedmap — draws a horizontal run of w+1 pixels.
  void     drawFastHLine1(int16_t x, int16_t y, int16_t w, uint16_t color);

private:
  void     init();
  void     setWindow(uint16_t x0, uint16_t x1, uint16_t y0, uint16_t y1);
  void     cmd(uint8_t c);
  void     dat8(uint8_t d);
  void     dat16(uint16_t d);

  uint8_t  CSpin, DCpin, RSTpin;

  uint32_t          CSPinMask, DCPinMask;
  volatile uint32_t *CSPort,  *DCPort;
};

#endif // _ILI9328_SPI_H
