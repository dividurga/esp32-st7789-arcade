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
/*  Portions Copyright (c) 2026 dividurga                                     */
/*  Fork: ST7789 port, Flappy Bird, arcade homescreen, ESP32 button input     */

byte SPEED = 2; // 1=SLOW  2=NORMAL  4=FAST  (only these three values)

/******************************************************************************/
/*   MAIN GAME VARIABLES                                                      */
/******************************************************************************/

#define BONUS_INACTIVE_TIME 600
#define BONUS_ACTIVE_TIME 300

#define START_LIVES 2
#define START_LEVEL 1

byte MAXLIVES = 5;
byte LIVES = START_LIVES;
byte GAMEWIN = 0;
byte GAMEOVER = 0;
byte LEVEL = START_LEVEL;
byte ACTUALBONUS = 0;  //actual bonus icon
byte ACTIVEBONUS = 0;  //status of bonus
byte GAMEPAUSED = 0;

enum  AppState  { StateHome, StatePacman, StatePacmanEnd, StateFlappy };
AppState appState   = StateHome;
byte     menuSel    = 0;   // 0 = PACMAN, 1 = FLAPPY BIRD
byte     endMenuSel = 0;   // 0 = RESTART, 1 = MENU

#include <SPI.h>


#include "DrawIndexedMap.h"
// ST7789 display GPIO assignments (SCK=18, MOSI=23 are hardware SPI defaults)
#define CS     14   // SPI chip-select
#define DC     16   // data / command
#define RESET  17   // hardware reset

// ── Button GPIO assignments ────────────────────────────────────────────────
#define PIN_BTN_A      26   // pause / unpause
#define PIN_BTN_LEFT   4
#define PIN_BTN_DOWN   13
#define PIN_BTN_UP     27
#define PIN_BTN_RIGHT   22
ili9328SPI tft(CS, DC, RESET);


/******************************************************************************/
/*   BUTTON STATE FLAGS                                                       */
/******************************************************************************/

boolean but_A = false;
boolean but_B = false;
boolean but_UP = false;
boolean but_DOWN = false;
boolean but_LEFT = false;
boolean but_RIGHT = false;


/******************************************************************************/
/*   GAME VARIABLES AND DEFINITIONS                                           */
/******************************************************************************/

#include "PacmanTiles.h"

enum GameState {
  ReadyState,
  PlayState,
  DeadGhostState, // Player got a ghost, show score sprite and only move eyes
  DeadPlayerState,
  EndLevelState
};

enum SpriteState
{
  PenState,
  RunState,
  FrightenedState,
  DeadNumberState,
  DeadEyesState,
  AteDotState,    // pacman
  DeadPacmanState
};

enum {
  MStopped = 0,
  MRight = 1,
  MDown = 2,
  MLeft = 3,
  MUp = 4,
};

#define ushort uint16_t

#define BLINKY 0
#define PINKY 1
#define INKY  2
#define CLYDE 3
#define PACMAN 4
#define BONUS 5

const byte _initSprites[] =
{
  BLINKY,  14,     17 - 3,   31, MLeft,
  PINKY,  14 - 2,   17,     79, MLeft,
  INKY,   14,     17,     137, MLeft,
  CLYDE,  14 + 2,   17,     203, MRight,
  PACMAN, 14,     17 + 9,     0, MLeft,
  BONUS, 14,     17 + 3,     0, MLeft,
};

//  Ghost colors
const byte _palette2[] =
{
  0, 11, 1, 15, // BLINKY red
  0, 11, 3, 15, // PINKY pink
  0, 11, 5, 15, // INKY cyan
  0, 11, 7, 15, // CLYDE brown
  0, 11, 9, 9, // PACMAN yellow
  0, 11, 15, 15, // FRIGHTENED
  0, 11, 0, 15, // DEADEYES

  0, 1, 15, 2, // cherry
  0, 1, 15, 12, // strawberry
  0, 7, 2, 12, // peach
  0, 9, 15, 0, // bell

  0, 15, 1, 2, // apple
  0, 12, 15, 5, // grape
  0, 11, 9, 1, // galaxian
  0, 5, 15, 15, // key

};

const byte _paletteIcon2[] =
{
  0, 9, 9, 9, // PACMAN

  0, 2, 15, 1, // cherry
  0, 12, 15, 1, // strawberry
  0, 12, 2, 7, // peach
  0, 0, 15, 9, // bell

  0, 2, 15, 1, // apple
  0, 12, 15, 5, // grape
  0, 1, 9, 11, // galaxian
  0, 5, 15, 15, // key
};

#define PACMANICON 1
#define BONUSICON 2

#define FRIGHTENEDPALETTE 5
#define DEADEYESPALETTE 6

#define BONUSPALETTE 7

#define FPS 60
#define CHASE 0
#define SCATTER 1

#define DOT 7
#define PILL 14
#define PENGATE 0x1B

const byte _opposite[] = { MStopped, MLeft, MUp, MRight, MDown };
#define OppositeDirection(_x) pgm_read_byte(_opposite + _x)

const byte _scatterChase[] = { 7, 20, 7, 20, 5, 20, 5, 0 };
const byte _scatterTargets[] = { 2, 0, 25, 0, 0, 35, 27, 35 }; // inky/clyde scatter targets are backwards
const signed char _pinkyTargetOffset[] = { 4, 0, 0, 4, -4, 0, -4, 4 }; // Includes pinky target bug

#define FRIGHTENEDGHOSTSPRITE 0
#define GHOSTSPRITE 2
#define NUMBERSPRITE 10
#define PACMANSPRITE 14

const byte _pacLeftAnim[] = { 5, 6, 5, 4 };
const byte _pacRightAnim[] = { 2, 0, 2, 4 };
const byte _pacVAnim[] = { 4, 3, 1, 3 };

word _BonusInactiveTimmer = BONUS_INACTIVE_TIME;
word _BonusActiveTimmer = 0;
void drawBanner(const char* text);

/******************************************************************************/
/*   GAME - Sprite Class                                                      */
/******************************************************************************/

class Sprite
{
  public:
    int16_t _x, _y;
    int16_t lastx, lasty;
    byte cx, cy;        // cell x and y
    byte tx, ty;        // target x and y

    SpriteState state;
    byte  pentimer;     // frames to wait before leaving pen

    byte who;
    byte _speed;
    byte dir;
    byte phase;

    // Sprite bits
    byte palette2;  // 4->16 color map index
    byte bits;      // index of sprite bits
    signed char sy;

