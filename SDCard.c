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

#include <stdio.h>
#include <stdlib.h>

#include "SDCard.h"
#include "gpio.h"

int SDCard__cmd(int cmd, int arg);
int SDCard__cmdx(int cmd, int arg);
int SDCard__cmd8(void);
int SDCard__cmd58(uint32_t *);
int SDCard_initialise_card(void);
int SDCard_initialise_card_v1(void);
int SDCard_initialise_card_v2(void);

int SDCard__read(uint8_t *buffer, int length);
int SDCard__write(const uint8_t *buffer, int length);

// int start_multi_write(uint32_t start_block, uint32_t n_blocks);
// int validate_buffer(uint8_t *, int);
// int end_multi_write(void);

// int start_multi_read(uint32_t start_block, uint32_t n_blocks);
// int validate_buffer(uint8_t *, int);
// int check_buffer(uint8_t *, int);
// int end_multi_read(void);

uint32_t SDCard__sd_sectors(void);
static uint32_t _sectors;
static PinName _cs;
static spi_private_t spi;
static int cardtype;

#define SD_COMMAND_TIMEOUT 4096

void SDCard_init(PinName mosi, PinName miso, PinName sclk, PinName cs)
{
    SPI_init(&spi, mosi, miso, sclk);
    GPIO_init(cs);
    GPIO_output(cs);
    GPIO_set(cs);
    _cs = cs;
}

#define R1_IDLE_STATE           (1 << 0)
#define R1_ERASE_RESET          (1 << 1)
#define R1_ILLEGAL_COMMAND      (1 << 2)
#define R1_COM_CRC_ERROR        (1 << 3)
#define R1_ERASE_SEQUENCE_ERROR (1 << 4)
#define R1_ADDRESS_ERROR        (1 << 5)
#define R1_PARAMETER_ERROR      (1 << 6)

// Types
//  - v1.x Standard Capacity
//  - v2.x Standard Capacity
//  - v2.x High Capacity
//  - Not recognised as an SD Card

#define SDCARD_FAIL 0
#define SDCARD_V1   1
#define SDCARD_V2   2
#define SDCARD_V2HC 3

#define BUSY_FLAG_MULTIREAD          1
#define BUSY_FLAG_MULTIWRITE         2
#define BUSY_FLAG_ENDREAD            4
#define BUSY_FLAG_ENDWRITE           8
#define BUSY_FLAG_WAITNOTBUSY       (1<<31)

#define SDCMD_GO_IDLE_STATE          0
#define SDCMD_ALL_SEND_CID           2
#define SDCMD_SEND_RELATIVE_ADDR     3
#define SDCMD_SET_DSR                4
#define SDCMD_SELECT_CARD            7
#define SDCMD_SEND_IF_COND           8
#define SDCMD_SEND_CSD               9
#define SDCMD_SEND_CID              10
#define SDCMD_STOP_TRANSMISSION     12
#define SDCMD_SEND_STATUS           13
#define SDCMD_GO_INACTIVE_STATE     15
#define SDCMD_SET_BLOCKLEN          16
#define SDCMD_READ_SINGLE_BLOCK     17
#define SDCMD_READ_MULTIPLE_BLOCK   18
#define SDCMD_WRITE_BLOCK           24
#define SDCMD_WRITE_MULTIPLE_BLOCK  25
#define SDCMD_PROGRAM_CSD           27
#define SDCMD_SET_WRITE_PROT        28
#define SDCMD_CLR_WRITE_PROT        29
#define SDCMD_SEND_WRITE_PROT       30
#define SDCMD_ERASE_WR_BLOCK_START  32
#define SDCMD_ERASE_WR_BLK_END      33
#define SDCMD_ERASE                 38
#define SDCMD_LOCK_UNLOCK           42
#define SDCMD_APP_CMD               55
#define SDCMD_GEN_CMD               56

#define SD_ACMD_SET_BUS_WIDTH            6
#define SD_ACMD_SD_STATUS               13
#define SD_ACMD_SEND_NUM_WR_BLOCKS      22
#define SD_ACMD_SET_WR_BLK_ERASE_COUNT  23
#define SD_ACMD_SD_SEND_OP_COND         41
#define SD_ACMD_SET_CLR_CARD_DETECT     42
#define SD_ACMD_SEND_CSR                51

