/******************************************************************************/
/*                                                                            */
/*  PACMAN GAME FOR ESP32 — ST7789 170×320                                    */
/*                                                                            */
/******************************************************************************/
/*  Copyright (c) 2014  Dr. NCX (mirracle.mxx@gmail.com)                      */
/*                                                                            */
/* THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL              */
/* WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED              */
/* WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR    */
/* BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES      */
/* OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,     */
/* WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,     */
/* ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS        */
/* SOFTWARE.                                                                  */
/*                                                                            */
/*  MIT license, all text above must be included in any redistribution.       */
/*                                                                            */
/*  Portions Copyright (c) 2026 dividuraga — ST7789 ESP32 port                */

#include "ili9328.h"

#define C16(_rr,_gg,_bb) ((ushort)(((_rr & 0xF8) << 8) | ((_gg & 0xFC) << 3) | ((_bb & 0xF8) >> 3)))

uint16_t _paletteW[] =
{
  C16(0, 0, 0),
  C16(255, 0, 0),      // 1 red
  C16(222, 151, 81),   // 2 brown
  C16(255, 0, 255),    // 3 pink

  C16(0, 0, 0),
  C16(0, 255, 255),    // 5 cyan
  C16(71, 84, 255),    // 6 mid blue
  C16(255, 184, 81),   // 7 lt brown

  C16(0, 0, 0),
  C16(255, 255, 0),    // 9 yellow
  C16(0, 0, 0),
  C16(33, 33, 255),    // 11 blue

  C16(0, 255, 0),      // 12 green
  C16(71, 84, 174),    // 13 aqua
  C16(255, 184, 174),  // 14 lt pink
  C16(222, 222, 255),  // 15 whiteish
};

// Nearest-neighbour scale: 8×8 tile index buffer → 6×6 pixels on screen.
// Source columns used: 0,1,2,4,5,6  (skips 3 and 7).
void drawIndexedmap(ili9328SPI* tft, uint8_t* indexmap, int16_t x, uint16_t y) {
  for (byte oy = 0; oy < 6; oy++) {
    byte sy = (oy * 8) / 6;
    word color = (word)_paletteW[indexmap[sy * 8]];
    byte runStart = 0;

    for (byte ox = 1; ox <= 6; ox++) {
      word next = (ox < 6) ? (word)_paletteW[indexmap[sy * 8 + (ox * 8) / 6]] : ~color;
      if (next != color) {
        tft->drawFastHLine1(x + runStart, y + oy, ox - runStart, color);
        color = next;
        runStart = ox;
      }
    }
  }
}
