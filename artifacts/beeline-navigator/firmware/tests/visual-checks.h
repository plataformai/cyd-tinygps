static void runVisualChecks() {
  display.rotation = 1;
  display.textSamples.clear();
  drawIdleScreen();
  const TFT_eSPI::TextSample* waiting = nullptr;
  const TFT_eSPI::TextSample* connect = nullptr;
  bool showsSpeedTitle = false;
  for (const auto& sample : display.textSamples) {
    assert(sample.value != "BEELINE");
    if (sample.value == "VELOCIDADE") showsSpeedTitle = true;
    if (sample.value == "Aguardando instrucoes") waiting = &sample;
    if (sample.value == "Conecte pelo app Android") connect = &sample;
  }
  assert(waiting && connect && waiting->x == 120 && connect->x == 120);
  assert(!showsSpeedTitle);
  assert(waiting->y + connect->y == 2 * (Layout::PANEL_TOP / 2));
  assert(connect->y - waiting->y == 32); // 16px text and one blank 16px line.

  const float samples[] = {0, 35, 50, 50.01f, 75, 100, 100.01f, 115, 140, 180};
  for (float value : samples) {
    speedAvailable = true;
    currentSpeedKmh = value;
    display.rectangles.clear();
    display.triangles.clear();
    drawSpeedPanel();
    int found = 0, bright = 0;
    const int starts[] = {Layout::SCALE_X,
      Layout::SCALE_X + speedScaleWidth() * 50 / 140,
      Layout::SCALE_X + speedScaleWidth() * 100 / 140};
    for (const auto& rect : display.rectangles) {
      if (rect.y == Layout::BAR_Y && rect.height == Layout::BAR_HEIGHT) {
        assert(rect.x == starts[found]);
        assert(rect.color == (found == speedBand(value) ? COLOR_AMBER : COLOR_SPEED_HIGH));
        bright += rect.color == COLOR_AMBER;
        ++found;
      }
    }
    assert(found == 3 && bright == 1);
    assert(Layout::SCALE_X + speedScaleWidth() + Layout::MARGIN +
           Layout::SCALE_BUTTON_CLEARANCE == display.width());
    const auto& marker = display.rectangles.back();
    assert(marker.width == 2 && marker.color == COLOR_TEXT);
    assert(marker.y >= Layout::PANEL_TOP && marker.y + marker.height <= Layout::SCALE_LABEL_Y);
    const auto& triangle = display.triangles.back();
    assert(triangle.color == COLOR_TEXT && triangle.y2 == marker.y);
    assert(triangle.y0 >= Layout::PANEL_TOP);
    assert(triangle.x0 >= 0 && triangle.x1 < display.width());
  }
  speedAvailable = false;
  display.triangles.clear();
  display.rectangles.clear();
  drawSpeedPanel();
  assert(display.triangles.empty());
  for (const auto& rect : display.rectangles) {
    if (rect.y == Layout::BAR_Y && rect.height == Layout::BAR_HEIGHT)
      assert(rect.color == COLOR_SPEED_HIGH);
  }
  dayMode = true;
  speedAvailable = true;
  currentSpeedKmh = 75;
  display.rectangles.clear();
  display.triangles.clear();
  drawSpeedPanel();
  std::vector<uint16_t> dayBands;
  for (const auto& rect : display.rectangles) {
    if (rect.y == Layout::BAR_Y && rect.height == Layout::BAR_HEIGHT)
      dayBands.push_back(rect.color);
  }
  assert(dayBands.size() == 3);
  assert(dayBands[0] == COLOR_SPEED_HIGH && dayBands[1] == COLOR_AMBER &&
         dayBands[2] == COLOR_SPEED_HIGH);
  assert(display.rectangles.back().color == COLOR_BLACK);
  dayMode = false;
  speedAvailable = false;
  // Crossing a band boundary must repaint even when rounded speed/pixel is unchanged.
  for (float boundary : {50.0f, 100.0f}) {
    write(std::string("{\"type\":\"speed\",\"valid\":true,\"speedKmh\":") +
          std::to_string(boundary) + "}"); tick();
    const auto before = display.panelDraws;
    write(std::string("{\"type\":\"speed\",\"valid\":true,\"speedKmh\":") +
          std::to_string(boundary + 0.01f) + "}"); tick();
    assert(display.panelDraws == before + 1);
  }
  std::cout << "PASS: centered idle, all three speed bands, 2px marker and attached triangle\n";
}
