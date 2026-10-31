#pragma once
#include <Arduino.h>
#include <mutex>
using portMUX_TYPE = std::mutex;
#define portMUX_INITIALIZER_UNLOCKED {}
inline void portENTER_CRITICAL(portMUX_TYPE* mux) { mux->lock(); ++criticalDepth; }
inline void portEXIT_CRITICAL(portMUX_TYPE* mux) { --criticalDepth; mux->unlock(); }
