/*****************************************************************************
 *                                                                            *
 * DFU/SD/SDHC Bootloader for LPC17xx                                         *
 *                                                                            *
 * by Triffid Hunter                                                          *
 *                                                                            *
 *                                                                            *
 * This firmware is Copyright (C) 2009-2010 Michael Moon aka Triffid_Hunter   *
 *                                                                            *
 * This program is free software; you can redistribute it and/or modify       *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation; either version 2 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * This program is distributed in the hope that it will be useful,            *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with this program; if not, write to the Free Software                *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA *
 *                                                                            *
 *****************************************************************************/

#include "SDCard.h"
#include "gpio.h"
#include "sbl_iap.h"
#include "sbl_config.h"
#include "ff.h"
#include "lpc17xx_wdt.h"
#include "delay.h"
#include "spi_lcd.h"
#include "buzzer.h"
#include "mcp23008.h"
#include <stdio.h>

static FATFS fat;
static FIL file;

static const char *firmware_file = "firmware.bin";
static const char *firmware_old  = "firmware.cur";

#define BLOCK_SIZE (512)

void flash_sd_firmware(void)
{
    uint32_t r = BLOCK_SIZE;
    uint8_t buf[BLOCK_SIZE];
    uint32_t address = USER_FLASH_START;
    FILINFO fi;
    char txt[32];

    // file was previously opened in main()

    if (f_stat(firmware_file, &fi) != FR_OK)
        return;

    while (r == BLOCK_SIZE) {
        if (f_read(&file, buf, sizeof(buf), &r) != FR_OK) {
            f_close(&file);
            return;
        }

        if (address % 10240 == 0) {
            sprintf(txt, "%06d/%06d", address - USER_FLASH_START, (int)fi.fsize);
            lcdSetCursor(1, 4);
            lcdWrite(txt);
        }

        write_flash((void *)address, (char *)buf, BLOCK_SIZE);
        address += r;
    }

    f_close(&file);

    // successful flash, rename file
    if (address > USER_FLASH_START) {
        f_unlink(firmware_old);
        f_rename(firmware_file, firmware_old);
    }
}

#define END_OF_RAM (0x10007FFC)

static int new_execute_user_code(void)
{
    // check blank flash
    if (*(uint32_t *)(USER_FLASH_START) == 0xFFFFFFFF)
        return 1;
    // write a marker at end of RAM for reset vector to jump to userspace
    *(uint32_t *)(END_OF_RAM) = 0xDEADBEEF;
    // Reboot
    NVIC_SystemReset();

    // not reached
    return 0;
}

static void digitalLo(PinName pin)
{
    GPIO_init(pin);
    GPIO_output(pin);
    GPIO_write(pin, 0);
}

static void digitalHi(PinName pin)
{
    GPIO_init(pin);
    GPIO_output(pin);
    GPIO_write(pin, 1);
}

static const uint16_t elise[] = {
    329, 150,
    1, 175,
    311, 150,
    1, 175,
    329, 150,
    1, 175,
    311, 150,
    1, 175,
    329, 150,
    1, 175,
    246, 150,
    1, 200,
    293, 150,
    1, 200,
    261, 150,
    1, 200,
    220, 450,
    1, 500,
    146, 150,
    1, 175,
    174, 150,
    1, 200,
    220, 150,
    1, 200,
    246, 450,
    1, 500,
    174, 150,
    1, 200,
    233, 150,
    1, 200,
    246, 150,
    1, 200,
    261, 450,
    1, 500,
    1, 150,
    329, 150,
    1, 200,
    311, 150,
    1, 200,
    329, 150,
    1, 200,
    311, 150,
    1, 200,
    329, 150,
    1, 200,
    246, 150,
    1, 200,
    293, 150,
    1, 200,
    261, 150,
    1, 200,
    220, 450,
    1, 500,
    146, 150,
    1, 200,
    174, 150,
    1, 200,
    220, 150,
    1, 200,
    246, 450,
    1, 500,
    174, 150,
    1, 200,
    261, 150,
    1, 200,
    246, 150,
    1, 200,
    220, 450,
    1, 500,
    246, 150,
    1, 200,
    261, 150,
    1, 200,
    293, 150,
    1, 200,
    329, 450,
    1, 500,
    196, 150,
    1, 200,
    349, 150,
    1, 200,
    329, 150,
    1, 200,
    293, 450,
    1, 500,
    164, 150,
    1, 200,
    329, 150,
    1, 200,
    293, 150,
    1, 200,
    261, 450,
    1, 500,
    146, 150,
    1, 200,
    293, 150,
    1, 200,
    261, 150,
    1, 200,
    246, 450,
    1, 500,
    1, 200,
    329, 150,
    1, 200,
    311, 150,
    1, 175,
    329, 150,
    1, 175,
    311, 150,
    1, 175,
    329, 150,
    1, 175,
    246, 150,
    1, 200,
    293, 150,
    1, 200,
    261, 150,
    1, 200,
    220, 450,
    1, 500,
    146, 150,
    1, 175,
    174, 150,
    1, 200,
    220, 150,
    1, 200,
    246, 450,
    1, 500,
    174, 150,
    1, 200,
    233, 150,
    1, 200,
    246, 150,
    1, 200,
    261, 450,
    1, 500,
    1, 150,
    329, 150,
    1, 200,
    311, 150,
    1, 200,
    329, 150,
    1, 200,
    311, 150,
    1, 200,
    329, 150,
    1, 200,
    246, 150,
    1, 200,
    293, 150,
    1, 200,
    261, 150,
    1, 200,
    220, 450,
    1, 500,
    146, 150,
    1, 200,
    174, 150,
    1, 200,
    220, 150,
    1, 200,
    246, 450,
    1, 500,
    174, 150,
    1, 200,
    261, 150,
    1, 200,
    246, 150,
    1, 200,
    220, 450,
    1, 2500,
    0, 0,
};

