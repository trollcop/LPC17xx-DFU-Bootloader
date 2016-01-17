#include "gpio.h"
#include "delay.h"
#include "buzzer.h"

#ifdef HW_V1
static PinName buzz = P1_9;
#endif

#ifdef HW_V2
static PinName buzz = P4_29;
#endif

void buzzerInit(void)
{
    GPIO_init(buzz);
    GPIO_output(buzz);
    GPIO_clear(buzz);
}

void buzzerBuzz(uint16_t freq, long duration)
{
    duration *= 1000; //convert from ms to us
    long period = 1000000 / freq; // period in us
    long elapsed_time = 0;

    while (elapsed_time < duration) {
        GPIO_set(buzz);
        delayWaitus(period / 2);
        GPIO_clear(buzz);
        delayWaitus(period / 2);
        elapsed_time += (period);
    }
}

void buzzerPlay(uint16_t *sequence)
{
    uint16_t freq;
    long duration;

    while (*sequence) {
        freq = sequence[0];
        duration = sequence[1];
        
        if (freq == 1)
            delayWaitms(duration);
        else
            buzzerBuzz(freq, duration);
        sequence += 2;
    }
}
