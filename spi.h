#ifndef _SPI_H
#define _SPI_H

#include <stdint.h>
#include "lpc17xx_ssp.h"
#include "pins.h"

typedef struct spi_private_t {
    LPC_SSP_TypeDef *sspr;
} spi_private_t;

void SPI_init(spi_private_t *priv, PinName mosi, PinName miso, PinName sclk);

void SPI_frequency(spi_private_t *priv, uint32_t speed);
uint8_t SPI_write(spi_private_t *priv, uint8_t data);

#endif /* _SPI_H */
