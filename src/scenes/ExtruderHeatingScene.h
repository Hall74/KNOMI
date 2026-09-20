#pragma once
#include "AbstractScene.h"
#include "ui/Readout.h"

// Gegenueber v2.5.0 geaendert: die beiden Temperaturen werden mit Readout in
// Reinweiss gezeichnet statt mit TextLabel in der Accent-Farbe.
//
// Die Positionen bleiben unveraendert bei 75 px ueber und unter der Mitte,
// also y=45 und y=195. Zusammen mit der gekuerzten Temperaturformatierung in
// KlipperStreaming.h ist das geloeschte Rechteck nur noch rund 70 px breit
// statt 180 - damit bleibt die Ringgrafik des Themes unangetastet.

// Sollwert zurueckgenommen, Istwert in Amber - so sieht man auf einen Blick,
// welche der beiden Zahlen sich bewegt.
// Beide Heiz-Szenen landen in derselben Uebersetzungseinheit, deshalb der
// Schutz gegen doppelte Definition.
#ifndef HEATSCENE_TARGET
#define HEATSCENE_TARGET 0x94A3B8
#define HEATSCENE_ACTUAL 0xFBBF24
#endif

class ExtruderHeatingScene : public AbstractScene {
private:
  ResourceImage *ri_img;
  Readout *actualTemp;
  Readout *targetTemp;

public:
  explicit ExtruderHeatingScene(SceneDeps deps) : AbstractScene(deps) {
    uint32_t bg = deps.styles->getBackgroundColor();
    int cx = deps.displayHAL->tft->width() / 2;
    int cy = deps.displayHAL->tft->height() / 2;

    ri_img = KnownResourceImages::get_ext_temp();
    targetTemp = new Readout(cx, cy - 75, gfxSmall, HEATSCENE_TARGET, bg);
    actualTemp = new Readout(cx, cy + 75, gfxSmall, HEATSCENE_ACTUAL, bg);
  }

  ~ExtruderHeatingScene() override {
    delete ri_img;
    delete actualTemp;
    delete targetTemp;
  }

  SwitchSceneRequest *NextScene() override {
    if (!deps.klipperStreaming->isHeatingExtruder()) {
      return new SwitchSceneRequest(deps, SceneId::Standby);
    }

    return nullptr;
  }

  void Tick() override {
    targetTemp->setText(deps.klipperStreaming->extruderTargetString);
    actualTemp->setText(deps.klipperStreaming->extruderTemperatureString);

    ri_img->tick(deps.displayHAL);
    targetTemp->tick(deps.displayHAL);
    actualTemp->tick(deps.displayHAL);
  }
};