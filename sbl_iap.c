//-----------------------------------------------------------------------------
// Software that is described herein is for illustrative purposes only
// which provides customers with programming information regarding the
// products. This software is supplied "AS IS" without any warranties.
// NXP Semiconductors assumes no responsibility or liability for the
// use of the software, conveys no license or title under any patent,
// copyright, or mask work right to the product. NXP Semiconductors
// reserves the right to make changes in the software without
// notification. NXP Semiconductors also make no representation or
// warranty that such application will be suitable for the specified
// use without further testing or modification.
//-----------------------------------------------------------------------------

/***********************************************************************
 * Code Red Technologies - Minor modifications to original NXP AN10866
 * example code for use in RDB1768 secondary USB bootloader based on
 * LPCUSB USB stack.
 * *********************************************************************/

#include "sbl_iap.h"
#include "sbl_config.h"
#include "LPC17xx.h"

unsigned param_table[5];
unsigned result_table[5];

char flash_buf[FLASH_BUF_SIZE];

unsigned *flash_address = 0;
unsigned byte_ctr = 0;

void write_data(unsigned cclk,unsigned flash_address,unsigned * flash_data_buf, unsigned count);
void find_erase_prepare_sector(unsigned cclk, unsigned flash_address);
void erase_sector(unsigned start_sector,unsigned end_sector,unsigned cclk);
void prepare_sector(unsigned start_sector,unsigned end_sector,unsigned cclk);
void iap_entry(unsigned param_tab[],unsigned result_tab[]);

unsigned write_flash(unsigned *dst, char *src, unsigned no_of_bytes)
{
    unsigned i;

    /* Store flash start address */
    if (flash_address == 0)
        flash_address = (unsigned *)dst;

    for (i = 0; i < no_of_bytes; i++)
        flash_buf[(byte_ctr + i)] = *(src + i);

    byte_ctr = byte_ctr + no_of_bytes;

    if (byte_ctr == FLASH_BUF_SIZE) {
        /* We have accumulated enough bytes to trigger a flash write */
        find_erase_prepare_sector(SystemCoreClock / 1000, (unsigned)flash_address);
        if (result_table[0] != CMD_SUCCESS)
            return result_table[0];

        write_data(SystemCoreClock / 1000, (unsigned)flash_address, (unsigned *)flash_buf, FLASH_BUF_SIZE);
        if (result_table[0] != CMD_SUCCESS)
            return result_table[0];

        /* Reset byte counter and flash address */
        byte_ctr = 0;
        flash_address = 0;
    }
    return CMD_SUCCESS;
}

void find_erase_prepare_sector(unsigned cclk, unsigned flash_address)
{
    unsigned i;

    __disable_irq();
    for(i = USER_START_SECTOR; i <= MAX_USER_SECTOR; i++) {
        if (flash_address < SECTOR_END(i)) {
            if (flash_address == SECTOR_START(i)) {
                prepare_sector(i, i, cclk);
                erase_sector(i, i, cclk);
            }
            prepare_sector(i, i, cclk);
            break;
        }
    }
    __enable_irq();
}

void write_data(unsigned cclk, unsigned flash_address, unsigned *flash_data_buf, unsigned count)
{
    __disable_irq();
    param_table[0] = COPY_RAM_TO_FLASH;
    param_table[1] = flash_address;
    param_table[2] = (unsigned)flash_data_buf;
    param_table[3] = count;
    param_table[4] = cclk;
    iap_entry(param_table, result_table);
    __enable_irq();
}

void erase_sector(unsigned start_sector, unsigned end_sector, unsigned cclk)
{
    param_table[0] = ERASE_SECTOR;
    param_table[1] = start_sector;
    param_table[2] = end_sector;
    param_table[3] = cclk;
    iap_entry(param_table, result_table);
}

void prepare_sector(unsigned start_sector, unsigned end_sector, unsigned cclk)
{
    param_table[0] = PREPARE_SECTOR_FOR_WRITE;
    param_table[1] = start_sector;
    param_table[2] = end_sector;
    param_table[3] = cclk;
    iap_entry(param_table, result_table);
}

void iap_entry(unsigned param_tab[], unsigned result_tab[])
{
    void (*iap)(unsigned [],unsigned []);

    iap = (void (*)(unsigned [], unsigned []))IAP_ADDRESS;
    iap(param_tab, result_tab);
}