    void Init(const byte* s)
    {
      who = pgm_read_byte(s++);
      cx =  pgm_read_byte(s++);
      cy =  pgm_read_byte(s++);
      pentimer = pgm_read_byte(s++);
      dir = pgm_read_byte(s);
      _x = lastx = (int16_t)cx * 8 - 4;
      _y = lasty = (int16_t)cy * 8;
      state = PenState;
      _speed = 0;
      Target(random(20), random(20));
    }


    void Target(byte x, byte y)
    {
      tx = x;
      ty = y;
    }

    int16_t Distance(byte x, byte y)
    {
      int16_t dx = cx - x;
      int16_t dy = cy - y;
      return dx * dx + dy * dy;
    }

    void SetupDraw(GameState gameState, byte deadGhostIndex)
    {
      sy = 1;
      palette2 = who;
      byte p = phase >> 3;

      if (who == BONUS) {
        bits = 21 + ACTUALBONUS;
        palette2 = BONUSPALETTE + ACTUALBONUS;
        return;
      }

      if (who != PACMAN)
      {
        bits = GHOSTSPRITE + ((dir - 1) << 1) + (p & 1); // Ghosts
        switch (state)
        {
          case FrightenedState:
            bits = FRIGHTENEDGHOSTSPRITE + (p & 1); // frightened
            palette2 = FRIGHTENEDPALETTE;
            break;
          case DeadNumberState:
            palette2 = FRIGHTENEDPALETTE;
            bits = NUMBERSPRITE + deadGhostIndex;
            break;
          case DeadEyesState:
            palette2 = DEADEYESPALETTE;
            break;
          default:
            ;
        }
        return;
      }

      //  PACMAN animation
      byte f = (phase >> 1) & 3;
      if (dir == MLeft)
        f = pgm_read_byte(_pacLeftAnim + f);
      else if (dir == MRight)
        f = pgm_read_byte(_pacRightAnim + f);
      else
        f = pgm_read_byte(_pacVAnim + f);
      if (dir == MUp)
        sy = -1;
      bits = f + PACMANSPRITE;
    }

    //  Draw this sprite into the tile at x,y
    void Draw8(int16_t x, int16_t y, byte* tile)
    {
      int16_t px = x - (_x - 4);
      if (px <= -8 || px >= 16) return;
      int16_t py = y - (_y - 4);
      if (py <= -8 || py >= 16) return;

      // Clip y
      int16_t lines = py + 8;
      if (lines > 16)
        lines = 16;
      if (py < 0)
      {
        tile -= py * 8;
        py = 0;
      }
      lines -= py;

      //  Clip in X
      byte right = 16 - px;
      if (right > 8)
        right = 8;
      byte left = 0;
      if (px < 0)
      {
        left = -px;
        px = 0;
      }

      //  Get bitmap
      signed char dy = sy;
      if (dy < 0)
        py = 15 - py;  // VFlip
      byte* data = (byte*)(pacman16x16 + bits * 64);
      data += py << 2;
      dy <<= 2;
      data += px >> 2;
      px &= 3;

      const byte* palette = _palette2 + (palette2 << 2);
      while (lines)
      {
        const byte *src = data;
        byte d = pgm_read_byte(src++);
        d >>= px << 1;
        byte sx = 4 - px;
        byte x = left;
        do
        {
          byte p = d & 3;
          if (p)
          {
            p = pgm_read_byte(palette + p);
            if (p)
              tile[x] = p;
          }
          d >>= 2;    // Next pixel
          if (!--sx)
          {
            d = pgm_read_byte(src++);
            sx = 4;
          }
        } while (++x < right);

        tile += 8;
        data += dy;
        lines--;
      }
    }
};

/******************************************************************************/
/*   GAME - Playfield Class                                                   */
/******************************************************************************/

class Playfield
{

    Sprite _sprites[5];

    Sprite _BonusSprite;

    byte _dotMap[(32 / 4) * (36 - 6)];

    GameState _state;
    long    _score;
    long    _hiscore;   // 7 digits max
    long    _lifescore;
    signed char    _scoreStr[8];
    signed char    _hiscoreStr[8];
    byte    _icons[14];         // Along bottom of screen

    ushort  _stateTimer;
    ushort  _frightenedTimer;
    byte    _frightenedCount;
    byte    _scIndex;
    ushort  _scTimer;           // next change of sc status
    uint16_t _dotsLeft;         // remaining dots — drives Cruise Elroy thresholds

    bool _inited;
  public:
    Playfield() : _inited(false) {}

    void forceRedraw() {
      tft.fillRect(0, 0, TFT_WIDTH, TFT_HEIGHT, 0x0000);
      memset(updateMap, 0, sizeof(updateMap));
      DrawAllBG();
    }

    // Draw 2 bit BG into 8 bit icon tiles at bottom
    void DrawBG2(byte cx, byte cy, byte* tile)
    {
      byte index = 0;
      signed char b = 0;

      index = _icons[cx >> 1];   // 13 icons across bottom
      if (index == 0)
      {
        memset(tile, 0, 64);
        return;
      }
      index--;
      index <<= 2;                        // 4 tiles per icon

      b = (1 - (cx & 1)) + ((cy & 1) << 1); // Index of tile

      const byte* bg = pacman8x8x2 + ((b + index) << 4);
      const byte* palette = _paletteIcon2 + index;


      byte x = 16;
      while (x--)
      {
        byte bits = (signed char)pgm_read_byte(bg++);
        byte i = 4;
        while (i--)
        {
          tile[i] = pgm_read_byte(palette + (bits & 3));
          bits >>= 2;
        }
        tile += 4;
      }
    }

    byte GetTile(int16_t cx, int16_t ty)
    {

      if (_state != ReadyState && ty == 20 && cx > 10 && cx < 17) return (0); //READY TEXT ZONE

      if (LEVEL % 5 == 1) return pgm_read_byte(playMap1 + ty * 28 + cx);
      if (LEVEL % 5 == 2) return pgm_read_byte(playMap2 + ty * 28 + cx);
      if (LEVEL % 5 == 3) return pgm_read_byte(playMap3 + ty * 28 + cx);
      if (LEVEL % 5 == 4) return pgm_read_byte(playMap4 + ty * 28 + cx);
      return pgm_read_byte(playMap5 + ty * 28 + cx);
    }