#define BLOCK2ADDR(block)   (((cardtype == SDCARD_V1) || (cardtype == SDCARD_V2))?(block << 9):((cardtype == SDCARD_V2HC)?(block):0))

#define fprintf(...) do {} while (0)

int SDCard_initialise_card(void)
{
    int i, r;

    // Set to 25kHz for initialisation, and clock card with cs = 1
    SPI_frequency(&spi, 25000);
    GPIO_set(_cs);

    for(i = 0; i < 16; i++)
        SPI_write(&spi, 0xFF);

    // send CMD0, should return with all zeros except IDLE STATE set (bit 0)
    if (SDCard__cmd(SDCMD_GO_IDLE_STATE, 0) != R1_IDLE_STATE) {
        fprintf(stderr, "Could not put SD card in to SPI idle state\n");
        return cardtype = SDCARD_FAIL;
    }

    // send CMD8 to determine whther it is ver 2.x
    r = SDCard__cmd8();
    if (r == R1_IDLE_STATE) {
        return SDCard_initialise_card_v2();
    } else if (r == (R1_IDLE_STATE | R1_ILLEGAL_COMMAND)) {
        return SDCard_initialise_card_v1();
    } else {
        fprintf(stderr, "Not in idle state after sending CMD8 (not an SD card?)\n");
        return cardtype = SDCARD_FAIL;
    }
}

int SDCard_initialise_card_v1(void)
{
    int i;

    for (i = 0; i < SD_COMMAND_TIMEOUT; i++) {
        SDCard__cmd(SDCMD_APP_CMD, 0);
        if (SDCard__cmd(SD_ACMD_SD_SEND_OP_COND, 0) == 0)
            return cardtype = SDCARD_V1;
    }

    fprintf(stderr, "Timeout waiting for v1.x card\n");
    return SDCARD_FAIL;
}

int SDCard_initialise_card_v2(void)
{
    int i;
    uint32_t ocr;

    for (i = 0; i < SD_COMMAND_TIMEOUT; i++) {
        SDCard__cmd(SDCMD_APP_CMD, 0);
        if (SDCard__cmd(SD_ACMD_SD_SEND_OP_COND, (1UL << 30)) == 0) {
            SDCard__cmd58(&ocr);
            if (ocr & (1UL << 30))
                return cardtype = SDCARD_V2HC;
            else
                return cardtype = SDCARD_V2;
        }
    }

    fprintf(stderr, "Timeout waiting for v2.x card\n");
    return cardtype = SDCARD_FAIL;
}

int SDCard_disk_initialize(void)
{
    _sectors = 0;

    if (SDCard_initialise_card() == 0)
        return 1;

    _sectors = SDCard__sd_sectors();

    // Set block length to 512 (CMD16)
    if (SDCard__cmd(SDCMD_SET_BLOCKLEN, 512) != 0) {
        fprintf(stderr, "Set 512-byte block timed out\n");
        return 1;
    }

    SPI_frequency(&spi, 1000000); // Set to 1MHz for data transfer
    return 0;
}

int SDCard_disk_write(const uint8_t *buffer, uint32_t block_number)
{
    // set write address for single block (CMD24)
    if (SDCard__cmd(SDCMD_WRITE_BLOCK, BLOCK2ADDR(block_number)) != 0)
        return 1;

    // send the data block
    SDCard__write(buffer, 512);
    return 0;
}

int SDCard_disk_read(uint8_t *buffer, uint32_t block_number)
{
    // set read address for single block (CMD17)
    if (SDCard__cmd(SDCMD_READ_SINGLE_BLOCK, BLOCK2ADDR(block_number)) != 0)
        return 1;

    // receive the data
    SDCard__read(buffer, 512);
    return 0;
}

int SDCard_disk_erase(uint32_t block_number, int count)
{
    return -1;
}

int SDCard_disk_status(void)
{ 
    return (_sectors > 0) ? 0 : 1;
}

int SDCard_disk_sync(void)
{
    // TODO: wait for DMA, wait for card not busy
    return 0;
}

