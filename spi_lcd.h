#pragma once

void lcdInit(void);
void lcdClear(void);
void lcdSetCursor(uint8_t col, uint8_t row);
void lcdWrite(const char *line);
