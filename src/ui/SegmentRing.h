#pragma once
#include "DisplayHAL.h"

// Fortschrittsring aus einzelnen Bloecken statt als weicher Bogen.
//
// 45 Segmente * 8 Grad = 360 Grad, davon 6 Grad gezeichnet und 2 Grad Luecke.
// Alles in ganzen Grad, weil TFT_eSPI::drawArc() Integer-Winkel nimmt.
// Winkelbezug wie beim mitgelieferten Arc: 0 = 12 Uhr, im Uhrzeigersinn,
// Start bei 180 = 6 Uhr.
//
// Vorteil gegenueber dem weichen Arc: waechst der Fortschritt, wird nur das
// neue Segment gezeichnet. Kein Ueberzeichnen, kein Flackern, kaum SPI-Last.
#define SEGRING_COUNT 45
#define SEGRING_STEP 8

class SegmentRing {
  int rOuter, rInner;
  uint16_t on, off, bg;
  int lit = 0;
  int drawn = -1;
  bool redrawAll = true;

public:
  SegmentRing(int rOuter, int rInner, uint32_t colorOn, uint32_t colorOff, uint32_t bgColor) {
    this->rOuter = rOuter;
    this->rInner = rInner;
    this->on = DisplayHAL::toSpiColor(colorOn);
    this->off = DisplayHAL::toSpiColor(colorOff);
    this->bg = DisplayHAL::toSpiColor(bgColor);
  }

  void setProgress(int percent) {
    int v = percent < 0 ? 0 : (percent > 100 ? 100 : percent);
    int n = (SEGRING_COUNT * v + 50) / 100;
    if (n == lit)
      return;
    if (n < lit)
      redrawAll = true; // rueckwaerts: geloeschte Bloecke muessen zurueck auf off
    lit = n;
  }

  void invalidate() { redrawAll = true; }

  void tick(DisplayHAL *hal) {
    if (!redrawAll && drawn == lit)
      return;

    TFT_eSPI *tft = hal->tft;
    int cx = tft->width() / 2;
    int cy = tft->height() / 2;

    int from = redrawAll ? 0 : (drawn < 0 ? 0 : drawn);
    int to = redrawAll ? SEGRING_COUNT : lit;

    tft->startWrite();
    for (int k = from; k < to; k++)
      segment(tft, cx, cy, k, k < lit ? on : off);
    tft->endWrite();

    drawn = lit;
    redrawAll = false;
  }

private:
  void segment(TFT_eSPI *tft, int cx, int cy, int k, uint16_t col) {
    int a0 = (180 + k * SEGRING_STEP + 1) % 360;
    int a1 = a0 + (SEGRING_STEP - 2);
    if (a1 >= 360) {
      tft->drawArc(cx, cy, rOuter, rInner, a0, 359, col, bg, true);
      tft->drawArc(cx, cy, rOuter, rInner, 0, a1 - 360, col, bg, true);
    } else {
      tft->drawArc(cx, cy, rOuter, rInner, a0, a1, col, bg, true);
    }
  }
};