uint32_t SDCard_disk_sectors() { return _sectors; }
uint64_t SDCard_disk_size() { return ((uint64_t) _sectors) << 9; }
uint32_t SDCard_disk_blocksize() { return (1<<9); }

// PRIVATE FUNCTIONS

static void SDCard_SPI_write(uint8_t cmd, uint32_t arg)
{
    SPI_write(&spi, 0x40 | cmd);
    SPI_write(&spi, arg >> 24);
    SPI_write(&spi, arg >> 16);
    SPI_write(&spi, arg >> 8);
    SPI_write(&spi, arg >> 0);
    SPI_write(&spi, 0x95);
}

int SDCard__cmd(int cmd, int arg)
{
    int i;

    GPIO_clear(_cs);

    // send a command
    SDCard_SPI_write(cmd, arg);

    // wait for the repsonse (response[7] == 0)
    for (i = 0; i < SD_COMMAND_TIMEOUT; i++) {
        int response = SPI_write(&spi, 0xFF);
        if (!(response & 0x80)) {
            GPIO_set(_cs);
            SPI_write(&spi, 0xFF);

            return response;
        }
    }

    GPIO_set(_cs);
    SPI_write(&spi, 0xFF);
    return -1; // timeout
}

int SDCard__cmdx(int cmd, int arg)
{
    int i;

    GPIO_clear(_cs);

    // send a command
    SDCard_SPI_write(cmd, arg);

    // wait for the repsonse (response[7] == 0)
    for (i = 0; i < SD_COMMAND_TIMEOUT; i++) {
        int response = SPI_write(&spi, 0xFF);
        if (!(response & 0x80))
            return response;
    }
    
    GPIO_set(_cs);
    SPI_write(&spi, 0xFF);
    return -1; // timeout
}

int SDCard__cmd58(uint32_t *ocr)
{
    int i;

    GPIO_clear(_cs);

    // send a command
    SDCard_SPI_write(58, 0);

    // wait for the repsonse (response[7] == 0)
    for(i = 0; i < SD_COMMAND_TIMEOUT; i++) {
        int response = SPI_write(&spi, 0xFF);
        if (!(response & 0x80)) {
            *ocr = SPI_write(&spi, 0xFF) << 24;
            *ocr |= SPI_write(&spi, 0xFF) << 16;
            *ocr |= SPI_write(&spi, 0xFF) << 8;
            *ocr |= SPI_write(&spi, 0xFF) << 0;
            GPIO_set(_cs);
            SPI_write(&spi, 0xFF);
            return response;
        }
    }

    GPIO_set(_cs);
    SPI_write(&spi, 0xFF);
    return -1; // timeout
}

int SDCard__cmd8(void)
{
    int i, j;

    GPIO_clear(_cs);

    // send a command
    SPI_write(&spi, 0x40 | SDCMD_SEND_IF_COND); // CMD8
    SPI_write(&spi, 0x00);     // reserved
    SPI_write(&spi, 0x00);     // reserved
    SPI_write(&spi, 0x01);     // 3.3v
    SPI_write(&spi, 0xAA);     // check pattern
    SPI_write(&spi, 0x87);     // crc

    // wait for the repsonse (response[7] == 0)
    for (i = 0; i < SD_COMMAND_TIMEOUT * 1000; i++) {
        uint8_t response[5];
        response[0] = SPI_write(&spi, 0xFF);
        if (!(response[0] & 0x80)) {
                for (j = 1; j < 5; j++)
                    response[j] = SPI_write(&spi, 0xFF);
                GPIO_set(_cs);
                SPI_write(&spi, 0xFF);
                return response[0];
        }
    }
    GPIO_set(_cs);
    SPI_write(&spi, 0xFF);
    return -1; // timeout
}

int SDCard__read(uint8_t *buffer, int length)
{
    int i;

    GPIO_clear(_cs);

    // read until start byte (0xFF)
    while (SPI_write(&spi, 0xFF) != 0xFE);

    // read data
    for (i = 0; i < length; i++)
        buffer[i] = SPI_write(&spi, 0xFF);
    SPI_write(&spi, 0xFF); // checksum
    SPI_write(&spi, 0xFF);

    GPIO_set(_cs);
    SPI_write(&spi, 0xFF);
    return 0;
}