    // Draw 1 bit BG into 8 bit tile
    void DrawBG(byte cx, byte cy, byte* tile)
    {
      if (cy >= 34) // icon row below maze
      {
        DrawBG2(cx, cy, tile);
        return;
      }

      byte c = 11;
      if (LEVEL % 8 == 1) c = 11; // Blue
      if (LEVEL % 8 == 2) c = 12; // Green
      if (LEVEL % 8 == 3) c = 1; // Red
      if (LEVEL % 8 == 4) c = 9; // Yellow
      if (LEVEL % 8 == 5) c = 2; // Brown
      if (LEVEL % 8 == 6) c = 5; // Cyan
      if (LEVEL % 8 == 7) c = 3; // Pink
      if (LEVEL % 8 == 0) c = 15; // White

      byte b = GetTile(cx, cy);
      const byte* bg;

      memset(tile, 0, 64);
      if (cy == 20 && cx >= 11 && cx < 17)
      {
        if ((_state != ReadyState && GAMEPAUSED != 1) || ACTIVEBONUS == 1) b = 0; // hide 'READY!'
        else if (GAMEPAUSED == 1 && cx == 11) b = 'P';
        else if (GAMEPAUSED == 1 && cx == 12) b = 'A';
        else if (GAMEPAUSED == 1 && cx == 13) b = 'U';
        else if (GAMEPAUSED == 1 && cx == 14) b = 'S';
        else if (GAMEPAUSED == 1 && cx == 15) b = 'E';
        else if (GAMEPAUSED == 1 && cx == 16) b = 'D';
      }
      else if (cy == 1)
      {
        if (cx < 7)
          b = _scoreStr[cx];
        else if (cx >= 10 && cx < 17)
          b = _hiscoreStr[cx - 10]; // HiScore
      } else {
        if (b == DOT || b == PILL)  // DOT==7, PILL==14
        {
          if (!GetDot(cx, cy))
            return;
          c = 14;
        }
        if (b == PENGATE)
          c = 14;
      }

      bg = playTiles + (b << 3);
      if (b >= '0')
        c = 15; // text is white

      for (byte y = 0; y < 8; y++)
      {
        signed char bits = (signed char)pgm_read_byte(bg++);  // signed: MSB drives fill direction
        byte x = 0;
        while (bits)
        {
          if (bits < 0)
            tile[x] = c;
          bits <<= 1;
          x++;
        }
        tile += 8;
      }
    }

    // Draw BG then all sprites in this cell
    void Draw(uint16_t x, uint16_t y, bool sprites)
    {

      byte tile[8 * 8];

      //      Fill with BG
      DrawBG(x, y, tile);

      //      Overlay sprites (sprite logic stays at 8px/tile resolution)
      uint16_t px = x << 3;
      uint16_t py = y << 3;
      if (sprites)
      {
        for (byte i = 0; i < 5; i++)
          _sprites[i].Draw8(px, py, tile);

        if (ACTIVEBONUS) _BonusSprite.Draw8(px, py, tile);

      }

      // Screen coords: 6px/tile, 1px left margin, 52px top margin
      // Label row (y==0) is shifted 2px up so it doesn't visually merge with the score row.
      uint16_t sx = x * 6 + (170 - 168) / 2;
      uint16_t sy = y * 6 + (320 - 216) / 2 - (y == 0 ? 2 : 0);

      drawIndexedmap(&tft, tile, sx, sy);
    }

    boolean updateMap [36][28];

    void Mark(int16_t x, int16_t y)
    {
      x -= 4;
      y -= 4;

      updateMap[(y >> 3)][(x >> 3)] = true;
      updateMap[(y >> 3)][(x >> 3) + 1] = true;
      updateMap[(y >> 3)][(x >> 3) + 2] = true;
      updateMap[(y >> 3) + 1][(x >> 3)] = true;
      updateMap[(y >> 3) + 1][(x >> 3) + 1] = true;
      updateMap[(y >> 3) + 1][(x >> 3) + 2] = true;
      updateMap[(y >> 3) + 2][(x >> 3)] = true;
      updateMap[(y >> 3) + 2][(x >> 3) + 1] = true;
      updateMap[(y >> 3) + 2][(x >> 3) + 2] = true;

    }

    void DrawAllBG()
    {
      for (byte y = 0; y < 36; y++)
        for (byte x = 0; x < 28; x++) {
          Draw(x, y, false);
        }
      drawBanner("PACMAN");
    }

    void DrawAll()
    {
      //  Mark sprite old/new positions as dirty
      for (byte i = 0; i < 5; i++)
      {
        Sprite* s = _sprites + i;
        Mark(s->lastx, s->lasty);
        Mark(s->_x, s->_y);

      }

      // Mark BONUS sprite old/new positions as dirty
      Sprite* _s = &_BonusSprite;
      Mark(_s->lastx, _s->lasty);
      Mark(_s->_x, _s->_y);


      for (byte i = 0; i < 5; i++)
        _sprites[i].SetupDraw(_state, _frightenedCount - 1);

      _BonusSprite.SetupDraw(_state, _frightenedCount - 1);


      for (byte tmpY = 0; tmpY < 36; tmpY++) {
        for (byte tmpX = 0; tmpX < 28; tmpX++) {
          if (updateMap[tmpY][tmpX] == true) Draw(tmpX, tmpY, true);
          updateMap[tmpY][tmpX] = false;
        }

      }
    }


    int16_t Chase(Sprite* s, int16_t cx, int16_t cy)
    {
      while (cx < 0)      //  Tunneling
        cx += 28;
      while (cx >= 28)
        cx -= 28;

      byte t = GetTile(cx, cy);
      if (!(t == 0 || t == DOT || t == PILL || t == PENGATE))
        return 0x7FFF;

      if (t == PENGATE)
      {
        if (s->who == PACMAN)
          return 0x7FFF;  // Pacman can't cross this to enter pen
        if (!(InPen(s->cx, s->cy) || s->state == DeadEyesState))
          return 0x7FFF;  // Can cross if dead or in pen trying to get out
      }

      int16_t dx = s->tx - cx;
      int16_t dy = s->ty - cy;
      return (dx * dx + dy * dy);

    }

