// TFT_eSPI User Setup
// This file configures TFT_eSPI for the 1.9" ST7789 display
// When USER_SETUP_LOADED is defined, TFT_eSPI will use this file

// Display driver
#define ST7789_DRIVER

// Display dimensions
#define TFT_WIDTH  170
#define TFT_HEIGHT 320

// Display type (ST7789 variant)
#define TFT_RST            4   // Reset pin
#define TFT_DC             2   // Data/Command pin
#define TFT_CS             15   // Chip Select pin
#define TFT_MOSI           23   // SPI MOSI
#define TFT_SCLK           18   // SPI Clock
#define TFT_MISO           -1   // MISO not used

// Backlight
#define TFT_BL            -1   // No backlight control
#define TFT_BACKLIGHT_ON HIGH

// Rotation
#define TFT_ROTATION     0

// Load standard fonts
#define LOAD_GLCD
#define FONT_2
#define FONT_4
#define FONT_6
#define FONT_7
#define FONT_8

// SPI frequency
#define SPI_FREQUENCY   40000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000

// Enable 16-bit colour
#define TFT_RGB_ORDER TFT_RGB  // Colour order: RGB

// Custom colour definitions
#define COLOUR_BLUE 0x031F
#define COLOUR_DARK_BLUE 0x0300
#define COLOUR_GREEN 0x07E0
#define COLOUR_RED 0xF800
#define COLOUR_YELLOW 0xFFE0
#define COLOUR_CYAN 0x07FF
#define COLOUR_MAGENTA 0xF81F
#define COLOUR_WHITE 0xFFFF
#define COLOUR_BLACK 0x0000
#define COLOUR_GREY 0x632C
#define COLOUR_LIGHT_GREY 0xC622