static const uint16_t bootseq[] = { 500, 100, 580, 200, 0, 0 };
static const uint16_t jumpseq[] = { 580, 100, 500, 200, 0, 0 };
static const uint16_t waitseq[] = { 440, 100, 350, 300, 0, 0 };

int main(void)
{
    int rv = 0;
    FRESULT res;

    // initialize things that can burn to sane defaults
    digitalLo(P2_0);
    digitalLo(P2_1);
    digitalLo(P2_2);
    digitalLo(P2_3);
    digitalLo(P2_4);
    digitalHi(P3_26);

    // initialize GPIO expander on V2 hw and set some pins
    mcpInit();

    delayInit();
    
    // delay before talking to SD card
    delayWaitms(500);
    SDCard_init(P0_9, P0_8, P0_7, P0_6);

    // if firmware file not found, directly jump to userspace without initializing other stuff, but if SD card is not inserted, wait around in a loop
    f_mount(0, &fat);
    res = f_open(&file, firmware_file, FA_READ);
    // file not found, try jumping to userspace. rv will be non-zero if userspace is blank
    if (res == FR_NO_FILE)
        rv = new_execute_user_code();

    // or else, show stuff on-screen
    buzzerInit();
    buzzerPlay(bootseq);
    lcdInit();

    if (rv) {
        // flash was empty (no firmware AND no flash)
        lcdSetCursor(1, 0);
        lcdWrite("NO FIRMWARE.BIN");
        lcdSetCursor(1, 1);
        lcdWrite("EMPTY FLASH, TOO");
        lcdSetCursor(1, 2);
        lcdWrite("NOWHERE TO GO :(");
        lcdSetCursor(1, 3);
        lcdWrite("CHECK SDCARD, RETRY");

        while(1)
            buzzerPlay(elise);
    }
    
    if (res == FR_NOT_READY) {
        lcdSetCursor(1, 0);
        lcdWrite("SDCard Not Inserted");
        lcdSetCursor(1, 1);
        lcdWrite("~ Waiting for card ~");
    }

    while (res == FR_NOT_READY) {
        f_close(&file);
        buzzerPlay(waitseq);
        delayWaitms(1000);
        res = f_open(&file, firmware_file, FA_READ);
    }

    lcdClear();

    // reboot if SD was inserted, but without firmware file
    if (res != FR_OK)
        NVIC_SystemReset();

    lcdSetCursor(1, 0);
    lcdWrite("BOOTLOADER MODE");

    lcdSetCursor(1, 1);
    lcdWrite("LOAD FIRMWARE.BIN");

    flash_sd_firmware();

    // clear flash counter
    lcdSetCursor(1, 4);
    lcdWrite("              ");

    lcdSetCursor(1, 2);
    lcdWrite("BOOTING");

    buzzerPlay(jumpseq);
    
    // reset to boot
    NVIC_SystemReset();
}

DWORD get_fattime(void)
{
#define YEAR    2016
#define MONTH   1
#define DAY     16
#define HOUR    4
#define MINUTE  20
#define SECOND  0

    return  ((uint32_t)(YEAR  & 127) << 25) |
            ((MONTH &  15) << 21) |
            ((DAY   &  31) << 16) |
            ((HOUR  &  31) << 11) |
            ((MINUTE & 63) <<  5) |
            ((SECOND & 63) <<  0);
}
