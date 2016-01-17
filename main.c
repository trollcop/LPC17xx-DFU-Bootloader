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

// this seems to fix an issue with handoff after poweroff
// found here http://knowledgebase.nxp.trimm.net/showthread.php?t=2869
typedef void __attribute__((noreturn))(*exec)();

static void boot(void)
{
    uint32_t *start;

    __disable_irq();
    SCB->VTOR = (USER_FLASH_START & 0x1FFFFF80);
    __set_MSP(*(uint32_t *)USER_FLASH_START);
    start = (uint32_t *)(USER_FLASH_START + 4);
    ((exec)(*start))();
}

static int new_execute_user_code(void)
{
    // check blank flash
    if (*(uint32_t *)(USER_FLASH_START) == 0xFFFFFFFF)
        return 1;

    // relocate vector table
    SCB->VTOR = (USER_FLASH_START & 0x1FFFFF80);
    // switch to RC generator
    LPC_SC->PLL0CON = 0x1; // disconnect PLL0
    LPC_SC->PLL0FEED = 0xAA;
    LPC_SC->PLL0FEED = 0x55;
    while (LPC_SC->PLL0STAT&(1<<25));
    LPC_SC->PLL0CON = 0x0;    // power down
    LPC_SC->PLL0FEED = 0xAA;
    LPC_SC->PLL0FEED = 0x55;
    while (LPC_SC->PLL0STAT&(1<<24));
    // disable PLL1
    LPC_SC->PLL1CON   = 0;
    LPC_SC->PLL1FEED  = 0xAA;
    LPC_SC->PLL1FEED  = 0x55;
    while (LPC_SC->PLL1STAT&(1<<9));

    LPC_SC->FLASHCFG &= 0x0fff;  // This is the default flash read/write setting for IRC
    LPC_SC->FLASHCFG |= 0x5000;
    LPC_SC->CCLKCFG = 0x0;     //  Select the IRC as clk
    LPC_SC->CLKSRCSEL = 0x00;
    LPC_SC->SCS = 0x00;		    // not using XTAL anymore

    // reset pipeline, sync bus and memory access
	__asm (
		   "dmb\n"
		   "dsb\n"
		   "isb\n"
		  );
	

    boot();
    
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

int main(void)
{
    int rv = 0;
    static uint16_t bootseq[] = { 500, 100, 580, 200, 0, 0 };
    static uint16_t jumpseq[] = { 580, 100, 500, 200, 0, 0 };

    WDT_Feed();

    // initialize things that can burn to sane defaults
    digitalLo(P2_0);
    digitalLo(P2_1);
    digitalLo(P2_2);
    digitalLo(P2_3);
    digitalLo(P2_4);
    digitalHi(P3_26);

    SDCard_init(P0_9, P0_8, P0_7, P0_6);

    // if firmware file not found, directly jump to userspace without initializing other stuff
    f_mount(0, &fat);
    if (f_open(&file, firmware_file, FA_READ) != FR_OK)
        rv = new_execute_user_code();

    // or else, show stuff on-screen
    delayInit();
    buzzerInit();
    buzzerPlay(bootseq);
    lcdInit();
    
    if (rv) {
        // flash was empty (no firmware AND no flash)
        lcdSetCursor(1, 0);
        lcdWrite("NO BOOTLOADER.BIN");

        while(1);
    }

    lcdSetCursor(1, 0);
    lcdWrite("BOOTLOADER MODE");

    // give SD card time to wake up
    delayWaitms(400);

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
