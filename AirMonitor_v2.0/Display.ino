//Display.ino
#include "Global.h"

void drawCO2Graph(int x, int y, int w, int h) {
  tft.fillRect(x, y, w, h, ST77XX_BLACK);
  tft.drawRect(x, y, w, h, 0x4208);
  int nl = map(constrain(co2, 400, 2000), 400, 2000, 0, h - 4);
  for (int i = 0; i < w; i += 4) tft.drawPixel(x + i, y + h - 2 - nl, 0x03E0);
  for (int i = 0; i < GRAPH_SAMPLES - 1; i++) {
    if (co2History[i + 1] == 0) break;
    int v1 = map(constrain(co2History[i], 400, 2000), 400, 2000, 0, h - 4);
    int v2 = map(constrain(co2History[i + 1], 400, 2000), 400, 2000, 0, h - 4);
    tft.drawLine(x + i * 2, y + h - 2 - v1, x + (i + 1) * 2, y + h - 2 - v2, ST77XX_GREEN);
  }
}

void drawProgressBar(unsigned long ms) {
  static int lastW = -1;
  unsigned long diff = ms - lastDisplayUpdate;
  int bw = (diff * tft.width()) / 5000;
  if (bw > tft.width()) bw = tft.width();
  if (bw != lastW) {
    if (bw < lastW) tft.fillRect(0, tft.height() - 2, tft.width(), 2, ST77XX_BLACK);
    tft.fillRect(0, tft.height() - 2, bw, 2, ST77XX_CYAN);
    lastW = bw;
  }
}

void updateClock(unsigned long ms) {
  if (ms - lastClockUpdate < 1000) return;
  lastClockUpdate = ms;
  struct tm ti;
  bool timeValid = getLocalTime(&ti, 0) && (ti.tm_year > 100);

  if (timeValid != lastWasTime) {
    tft.fillRect(0, 0, tft.width(), 32, ST77XX_BLACK);
    lastWasTime = timeValid;
  }

  if (timeValid) {
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setTextSize(3); tft.setCursor(5, 5);
    char tStr[10]; strftime(tStr, sizeof(tStr), "%I:%M", &ti);
    if (ti.tm_sec % 2 != 0) tft.print(String(tStr[0]) + String(tStr[1]) + " " + String(tStr[3]) + String(tStr[4]));
    else tft.print(tStr);
    tft.setTextSize(1); tft.setCursor(95, 5);
    char pStr[5]; strftime(pStr, sizeof(pStr), "%p", &ti);
    tft.print(pStr);
  } else {
    tft.setTextSize(1); tft.setTextColor(0x7BEF, ST77XX_BLACK);
    tft.setCursor(10, 5); tft.print("UPTIME");
    long s = ms / 1000; int h = s / 3600; int m = (s % 3600) / 60; int sec = s % 60;
    tft.setTextSize(2); tft.setCursor(10, 15); tft.printf("%02d:%02d", h, m);
    if (sec % 2 == 0) tft.fillCircle(120, 10, 2, ST77XX_RED);
    else tft.fillCircle(120, 10, 2, ST77XX_BLACK);
  }
}