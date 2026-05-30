#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

#define TFT_SCK   18
#define TFT_MOSI  23
#define TFT_CS    14
#define TFT_DC    16
#define TFT_RST   17

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Starting simplest TFT test...");

  SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);

  // Your 1.9 inch display is probably 170x320 ST7789
  tft.init(170, 320);

  tft.setRotation(1);
  tft.invertDisplay(true);

  Serial.println("Filling red");
  tft.fillScreen(ST77XX_RED);
  delay(1000);

  Serial.println("Filling green");
  tft.fillScreen(ST77XX_GREEN);
  delay(1000);

  Serial.println("Filling blue");
  tft.fillScreen(ST77XX_BLUE);
  delay(1000);

  Serial.println("Filling black");
  tft.fillScreen(ST77XX_BLACK);
  delay(500);

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(3);
  tft.setCursor(30, 40);
  tft.println("TFT");

  tft.setCursor(30, 90);
  tft.println("TEST");

  Serial.println("Done.");
}

void loop() {
}