int SDCard__write(const uint8_t *buffer, int length)
{
    int i;

    GPIO_clear(_cs);

    // indicate start of block
    SPI_write(&spi, 0xFE);

    // write the data
    for (i = 0; i < length; i++)
        SPI_write(&spi, buffer[i]);

    // write the checksum
    SPI_write(&spi, 0xFF);
    SPI_write(&spi, 0xFF);

    // check the repsonse token
    if ((SPI_write(&spi, 0xFF) & 0x1F) != 0x05) {
        GPIO_set(_cs);
        SPI_write(&spi, 0xFF);
        return 1;
    }

    // wait for write to finish
    while (SPI_write(&spi, 0xFF) == 0);

    GPIO_set(_cs);
    SPI_write(&spi, 0xFF);
    return 0;
}

static int ext_bits(uint8_t *data, int msb, int lsb)
{
    int bits = 0;
    int size = 1 + msb - lsb;
    for(int i=0; i<size; i++) {
        int position = lsb + i;
        int byte = 15 - (position >> 3);
        int bit = position & 0x7;
        int value = (data[byte] >> bit) & 1;
        bits |= value << i;
    }
    return bits;
}

uint32_t SDCard__sd_sectors(void)
{
    // CMD9, Response R2 (R1 byte + 16-byte block read)
    if (SDCard__cmdx(SDCMD_SEND_CSD, 0) != 0) {
        fprintf(stderr, "Didn't get a response from the disk\n");
        return 0;
    }

    uint8_t csd[16];
    if (SDCard__read(csd, 16) != 0) {
        fprintf(stderr, "Couldn't read csd response from disk\n");
        return 0;
    }

    // csd_structure : csd[127:126]
    // c_size        : csd[73:62]
    // c_size_mult   : csd[49:47]
    // read_bl_len   : csd[83:80] - the *maximum* read block length

    uint32_t csd_structure = ext_bits(csd, 127, 126);

//    printf("CSD_STRUCT = %d\n", csd_structure);

    if (csd_structure == 0)
	{
		if (cardtype == SDCARD_V2HC)
		{
// 			fprintf(stderr, "SDHC card with regular SD descriptor!\n");
			return 0;
		}
		uint32_t c_size = ext_bits(csd, 73, 62);
		uint32_t c_size_mult = ext_bits(csd, 49, 47);
		uint32_t read_bl_len = ext_bits(csd, 83, 80);

		uint32_t block_len = 1 << read_bl_len;
		uint32_t mult = 1 << (c_size_mult + 2);
		uint32_t blocknr = (c_size + 1) * mult;

		if (block_len >= 512) return blocknr * (block_len >> 9);
		else return (blocknr * block_len) >> 9;
	}
	else if (csd_structure == 1)
	{
		if (cardtype != SDCARD_V2HC)
		{
// 			fprintf(stderr, "SD V1 or V2 card with SDHC descriptor!\n");
			return 0;
		}
		uint32_t c_size = ext_bits(csd, 69, 48);
		uint32_t blocknr = (c_size + 1) * 1024;

		return blocknr;
	}
	else
	{
		fprintf(stderr, "Invalid CSD %lu\n", csd_structure);
        return 0;
    }

    // memory capacity = BLOCKNR * BLOCK_LEN
    // where
    //  BLOCKNR = (C_SIZE+1) * MULT
    //  MULT = 2^(C_SIZE_MULT+2) (C_SIZE_MULT < 8)
    //  BLOCK_LEN = 2^READ_BL_LEN, (READ_BL_LEN < 12)

//     uint32_t block_len = 1 << read_bl_len;
//     uint32_t mult = 1 << (c_size_mult + 2);
//     uint32_t blocknr = (c_size + 1) * mult;

//     uint32_t capacity = blocknr * block_len;

//     uint32_t blocks = capacity / 512;
//     uint32_t blocks;

//     if (block_len >= 512) return blocknr * (block_len >> 9);
//     else return (blocknr * block_len) >> 9;

//     return blocks;
}
