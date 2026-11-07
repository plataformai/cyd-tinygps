// Capture drawing commands from the production firmware, not a parallel layout.
#include <Arduino.h>
#include "../beeline-esp32.ino"

static void frame(const char* name, float speed, bool fresh, bool useDayMode = false) {
  std::printf("frame\t%s\n", name);
  dayMode = useDayMode;
  currentSpeedKmh = speed;
  speedAvailable = true;
  currentHeading = 45;
  headingAvailable = fresh;
  headingRemembered = true;
  remainingMeters = 3900;
  routeAvailable = true;
  display.traceDrawing = true;
  if (useDayMode) {
    renderInstruction({"text", "Em direcao a R. Haddock Lobo", 350, true});
  } else {
    drawIdleScreen();
  }
  display.traceDrawing = false;
}

int main() {
  frame("35 km/h - faixa 0 a 50", 35, true);
  frame("75 km/h - faixa 50 a 100", 75, true);
  frame("115 km/h - faixa acima de 100", 115, true);
  frame("Parado - ultimo rumo mantido na RAM", 0, false);
  frame("MODO DIA - texto e distancia da instrucao", 75, true, true);
}
