#pragma once

void buzzerInit(void);
void buzzerBuzz(uint16_t freq, long duration);
void buzzerPlay(const uint16_t *sequence);