    void UpdateTimers()
    {
      // Update scatter/chase selector, low bit of index indicates scatter.
      // On every mode transition all active ghosts reverse — original arcade behaviour.
      if (_scIndex < 8)
      {
        if (_scTimer-- == 0)
        {
          byte duration = pgm_read_byte(_scatterChase + _scIndex++);
          _scTimer = duration * FPS;
          for (byte i = 0; i < 4; i++) {
            Sprite* g = _sprites + i;
            if (g->state == RunState || g->state == FrightenedState)
              g->dir = OppositeDirection(g->dir);
          }
        }
      }

      // Bonus timer
      if (ACTIVEBONUS == 0 && _BonusInactiveTimmer-- == 0) {
        _BonusActiveTimmer = BONUS_ACTIVE_TIME;
        ACTIVEBONUS = 1;
      }
      if (ACTIVEBONUS == 1 && _BonusActiveTimmer-- == 0) {
        _BonusInactiveTimmer = BONUS_INACTIVE_TIME;
        ACTIVEBONUS = 0;
      }


      //  Release frightened ghosts
      if (_frightenedTimer && !--_frightenedTimer)
      {
        for (byte i = 0; i < 4; i++)
        {
          Sprite* s = _sprites + i;
          if (s->state == FrightenedState)
          {
            s->state = RunState;
            s->dir = OppositeDirection(s->dir);
          }
        }
      }
    }

    void Scatter(Sprite* s)
    {
      const byte* st = _scatterTargets + (s->who << 1);
      s->Target(pgm_read_byte(st), pgm_read_byte(st + 1));
    }

    void UpdateTargets()
    {
      if (_state == ReadyState)
        return;
      Sprite* pacman = _sprites + PACMAN;

      //  Ghost AI
      bool scatter = _scIndex & 1;
      for (byte i = 0; i < 4; i++)
      {
        Sprite* s = _sprites + i;

        //  Deal with returning ghost to pen
        if (s->state == DeadEyesState)
        {
          if (s->cx == 14 && s->cy == 17) // returned to pen
          {
            s->state = PenState;        // Revived in pen
            s->pentimer = 80;
          }
          else
            s->Target(14, 17);          // target pen entrance
          continue;
        }

        //  Release ghost from pen when timer expires
        if (s->pentimer)
        {
          if (--s->pentimer)  // stay in pen a while longer
            continue;
          s->state = RunState;
        }

        if (InPen(s->cx, s->cy))
        {
          s->Target(14, 14 - 2); // Get out of pen first
        } else {
          if (scatter || s->state == FrightenedState)
            Scatter(s);
          else
          {
            // Chase mode targeting
            signed char tx = pacman->cx;
            signed char ty = pacman->cy;
            switch (s->who)
            {
              case PINKY:
                {
                  const signed char* pto = _pinkyTargetOffset + ((pacman->dir - 1) << 1);
                  tx += pgm_read_byte(pto);
                  ty += pgm_read_byte(pto + 1);
                }
                break;
              case INKY:
                {
                  const signed char* pto = _pinkyTargetOffset + ((pacman->dir - 1) << 1);
                  Sprite* binky = _sprites + BLINKY;
                  tx += pgm_read_byte(pto) >> 1;
                  ty += pgm_read_byte(pto + 1) >> 1;
                  tx += tx - binky->cx;
                  ty += ty - binky->cy;
                }
                break;
              case CLYDE:
                {
                  if (s->Distance(pacman->cx, pacman->cy) < 64)
                  {
                    const byte* st = _scatterTargets + CLYDE * 2;
                    tx = pgm_read_byte(st);
                    ty = pgm_read_byte(st + 1);
                  }
                }
                break;
            }
            s->Target(tx, ty);
          }
        }
      }
    }

    byte ChooseDir(int16_t dir, Sprite* s)
    {
      int16_t choice[4];
      choice[0] = Chase(s, s->cx, s->cy - 1); // Up
      choice[1] = Chase(s, s->cx - 1, s->cy); // Left
      choice[2] = Chase(s, s->cx, s->cy + 1); // Down
      choice[3] = Chase(s, s->cx + 1, s->cy); // Right

      // ── Pacman: player input ─────────────────────────────────────────────
      if (s->who == PACMAN) {
        if (choice[0] < 0x7FFF && but_UP)    return MUp;
        if (choice[1] < 0x7FFF && but_LEFT)  return MLeft;
        if (choice[2] < 0x7FFF && but_DOWN)  return MDown;
        if (choice[3] < 0x7FFF && but_RIGHT) return MRight;
        // Continue in current direction if still open
        if (choice[0] < 0x7FFF && dir == MUp)    return MUp;
        if (choice[1] < 0x7FFF && dir == MLeft)  return MLeft;
        if (choice[2] < 0x7FFF && dir == MDown)  return MDown;
        if (choice[3] < 0x7FFF && dir == MRight) return MRight;
        return MStopped;
      }

      // ── Ghost AI ─────────────────────────────────────────────────────────
      byte opposite = OppositeDirection(dir);

      // Frightened: random valid direction — matches original arcade RNG behaviour
      if (s->state == FrightenedState) {
        byte valid[4], count = 0;
        for (byte i = 0; i < 4; i++) {
          byte d = 4 - i;
          if (choice[i] < 0x7FFF && d != opposite)
            valid[count++] = d;
        }
        if (count) return valid[random(count)];
      }

      // Chase / scatter: greedy minimum distance to target, no reversals
      if (dir < 1 || dir > 4) return MStopped;  // guard against invalid direction
      int16_t dist = choice[4 - dir]; // favour current direction on ties
      for (byte i = 0; i < 4; i++) {
        byte d = 4 - i;
        if (d != opposite && choice[i] < dist) {
          dist = choice[i];
          dir  = d;
        }
      }
      return dir;
    }

    bool InPen(byte cx, byte cy)
    {
      if (cx <= 10 || cx >= 18) return false;
      if (cy <= 14 || cy >= 18) return false;
      return true;
    }

    byte GetSpeed(Sprite* s)
    {
      if (s->who == PACMAN)
        return _frightenedTimer ? 90 : 80;
      if (s->state == FrightenedState)
        return 40;
      if (s->state == DeadEyesState)
        return 100;
      if (s->cy == 17 && (s->cx <= 5 || s->cx > 20))
        return 40;  // tunnel

      // Cruise Elroy: Blinky speeds up as dots are depleted.
      // Thresholds scale with level, matching the original arcade values.
      if (s->who == BLINKY) {
        byte lvl    = (LEVEL < 7) ? LEVEL : 7;  // caps at level 7
        byte elroy1 = 10 + lvl * 10;             // 20 @ L1, 30 @ L2 … 80 @ L7+
        byte elroy2 = elroy1 >> 1;
        if (_dotsLeft <= elroy2) return 90;      // Elroy 2: 90 % speed
        if (_dotsLeft <= elroy1) return 85;      // Elroy 1: 85 % speed
      }

      return 75;
    }

