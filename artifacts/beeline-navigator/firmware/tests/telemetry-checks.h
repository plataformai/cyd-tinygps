// Uses the production parser, loop and drawing functions, not another renderer.
static void runTelemetryChecks() {
  assert(navigationWidth() == 240);
  write(R"({"type":"heading","valid":true,"degrees":45})");
  write(R"({"type":"route","valid":true,"remainingMeters":3900,"ttlMs":30000})");
  write(R"({"type":"navigation","maneuver":"left","road":"Intercalado","distanceMeters":120})");
  for (int i = 0; i < 10000; ++i) write(R"({"type":"speed","valid":true,"speedKmh":50})");
  assert(headingPending && routePending && navigationPending && speedPending);
  tick();
  assert(headingAvailable && currentHeading == 45);
  assert(routeAvailable && remainingMeters == 3900 && remainingDistanceText() == "3,9 km");
  assert(currentInstruction.road == "Intercalado" && speedAvailable);
  assert(!display.circles.empty() && display.circles.back().color == COLOR_SPEED_HIGH);
  assert(display.circles.back().x == 280 && display.circles.back().radius == 22);
  for (const auto& triangle : display.triangles) {
    if (triangle.x0 >= 240) assert(triangle.x1 >= 240 && triangle.x2 >= 240 &&
                                  triangle.x0 < 320 && triangle.x1 < 320 && triangle.x2 < 320);
  }

  for (const auto degrees : {0, 45, 90, 180, 270, 359, 0}) {
    const auto fullDraws = display.fullDraws;
    display.textSamples.clear();
    display.triangles.clear();
    write(std::string("{\"type\":\"heading\",\"valid\":true,\"degrees\":") + std::to_string(degrees) + "}");
    tick();
    assert(display.fullDraws == fullDraws); // Only right column changes.
    for (int i = 0; i < 4; ++i) {
      int16_t x, y;
      cardinalPosition(i, degrees, x, y);
      assert(x >= 248 && x <= 312 && y >= 28 && y <= 92);
    }
    if (!display.triangles.empty()) {
      assert(display.triangles.size() == 2);
      const auto& arrow = display.triangles.front();
      assert(arrow.x0 == 280 && arrow.y0 == 44 && arrow.color == COLOR_BACKGROUND);
    }
  }
  int16_t x, y;
  cardinalPosition(0, 0, x, y); assert(x == 280 && y == 28);
  cardinalPosition(0, 90, x, y); assert(x == 248 && y == 60);
  cardinalPosition(1, 90, x, y); assert(x == 280 && y == 28);
  for (const auto& sample : display.textSamples) {
    if (sample.x >= 240) assert(sample.x < 320 && sample.y < Layout::PANEL_TOP);
  }
  assert(std::string(headingCardinal(0)) == "N");
  assert(std::string(headingCardinal(45)) == "NE");
  assert(std::string(headingCardinal(90)) == "L");
  assert(std::string(headingCardinal(180)) == "S");
  assert(std::string(headingCardinal(270)) == "O");
  assert(std::string(headingCardinal(359)) == "N");
  assert(normalizedHeading(360) == 0);
  int16_t x359, y359;
  cardinalPosition(0, 359, x359, y359);
  cardinalPosition(0, 0, x, y);
  assert(abs(x359 - x) <= 1 && abs(y359 - y) <= 1);

  // Wrongly typed packets neither clear current values nor overwrite valid pending data.
  write(R"({"type":"heading","valid":true,"degrees":180})");
  write(R"({"type":"heading","valid":true,"degrees":"90"})");
  write(R"({"type":"route","valid":true,"remainingMeters":900,"ttlMs":30000})");
  write(R"({"type":"route","valid":true,"remainingMeters":0.5,"ttlMs":30000})");
  write(R"({"type":"route","valid":true,"remainingMeters":1,"ttlMs":30001})");
  tick();
  assert(headingAvailable && currentHeading == 180);
  assert(routeAvailable && remainingMeters == 900 && remainingDistanceText() == "900 m");
  write(R"({"type":"heading","valid":true,"degrees":360})"); tick();
  assert(currentHeading == 0 && headingAvailable);
  for (const char* invalid : {
      R"({"type":"heading","valid":true,"degrees":-1})",
      R"({"type":"heading","valid":true,"degrees":361})",
      R"({"type":"heading","valid":true,"degrees":1e100})",
      R"({"type":"heading","valid":false,"degrees":null})"}) {
    write(R"({"type":"heading","valid":true,"degrees":45})"); tick();
    write(invalid); tick(); assert(!headingAvailable);
  }
  for (const char* invalid : {
      R"({"type":"route","valid":true,"remainingMeters":-1,"ttlMs":1000})",
      R"({"type":"route","valid":true,"remainingMeters":40000001,"ttlMs":1000})",
      R"({"type":"route","valid":false,"remainingMeters":null})"}) {
    write(R"({"type":"route","valid":true,"remainingMeters":40000000,"ttlMs":1000})"); tick();
    assert(remainingDistanceText() == "40000 km");
    write(invalid); tick(); assert(!routeAvailable);
  }
  write(R"({"type":"route","valid":true,"remainingMeters":0,"ttlMs":30000})"); tick();
  assert(routeAvailable && remainingDistanceText() == "0 m");
  write(R"({"type":"heading","valid":true,"degrees":90})"); tick(8000);
  assert(!headingAvailable && routeAvailable); // A delayed mailbox cannot revive stale course.
  write(R"({"type":"heading","valid":true,"degrees":90})"); tick(7999);
  assert(headingAvailable); tick(1); assert(!headingAvailable);
  write(R"({"type":"route","valid":true,"remainingMeters":3900,"ttlMs":500})"); tick(500);
  assert(!routeAvailable && remainingDistanceText() == "--");
  fakeNow = UINT32_MAX - 4000;
  write(R"({"type":"heading","valid":true,"degrees":90})");
  write(R"({"type":"route","valid":true,"remainingMeters":100,"ttlMs":30000})");
  tick(7999); assert(headingAvailable && routeAvailable);
  tick(1); assert(!headingAvailable && routeAvailable);
  tick(22000); assert(!routeAvailable);

  write(R"({"type":"heading","valid":true,"degrees":45})");
  write(R"({"type":"route","valid":true,"remainingMeters":3900,"ttlMs":30000})");
  tick();
  NimBLEConnInfo info;
  NimBLEDevice::createServer()->callbacks->onDisconnect(NimBLEDevice::createServer(), info, 0);
  tick();
  assert(!headingAvailable && !routeAvailable);
  std::cout << "PASS: 80px compass, cardinal orbit, fixed pointer, route formatting and independent mailboxes\n";
  std::cout << "PASS: heading/route validation, expiry, clock wrap, disconnect and partial refresh\n";
}
