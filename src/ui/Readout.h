#pragma once
// TextLabel.h zieht die beiden Montserrat-Fonts herein. Bewusst darueber
// eingebunden statt direkt, damit die Font-Header nur einmal pro
// Uebersetzungseinheit landen (die generierten Header haben keinen Guard).
#include "TextLabel.h"

// Ein Textfeld mit FREI WAEHLBARER Farbe.
// Unterschied zu TextLabel: TextLabel nimmt immer config->getAccentColor().
// Hier laesst sich Weiss (oder was auch immer) fest vorgeben, und es gibt
// zusaetzlich den eingebauten GLCD-Pixelfont in doppelter/dreifacher Groesse.
// Nur LOAD_GLCD und LOAD_GFXFF sind in tft_setup.h aktiv - Font 2/4/6/7/8
// stehen NICHT zur Verfuegung.
enum readoutFont {
  glcd2,    // 12 x 16 px pro Zeichen, Versalhoehe 14 px  (~1,9 mm)
  glcd3,    // 18 x 24 px pro Zeichen                     (~2,8 mm)
  gfxSmall, // Montserrat 20pt, ca. 28 px hoch
  gfxLarge  // Montserrat 32pt, ca. 45 px hoch
};

class Readout {
  int cx, cy; // Mittelpunkt, absolute Pixel (MC_DATUM)
  readoutFont font;
  uint16_t fg, bg;
  String text;
  int16_t maxWidth = 0;
  bool dirty = false;

public:
  Readout(int cx, int cy, readoutFont font, uint32_t fgColor, uint32_t bgColor) {
    this->cx = cx;
    this->cy = cy;
    this->font = font;
    this->fg = DisplayHAL::toSpiColor(fgColor);
    this->bg = DisplayHAL::toSpiColor(bgColor);
  }

  void setText(const String &value) {
    if (this->text.equals(value))
      return;
    this->text = value;
    this->dirty = true;
  }

  void invalidate() { this->dirty = true; }

  void tick(DisplayHAL *hal) {
    if (!dirty)
      return;

    TFT_eSPI *tft = hal->tft;
    tft->startWrite();
    applyFont(tft);
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(fg, bg, true);

    int16_t w = tft->textWidth(this->text.c_str());
    maxWidth = max(maxWidth, w);
    int h = tft->fontHeight();

    // Gleiche Technik wie TextLabel: breitester je gezeigter String bestimmt
    // die zu loeschende Flaeche, damit kein Rest stehen bleibt.
    tft->setTextPadding(max(0, maxWidth - w));
    tft->fillRect(cx - maxWidth / 2, cy - h / 2, maxWidth, h, bg);
    tft->drawString(this->text, cx, cy);

    tft->setTextSize(1); // Groesse nicht fuer den naechsten Zeichner stehen lassen
    tft->endWrite();
    dirty = false;
  }

private:
  void applyFont(TFT_eSPI *tft) {
    switch (font) {
    case glcd2:
      tft->setTextFont(1);
      tft->setTextSize(2);
      break;
    case glcd3:
      tft->setTextFont(1);
      tft->setTextSize(3);
      break;
    case gfxSmall:
      tft->setTextSize(1);
      tft->setFreeFont(&Montserrat_Regular20pt7b);
      break;
    case gfxLarge:
      tft->setTextSize(1);
      tft->setFreeFont(&Montserrat_Regular32pt7b);
      break;
    }
  }
};