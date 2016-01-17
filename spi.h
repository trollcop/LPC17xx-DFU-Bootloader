#ifndef _SPI_H
#define _SPI_H

#include <stdint.h>

#include "spi_hal.h"

#include "pins.h"


typedef struct spi_private_t {
    Pin_t miso;
    Pin_t mosi;
    Pin_t sclk;
    SPI_REG *sspr;
} spi_private_t;

void SPI_init(spi_private_t *priv, PinName mosi, PinName miso, PinName sclk);

void SPI_frequency(spi_private_t *priv, uint32_t speed);
uint8_t SPI_write(spi_private_t *priv, uint8_t data);

int SPI_writeblock(spi_private_t *priv, uint8_t *buf, int len);

typedef void (*fptr)(void);
extern fptr isr_dispatch[N_SPI_INTERRUPT_ROUTINES];

#endif /* _SPI_H */
