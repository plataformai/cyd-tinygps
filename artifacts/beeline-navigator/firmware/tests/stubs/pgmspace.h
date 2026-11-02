#pragma once
#include <cstdint>

#define PROGMEM
inline uint16_t pgm_read_word(const uint16_t* address) { return *address; }
