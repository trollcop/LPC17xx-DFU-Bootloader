#include <stdint.h>
#include "LPC17xx.h"
#include "delay.h"

/* these should be defined by CMSIS, but they aren't */
#define DWT_CTRL    (*(volatile uint32_t *)0xe0001000)
#define CYCCNTENA   (1 << 0)
#define DWT_CYCCNT  (*(volatile uint32_t *)0xe0001004)

/* cycles per microsecond */
static uint32_t us_ticks;
static volatile uint32_t ticks = 0;

void delayInit(void)
{
    // update SystemCoreClock uint32
    SystemCoreClockUpdate();

    /* compute the number of system clocks per microsecond */
    us_ticks = SystemCoreClock / 1000000;

    /* turn on access to the DWT registers */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* enable the CPU cycle counter */
    DWT_CTRL |= CYCCNTENA;
}

void delayWaitus(uint32_t uS)
{
    uint32_t elapsed = 0;
    uint32_t last_count = DWT_CYCCNT;

    for (;;) {
        uint32_t current_count = DWT_CYCCNT;
        uint32_t elapsed_uS;

        /* measure the time elapsed since the last time we checked */
        elapsed += current_count - last_count;
        last_count = current_count;

        /* convert to microseconds */
        elapsed_uS = elapsed / us_ticks;
        if (elapsed_uS >= uS)
            break;

        /* reduce the delay by the elapsed time */
        uS -= elapsed_uS;

        /* keep fractional microseconds for the next iteration */
        elapsed %= us_ticks;
    }
}

void delayWaitms(uint32_t mS)
{
    while (mS--)
        delayWaitus(1000);
}