    void PackmanDied() {  // Noooo... PACMAN DIED :(

      if (LIVES <= 0) {
        GAMEOVER = 1;
        LEVEL = START_LEVEL;
        LIVES = START_LIVES;
        Init();
      } else {
        LIVES--;

        _inited = true;
        _state = ReadyState;
        _stateTimer = FPS / 2;
        _frightenedCount = 0;
        _frightenedTimer = 0;

        const byte* s = _initSprites;
        for (int16_t i = 0; i < 5; i++)
          _sprites[i].Init(s + i * 5);

        _scIndex = 0;
        _scTimer = 1;

        memset(_icons, 0, sizeof(_icons));

        _BonusSprite.Init(s + 5 * 5);
        _BonusInactiveTimmer = BONUS_INACTIVE_TIME;
        _BonusActiveTimmer = 0;

        for (byte i = 0; i < ACTUALBONUS; i++) {
          _icons[13 - i] = BONUSICON + i;
        }

        for (byte i = 0; i < LIVES; i++) {
          _icons[0 + i] = PACMANICON;
        }

        for (byte y = 34; y < 36; y++)
          for (byte x = 0; x < 28; x++) {
            Draw(x, y, false);
          }

        DrawAllBG();
      }
    }


    void MoveAll()
    {
      UpdateTimers();
      UpdateTargets();


      //  Update game state
      if (_stateTimer)
      {
        if (--_stateTimer <= 0)
        {
          switch (_state)
          {
            case ReadyState:
              _state = PlayState;
              for (byte tmpX = 11; tmpX < 17; tmpX++) Draw(tmpX, 20, false); // clear 'READY!'

              break;
            case DeadGhostState:
              _state = PlayState;
              for (byte i = 0; i < 4; i++)
              {
                Sprite* s = _sprites + i;
                if (s->state == DeadNumberState)
                  s->state = DeadEyesState;
              }
              break;
            default:
              ;
          }
        } else {
          if (_state == ReadyState)
            return;
        }
      }

      for (byte i = 0; i < 5; i++)
      {
        Sprite* s = _sprites + i;

        //  In DeadGhostState, only eyes move
        if (_state == DeadGhostState && s->state != DeadEyesState)
          continue;

        //  Calculate speed
        s->_speed += GetSpeed(s);
        if (s->_speed < 100)
          continue;
        s->_speed -= 100;

        s->lastx = s->_x;
        s->lasty = s->_y;
        s->phase++;

        int16_t x = s->_x;
        int16_t y = s->_y;


        if ((x & 0x7) == 0 && (y & 0x7) == 0)   // cell aligned
          s->dir = ChooseDir(s->dir, s);      // time to choose another direction


        switch (s->dir)
        {
          case MLeft:     x -= SPEED; break;
          case MRight:    x += SPEED; break;
          case MUp:       y -= SPEED; break;
          case MDown:     y += SPEED; break;
          case MStopped:  break;
        }

        //  Wrap x because of tunnels
        while (x < 0)
          x += 224;
        while (x >= 224)
          x -= 224;

        s->_x = x;
        s->_y = y;
        s->cx = (x + 4) >> 3;
        s->cy = (y + 4) >> 3;

        if (s->who == PACMAN)
          EatDot(s->cx, s->cy);
      }

      //  Collide
      Sprite* pacman = _sprites + PACMAN;

      //  Collide with BONUS
      Sprite* _s = &_BonusSprite;
      if (ACTIVEBONUS == 1 && _s->cx == pacman->cx && _s->cy == pacman->cy)
      {
        Score(ACTUALBONUS * 50);
        ACTUALBONUS++;
        if (ACTUALBONUS > 7) {
          ACTUALBONUS = 0;
          if (LIVES < MAXLIVES) LIVES++;

            memset(_icons, 0, sizeof(_icons));

          for (byte i = 0; i < LIVES; i++) {
            _icons[0 + i] = PACMANICON;
          }

        }

        for (byte i = 0; i < ACTUALBONUS; i++) {
          _icons[13 - i] = BONUSICON + i;
        }

        for (byte y = 34; y < 36; y++)
          for (byte x = 0; x < 28; x++) {
            Draw(x, y, false);
          }

        ACTIVEBONUS = 0;
        _BonusInactiveTimmer = BONUS_INACTIVE_TIME;
      }

      for (byte i = 0; i < 4; i++)
      {
        Sprite* s = _sprites + i;
        if (s->_x + SPEED >= pacman->_x && s->_x - SPEED <= pacman->_x && s->_y + SPEED >= pacman->_y && s->_y - SPEED <= pacman->_y)



        {
          if (s->state == FrightenedState)
          {
            s->state = DeadNumberState;     // Killed a ghost
            _frightenedCount++;
            _state = DeadGhostState;
            _stateTimer = 10;
            Score((1 << _frightenedCount) * 100);
          }
          else {               // pacman died
            if (s->state == DeadNumberState || s->state == FrightenedState || s->state == DeadEyesState) {
            } else {
              PackmanDied();
            }
          }
        }
      }
    }

    //  Mark a position dirty
    void Mark(int16_t pos)
    {
      for (byte tmp = 0; tmp < 28; tmp++)
        updateMap[1][tmp] = true;

    }

    void SetScoreChar(byte i, signed char c)
    {
      if (_scoreStr[i] == c)
        return;
      _scoreStr[i] = c;
      Mark(i + 32);

    }

    void SetHiScoreChar(byte i, signed char c)
    {
      if (_hiscoreStr[i] == c)
        return;
      _hiscoreStr[i] = c;
      Mark(i + 32 + 10);
    }

    void Score(int16_t delta)
    {
      char str[8];
      _score += delta;
      if (_score > _hiscore) _hiscore = _score;

      if (_score > _lifescore && _score % 10000 > 0) {
        _lifescore = (_score / 10000 + 1) * 10000;

        LIVES++; // EVERY 10000 points = 1UP

        for (byte i = 0; i < LIVES; i++) {
          _icons[0 + i] = PACMANICON;
        }

        for (byte y = 34; y < 36; y++)
          for (byte x = 0; x < 28; x++) {
            Draw(x, y, false);
          }
        _score = _score + 100;
      }

      sprintf(str, "%ld", _score);
      if (strlen(str) > 7) strcpy(str, "9999999");
      byte i = 7 - strlen(str);
      byte j = 0;
      while (i < 7)
        SetScoreChar(i++, str[j++]);
      sprintf(str, "%ld", _hiscore);
      if (strlen(str) > 7) strcpy(str, "9999999");
      i = 7 - strlen(str);
      j = 0;
      while (i < 7)
        SetHiScoreChar(i++, str[j++]);
    }

    bool GetDot(byte cx, byte cy)
    {
      return _dotMap[(cy - 3) * 4 + (cx >> 3)] & (0x80 >> (cx & 7));
    }

