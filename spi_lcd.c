#include "gpio.h"
#include "spi.h"
#include "spi_lcd.h"
#include "glcdfont.h"
#include <string.h>

static spi_private_t spi;
static PinName cs = P0_16;
static PinName bg_ctl = P0_22;
static PinName a0 = P1_26;
static PinName rst = P1_27;

static uint8_t reversed = 0;
static uint8_t contrast = 9;

#define LCDWIDTH    (128)
#define LCDHEIGHT   (64)
#define LCDPAGES    ((LCDHEIGHT + 7) / 8)
#define FB_SIZE     (LCDWIDTH * LCDPAGES)

#define CLAMP(x, low, high) { if ( (x) < (low) ) x = (low); if ( (x) > (high) ) x = (high); } while (0);
#define swap(a, b) { uint8_t t = a; a = b; b = t; }

static uint8_t tx;
static uint8_t ty;
static uint8_t framebuffer[FB_SIZE];

static void send_commands(const uint8_t *buf, int size)
{
    GPIO_clear(cs);
    GPIO_clear(a0);
    while (size-- > 0)
        SPI_write(&spi, *buf++);
    GPIO_set(cs);
}

static void send_data(const uint8_t *buf, int size)
{
    GPIO_clear(cs);
    GPIO_set(a0);
    while (size-- > 0)
        SPI_write(&spi, *buf++);
    GPIO_set(cs);
    GPIO_clear(a0);
}

static void clear(void)
{
    tx = ty = 0;
    memset(framebuffer, 0, FB_SIZE);
}

static void set_xy(int x, int y)
{
    unsigned char cmd[3];

    CLAMP(x, 0, LCDWIDTH - 1);
    CLAMP(y, 0, LCDPAGES - 1);
    cmd[0] = 0xb0 | (y & 0x07);
    cmd[1] = 0x10 | (x >> 4);
    cmd[2] = 0x00 | (x & 0x0f);
    send_commands(cmd, 3);
}

static void setCursor(uint8_t col, uint8_t row)
{
    tx = col * 6;
    ty = row * 8;
}

static void home(void)
{
    tx = 0;
    ty = 0;
}

static void init(void)
{
    const uint8_t init_seq[] = {
        0x40,    //Display start line 0
        (uint8_t)(reversed ? 0xa0 : 0xa1), // ADC
        (uint8_t)(reversed ? 0xc8 : 0xc0), // COM select
        0xa6,    //Display normal
        0xa2,    //Set Bias 1/9 (Duty 1/65)
        0x2f,    //Booster, Regulator and Follower On
        0xf8,    //Set internal Booster to 4x
        0x00,
        0x27,    //Contrast set
        0x81,
        contrast,    //contrast value
        0xac,    //No indicator
        0x00,
        0xaf,    //Display on
    };
    
    GPIO_set(rst);
    send_commands(init_seq, sizeof(init_seq));
    clear();
}

static void send_pic(const uint8_t *data)
{
    int i;

    for (i = 0; i < LCDPAGES; i++) {
        set_xy(0, i);
        send_data(data + i * LCDWIDTH, LCDWIDTH);
    }
}

static int drawChar(int x, int y, unsigned char c, int color)
{
    int retVal = -1;
    int i;

    if(c == '\n') {
        ty += 8;
        retVal = -tx;
    }
    if (c == '\r') {
        retVal = -tx;
    } else {
        for (i = 0; i < 5; i++ ) {
            if (color == 0) {
                framebuffer[x + (y / 8 * 128) ] = ~(glcd_font[(c * 5) + i] << y % 8);
                if (y + 8 < 63) {
                    framebuffer[x + ((y + 8) / 8 * 128) ] = ~(glcd_font[(c * 5) + i] >> (8 - (y % 8)));
                }
            }
            if (color == 1) {
                framebuffer[x + ((y) / 8 * 128) ] = glcd_font[(c * 5) + i] << (y % 8);
                if (y + 8 < 63) {
                    framebuffer[x + ((y + 8) / 8 * 128) ] = glcd_font[(c * 5) + i] >> (8 - (y % 8));
                }
            }
            x++;
        }
        retVal = 6;
        tx += 6;
    }

    return retVal;
}

static void write_char(char value)
{
    drawChar(tx, ty, value, 1);
}

static void write(const char *line)
{
    int i, len;

    len = strlen(line);
    for (i = 0; i < len; ++i)
        write_char(line[i]);
}





void lcdInit(void)
{
    SPI_init(&spi, P0_18, P0_17, P0_15);

    GPIO_init(cs);
    GPIO_output(cs);
    GPIO_set(cs);
    
    GPIO_init(bg_ctl);
    GPIO_output(bg_ctl);
    GPIO_set(bg_ctl);

    GPIO_init(a0);
    GPIO_output(a0);
    GPIO_set(a0);

    GPIO_init(rst);
    GPIO_output(rst);
    GPIO_set(rst);

    // Write junk to LCD
    init();
    clear();
    setCursor(1, 1);
    write("BOOTLOADER LOL");
    send_pic(framebuffer);
}
