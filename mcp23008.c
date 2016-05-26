/* quick and dirty driver for mcp23008 */

#include "LPC17xx.h"
#include "lpc17xx_i2c.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "mcp23008.h"

#define I2C_DEVICE (LPC_I2C1)

#define MCP23008_IODIR 0x00
#define MCP23008_IPOL 0x01
#define MCP23008_GPINTEN 0x02
#define MCP23008_DEFVAL 0x03
#define MCP23008_INTCON 0x04
#define MCP23008_IOCON 0x05
#define MCP23008_GPPU 0x06
#define MCP23008_INTF 0x07
#define MCP23008_INTCAP 0x08
#define MCP23008_GPIO 0x09
#define MCP23008_OLAT 0x0A

void mcpInit(void)
{
    // I2C1 P0.0 = SDA1 P0.1 = SCL1
    LPC_PINCON->PINSEL0 |= (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3);
    LPC_PINCON->PINMODE0 |=(1 << 1) | (1 << 3);
    LPC_PINCON->PINMODE_OD0 |= (1 << 0) | (1 << 1);

    // Initialize I2C peripheral
    I2C_Init(I2C_DEVICE, 200000);
    I2C_Cmd(I2C_DEVICE, ENABLE);

    mcpConfigure();
    I2C_DeInit(I2C_DEVICE);
}

void mcpConfigure(void)
{
    int addr = 0x40;
    uint8_t txbuf[] = { MCP23008_IODIR, 0xFF, 0x11, 0x22, 0x33, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00 };
    int i;
    I2C_M_SETUP_Type xf = { 0, };
    
    xf.sl_addr7bit = addr;
    xf.tx_data = txbuf;
    xf.tx_length = sizeof(txbuf);
    xf.rx_data = 0;
    xf.rx_length = 0;
    xf.retransmissions_max = 1;
    
    for (i = 0; i < 5; i++) {
        I2C_MasterTransferData(I2C_DEVICE, &xf, I2C_TRANSFER_POLLING);
        addr++;
        xf.sl_addr7bit = addr;
    }        
}