    void EatDot(byte cx, byte cy)
    {
      if (!GetDot(cx, cy))
        return;
      byte mask = 0x80 >> (cx & 7);
      _dotMap[(cy - 3) * 4 + (cx >> 3)] &= ~mask;

      byte t = GetTile(cx, cy);
      if (t == PILL)
      {
        _frightenedTimer = 10 * FPS;
        _frightenedCount = 0;
        for (byte i = 0; i < 4; i++)
        {
          Sprite* s = _sprites + i;
          if (s->state == RunState)
          {
            s->state = FrightenedState;
            s->dir = OppositeDirection(s->dir);
          }
        }
        Score(50);
      }
      else
        Score(10);

      if (_dotsLeft) _dotsLeft--;
      if (!_dotsLeft) GAMEWIN = 1;
    }

    void Init()
    {
      if (GAMEWIN == 1) {
        GAMEWIN = 0;
      } else {
        LEVEL = START_LEVEL;
        LIVES = START_LIVES;
        ACTUALBONUS = 0; //actual bonus icon
        ACTIVEBONUS = 0; //status of bonus

        _score = 0;
        _lifescore = 10000;

        memset(_scoreStr, 0, sizeof(_scoreStr));
        _scoreStr[5] = _scoreStr[6] = '0';
      }


      _inited = true;
      _state = ReadyState;
      _stateTimer = FPS / 2;
      _frightenedCount = 0;
      _frightenedTimer = 0;

      const byte* s = _initSprites;
      for (int16_t i = 0; i < 5; i++)
        _sprites[i].Init(s + i * 5);

      _BonusSprite.Init(s + 5 * 5);
      _BonusInactiveTimmer = BONUS_INACTIVE_TIME;
      _BonusActiveTimmer = 0;

      _scIndex = 0;
      _scTimer = 1;

      memset(_icons, 0, sizeof(_icons));

      for (byte i = 0; i < ACTUALBONUS; i++) {
        _icons[13 - i] = BONUSICON + i;
      }

      for (byte i = 0; i < LIVES; i++) {
        _icons[0 + i] = PACMANICON;
      }

      for (byte y = 34; y < 36; y++)
        for (byte x = 0; x < 28; x++) {
          Draw(x, y, false);
        }

      _dotsLeft = 0;
      memset(_dotMap, 0, sizeof(_dotMap));
      byte* map = _dotMap;
      for (byte y = 3; y < 36 - 3; y++) // rows 3–32: playfield interior
      {
        for (byte x = 0; x < 28; x++)
        {
          byte t = GetTile(x, y);
          if (t == 7 || t == 14)
          {
            byte s = x & 7;
            map[x >> 3] |= (0x80 >> s);
            _dotsLeft++;
          }
        }
        map += 4;
      }
      DrawAllBG();
    }

    void Step()
    {
      if (GAMEWIN == 1) {
        LEVEL++;
        Init();
      }

      // Pause toggle (button A)
      if (but_A && !GAMEPAUSED) {
        but_A = false;
        GAMEPAUSED = 1;
      } else if (but_A && GAMEPAUSED) {
        but_A = false;
        GAMEPAUSED = 0;
        for (byte tmpX = 11; tmpX < 17; tmpX++) Draw(tmpX, 20, false);
      }

      // Reset (button B) or auto-init on first run
      if (but_B) {
        Init();
      } else if (!_inited) {
        Init();
      }

      if (!GAMEPAUSED) MoveAll();

      if (GAMEPAUSED) for (byte tmpX = 11; tmpX < 17; tmpX++) Draw(tmpX, 20, false);

      DrawAll();
    }
};

// ── Font helpers (reuse the game's 8×8 playTiles font) ────────────────────
void drawChar(char c, int16_t x, int16_t y, uint16_t color, byte scale = 1) {
  if (c == ' ') return;
  const byte* glyph = playTiles + ((byte)c << 3);
  for (byte row = 0; row < 8; row++) {
    signed char bits = (signed char)pgm_read_byte(glyph + row);
    for (byte col = 0; col < 8 && bits; col++) {
      if (bits < 0)
        tft.fillRect(x + col * scale, y + row * scale, scale, scale, color);
      bits <<= 1;
    }
  }
}

void drawStr(const char* s, int16_t x, int16_t y, uint16_t color, byte scale = 1) {
  while (*s) { drawChar(*s++, x, y, color, scale); x += 8 * scale; }
}

// ── Flappy Bird ───────────────────────────────────────────────────────────
class FlappyBird {
  static const int16_t BIRD_X   = 35;
  static const int16_t BIRD_SZ  = 10;
  static const int16_t PIPE_W   = 26;
  static const int16_t GAP_H    = 90;
  static const int16_t PIPE_SPD = 2;
  static const int16_t GROUND_H = 10;
  static const byte    NPIPES   = 2;

  int16_t  birdY, birdVel;
  int16_t  pipeX[NPIPES], gapY[NPIPES];
  bool     passed[NPIPES];
  uint16_t score;
  bool     dead, started;

  uint16_t COL_SKY, COL_PIPE, COL_BIRD, COL_GROUND, COL_WHITE, COL_RED;

  void safeFill(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
    if (x >= TFT_WIDTH || y >= TFT_HEIGHT || x + w <= 0 || y + h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > TFT_WIDTH)  w = TFT_WIDTH  - x;
    if (y + h > TFT_HEIGHT) h = TFT_HEIGHT - y;
    if (w > 0 && h > 0) tft.fillRect(x, y, w, h, c);
  }

  void pipeCols(int16_t x, int16_t w, int16_t gy, uint16_t c) {
    safeFill(x, 0,           w, gy,                            c);
    safeFill(x, gy + GAP_H,  w, TFT_HEIGHT - GROUND_H - gy - GAP_H, c);
  }

  void bird(uint16_t c) {
    tft.fillRect(BIRD_X, birdY - BIRD_SZ / 2, BIRD_SZ, BIRD_SZ, c);
  }

