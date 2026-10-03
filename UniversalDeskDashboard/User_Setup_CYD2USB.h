// TFT_eSPI configuration for ESP32-2432S028R CYD2USB (micro-USB + USB-C)
// Based on the known-good CYD2USB configuration used by the CYD community.

#define USER_SETUP_INFO "Universal Desk Dashboard - CYD2USB ST7789"

#define ST7789_DRIVER

#define TFT_RGB_ORDER TFT_BGR
#define TFT_INVERSION_OFF

#define TFT_WIDTH  240
#define TFT_HEIGHT 320

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1

#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

#define SPI_FREQUENCY       55000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY 2500000

// Keep display on HSPI. The XPT2046 touch uses the other SPI bus
// on CLK=25, MISO=39, MOSI=32, CS=33, IRQ=36.
#define USE_HSPI_PORT