  void showScore() {
    char buf[8]; sprintf(buf, "%u", score);
    safeFill(2, 2, 50, 10, COL_SKY);
    drawStr(buf, 4, 2, COL_WHITE);
  }

public:
  void init() {
    birdY = TFT_HEIGHT / 2; birdVel = 0;
    score = 0; dead = false; started = false;

    COL_SKY    = tft.color565(135, 206, 235);
    COL_PIPE   = tft.color565( 60, 160,  60);
    COL_BIRD   = tft.color565(255, 200,   0);
    COL_GROUND = tft.color565(180, 120,  60);
    COL_WHITE  = tft.color565(255, 255, 255);
    COL_RED    = tft.color565(255,  60,  60);

    pipeX[0] = TFT_WIDTH + 20;
    pipeX[1] = TFT_WIDTH + 20 + TFT_WIDTH / 2 + PIPE_W;
    for (byte i = 0; i < NPIPES; i++) {
      gapY[i]   = random(30, TFT_HEIGHT - GAP_H - GROUND_H - 30);
      passed[i] = false;
    }

    tft.fillRect(0, 0, TFT_WIDTH, TFT_HEIGHT - GROUND_H, COL_SKY);
    tft.fillRect(0, TFT_HEIGHT - GROUND_H, TFT_WIDTH, GROUND_H, COL_GROUND);
    bird(COL_BIRD);
    showScore();
    drawStr("PRESS A", (TFT_WIDTH - 7 * 8) / 2, TFT_HEIGHT / 2 + 20, COL_WHITE);
  }

  void flap() {
    if (!started) {
      started = true;
      safeFill(0, TFT_HEIGHT / 2 + 16, TFT_WIDTH, 12, COL_SKY);
    }
    birdVel = -20;
  }

  bool step() {
    if (!started || dead) return !dead;

    bird(COL_SKY);   // erase

    for (byte i = 0; i < NPIPES; i++) {
      pipeCols(pipeX[i] + PIPE_W - PIPE_SPD, PIPE_SPD, gapY[i], COL_SKY);  // trailing edge
      pipeX[i] -= PIPE_SPD;

      if (!passed[i] && pipeX[i] + PIPE_W / 2 < BIRD_X) {
        passed[i] = true; score++; showScore();
      }
      if (pipeX[i] + PIPE_W <= 0) {
        pipeX[i] = TFT_WIDTH + 10;
        gapY[i]  = random(30, TFT_HEIGHT - GAP_H - GROUND_H - 30);
        passed[i] = false;
      }
      pipeCols(pipeX[i], PIPE_SPD, gapY[i], COL_PIPE);  // leading edge
    }

    birdVel += 2;
    birdY   += birdVel >> 2;
    if (birdY < BIRD_SZ / 2) { birdY = BIRD_SZ / 2; birdVel = 0; }
    if (birdY > TFT_HEIGHT - GROUND_H - BIRD_SZ / 2) dead = true;

    for (byte i = 0; i < NPIPES; i++) {
      if (pipeX[i] < BIRD_X + BIRD_SZ && pipeX[i] + PIPE_W > BIRD_X) {
        if (birdY - BIRD_SZ / 2 < gapY[i] || birdY + BIRD_SZ / 2 > gapY[i] + GAP_H)
          dead = true;
      }
    }

    if (dead) {
      char buf[16]; sprintf(buf, "SCORE  %u", score);
      drawStr("GAME OVER",  (TFT_WIDTH - 9 * 8) / 2, TFT_HEIGHT / 2 - 16, COL_RED);
      drawStr(buf,          (TFT_WIDTH - (int16_t)strlen(buf) * 8) / 2, TFT_HEIGHT / 2,     COL_WHITE);
      drawStr("RIGHT MENU", (TFT_WIDTH - 10 * 8) / 2, TFT_HEIGHT / 2 + 16, COL_WHITE);
      drawStr("A  RETRY",   (TFT_WIDTH -  8 * 8) / 2, TFT_HEIGHT / 2 + 28, COL_WHITE);
      return false;
    }

    bird(COL_BIRD);  // redraw
    return true;
  }

  bool isDead() { return dead; }
};

FlappyBird flappyGame;
Playfield  _game;

/******************************************************************************/
/*   SETUP                                                                    */
/******************************************************************************/

#define BLACK 0x0000

// Draws "PACMAN" in the 52 px header band using the game's 8×8 font tiles scaled 2×.
void drawBanner(const char* text) {
  const byte     scale = 2;
  const uint16_t col   = tft.color565(255, 220, 0);
  byte           len   = strlen(text);
  uint16_t       textW = len * 8 * scale;
  uint16_t       x0    = (TFT_WIDTH - textW) / 2;
  uint16_t       y0    = (52 - 8 * scale) / 2;

  for (byte ci = 0; ci < len; ci++) {
    uint16_t cx = x0 + ci * (8 * scale);
    const byte* glyph = playTiles + ((byte)text[ci] << 3);
    for (byte row = 0; row < 8; row++) {
      signed char bits = (signed char)pgm_read_byte(glyph + row);
      for (byte col2 = 0; col2 < 8 && bits; col2++) {
        if (bits < 0)
          tft.fillRect(cx + col2 * scale, y0 + row * scale, scale, scale, col);
        bits <<= 1;
      }
    }
  }
}

void drawHomeScreen() {
  const uint16_t COL_SELBG  = tft.color565( 20,  20,  80);
  const uint16_t COL_YELLOW = tft.color565(255, 220,   0);
  const uint16_t COL_WHITE  = tft.color565(220, 220, 220);
  const uint16_t COL_GREY   = tft.color565(110, 110, 110);
  const uint16_t COL_DIM    = tft.color565( 70,  70,  70);

  tft.fillRect(0, 0, TFT_WIDTH, TFT_HEIGHT, BLACK);
  drawBanner("ARCADE");

  drawStr("SELECT  GAME", (TFT_WIDTH - 12 * 8) / 2, 72, COL_WHITE);

  const char*  labels[] = { "PACMAN", "FLAPPY BIRD" };
  const byte   lens[]   = {  6,        11           };
  const int16_t ys[]    = { 148,       198          };

  for (byte i = 0; i < 2; i++) {
    bool     sel = (i == menuSel);
    uint16_t bg  = sel ? COL_SELBG : BLACK;
    uint16_t fg  = sel ? COL_YELLOW : COL_GREY;
    tft.fillRect(0, ys[i] - 4, TFT_WIDTH, 16, bg);
    int16_t tx = (TFT_WIDTH - lens[i] * 8) / 2;
    drawStr(labels[i], tx, ys[i], fg);
    if (sel) drawStr(">", tx - 10, ys[i], COL_YELLOW);
  }

  drawStr("UP DN  MOVE",  (TFT_WIDTH - 11 * 8) / 2, 264, COL_DIM);
  drawStr("A  START",     (TFT_WIDTH -  8 * 8) / 2, 276, COL_DIM);
}

void homeScreenLoop() {
  static byte prevSel  = 0;
  static bool prevUp   = false, prevDown = false, prevA = false;

  bool currUp   = (digitalRead(PIN_BTN_UP)   == LOW);
  bool currDown = (digitalRead(PIN_BTN_DOWN) == LOW);
  bool currA    = (digitalRead(PIN_BTN_A)    == LOW);

  if ((currUp && !prevUp) || (currDown && !prevDown))
    menuSel ^= 1;

  if (currA && !prevA) {
    if (menuSel == 0) {
      but_A    = false;  // prevent A-press carrying over into Pacman's pause handler
      appState = StatePacman;
      _game.forceRedraw();
    } else {
      appState = StateFlappy;
      flappyGame.init();
    }
  }

  prevUp = currUp; prevDown = currDown; prevA = currA;

  if (menuSel != prevSel) {
    prevSel = menuSel;
    drawHomeScreen();
  }
}

void drawPacmanEndMenu() {
  const int16_t  BX = 15, BY = 115, BW = 140, BH = 86;
  const uint16_t BG  = tft.color565( 10,  10,  60);
  const uint16_t YEL = tft.color565(255, 220,   0);
  const uint16_t GRY = tft.color565(110, 110, 110);

  tft.fillRect(BX, BY, BW, BH, BG);
  tft.drawFastHLine(BX,          BY,          BW, YEL);
  tft.drawFastHLine(BX,          BY + BH - 1, BW, YEL);
  tft.drawFastVLine(BX,          BY,          BH, YEL);
  tft.drawFastVLine(BX + BW - 1, BY,          BH, YEL);

  drawStr("GAME OVER", BX + (BW - 9 * 8) / 2, BY + 8, YEL);

  const char*  labels[] = { "RESTART", "MENU" };
  const int16_t  ys[]   = { BY + 36, BY + 58 };

  for (byte i = 0; i < 2; i++) {
    uint16_t fg = (i == endMenuSel) ? YEL : GRY;
    int16_t  tx = BX + (BW - (int16_t)strlen(labels[i]) * 8) / 2;
    tft.fillRect(BX + 1, ys[i] - 2, BW - 2, 14, BG);
    drawStr(labels[i], tx, ys[i], fg);
    if (i == endMenuSel) drawStr(">", BX + 8, ys[i], YEL);
  }
}

void setup() {
  randomSeed(analogRead(0));
  Serial.begin(115200);

  pinMode(PIN_BTN_A,     INPUT_PULLUP);
  pinMode(PIN_BTN_LEFT,  INPUT_PULLUP);
  pinMode(PIN_BTN_DOWN,  INPUT_PULLUP);
  pinMode(PIN_BTN_UP,    INPUT_PULLUP);
  pinMode(PIN_BTN_RIGHT, INPUT_PULLUP);

  tft.begin();
  delay(100);
  drawHomeScreen();
}

/******************************************************************************/
/*   LOOP                                                                     */
/******************************************************************************/

void ClearKeys() {
  but_A=false;
  but_B=false;
  but_UP=false;
  but_DOWN=false;
  but_LEFT=false;
  but_RIGHT=false;
}

void KeyPadLoop() {
  // Direction buttons: pressing a direction latches it until a new one is pressed.
  // All buttons are active-LOW (INPUT_PULLUP).
  if      (digitalRead(PIN_BTN_UP)    == LOW) { but_UP=true;    but_DOWN=but_LEFT=but_RIGHT=false; }
  else if (digitalRead(PIN_BTN_DOWN)  == LOW) { but_DOWN=true;  but_UP=but_LEFT=but_RIGHT=false;   }
  else if (digitalRead(PIN_BTN_LEFT)  == LOW) { but_LEFT=true;  but_UP=but_DOWN=but_RIGHT=false;   }
  else if (digitalRead(PIN_BTN_RIGHT) == LOW) { but_RIGHT=true; but_UP=but_DOWN=but_LEFT=false;    }

  // A button (pause): edge-triggered so one press = one toggle.
  static bool prevA = HIGH;
  bool currA = (digitalRead(PIN_BTN_A) == LOW);
  if (currA && !prevA) but_A = true;
  prevA = currA;

  // Serial fallback for debug (z=pause, 8/2/4/6=directions, x=restart)
  if (Serial.available()) {
    char r = Serial.read();
    if (r == 'z') { but_A = true; }
    if (r == 'x') { but_B = true; }
    if (r == '8') { but_UP=true;    but_DOWN=but_LEFT=but_RIGHT=false; }
    if (r == '2') { but_DOWN=true;  but_UP=but_LEFT=but_RIGHT=false;   }
    if (r == '4') { but_LEFT=true;  but_UP=but_DOWN=but_RIGHT=false;   }
    if (r == '6') { but_RIGHT=true; but_UP=but_DOWN=but_LEFT=false;    }
  }
}

void loop() {
  KeyPadLoop();

  switch (appState) {

    case StateHome:
      homeScreenLoop();
      break;

    case StatePacman: {
      static uint32_t lastMs = 0;
      uint32_t now = millis();
      if (now - lastMs >= (1000 / FPS)) {
        lastMs = now;
        _game.Step();
      }
      if (GAMEOVER) {
        GAMEOVER      = 0;
        endMenuSel    = 0;
        appState      = StatePacmanEnd;
        but_A = but_UP = but_DOWN = false;  // clear stale button state on entry
        drawPacmanEndMenu();
      }
      break;
    }

    case StatePacmanEnd: {
      static bool prevUp = false, prevDown = false, prevA = false;
      bool currUp   = (digitalRead(PIN_BTN_UP)   == LOW);
      bool currDown = (digitalRead(PIN_BTN_DOWN) == LOW);
      bool currA    = (digitalRead(PIN_BTN_A)    == LOW);

      if ((currUp && !prevUp) || (currDown && !prevDown)) {
        endMenuSel ^= 1;
        drawPacmanEndMenu();
      }
      if (currA && !prevA) {
        if (endMenuSel == 0) {
          but_A    = false;
          appState = StatePacman;
          _game.forceRedraw();
        } else {
          appState = StateHome;
          menuSel  = 0;
          drawHomeScreen();
        }
      }
      prevUp = currUp; prevDown = currDown; prevA = currA;
      break;
    }

    case StateFlappy:
      if (but_A) {
        but_A = false;
        if (flappyGame.isDead())
          flappyGame.init();
        else
          flappyGame.flap();
      }
      flappyGame.step();
      if (but_RIGHT) {
        but_RIGHT = false;
        appState  = StateHome;
        menuSel   = 1;
        drawHomeScreen();
        break;
      }
      delay(30);   // ~33 fps
      break;
  }
}
