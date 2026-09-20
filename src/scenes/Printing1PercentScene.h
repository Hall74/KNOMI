#pragma once
#include "../log.h"
#include "AbstractScene.h"
#include "ui/Readout.h"
#include "ui/SegmentRing.h"

// Dichter Druck-Screen.
//
// Statt nur Bogen und Prozentzahl steht hier waehrend des ganzen Drucks:
//
//        L 42/318          Layer, sonst Z-Hoehe   GLCD x2, weiss
//          37%             Fortschritt            Montserrat 32pt, weiss
//        E215  B60         Hotend / Bett          GLCD x2, weiss
//        ETA 1:42          Restzeit, sonst Laufzeit
//
// aussen herum 45 Segmente als Fortschrittsring.
//
// Alle Textfelder nutzen Readout, nicht TextLabel: TextLabel zeichnet immer
// in der Accent-Farbe, hier soll es reines Weiss sein. Der Ring nimmt die
// Accent-Farbe aus der Konfiguration.
//
// Inhalt liegt vollstaendig innerhalb r <= 100, der Ring belegt r = 104..118.

#define PRINTSCREEN_WHITE 0xFFFFFF

class Printing1PercentScene : public AbstractScene {
public:
  SegmentRing *ring;
  Readout *pctText;
  Readout *layerText;
  Readout *tempText;
  Readout *etaText;
  SceneTimer *timer = nullptr;

  explicit Printing1PercentScene(SceneDeps deps) : AbstractScene(deps) {
    uint32_t bg = deps.styles->getBackgroundColor();
    ring = new SegmentRing(118, 104, deps.styles->getAccentColor(), 0x141B27, bg);
    layerText = new Readout(120, 66, glcd2, PRINTSCREEN_WHITE, bg);
    pctText = new Readout(120, 118, gfxLarge, PRINTSCREEN_WHITE, bg);
    tempText = new Readout(120, 160, glcd2, PRINTSCREEN_WHITE, bg);
    etaText = new Readout(120, 186, glcd2, PRINTSCREEN_WHITE, bg);
  }

  ~Printing1PercentScene() override {
    delete ring;
    delete pctText;
    delete layerText;
    delete tempText;
    delete etaText;
    delete timer;
  }

  SwitchSceneRequest *NextScene() override {
    if (!deps.klipperStreaming->isPrinting()) {
      return new SwitchSceneRequest(deps, SceneId::Standby);
    }

    int progress = this->getPrintProgress();
    if (progress == 100) {
      if (timer == nullptr) {
        timer = new SceneTimer(7000);
      } else if (timer->isCompleted()) {
        return new SwitchSceneRequest(deps, SceneId::Printing100Percent);
      }
    }

    ring->setProgress(progress);
    pctText->setText(String(progress) + "%");
    layerText->setText(formatLayer());
    tempText->setText(formatTemps());
    etaText->setText(formatEta(progress));

    return nullptr;
  }

  void Tick() override {
    ring->tick(deps.displayHAL);
    pctText->tick(deps.displayHAL);
    layerText->tick(deps.displayHAL);
    tempText->tick(deps.displayHAL);
    etaText->tick(deps.displayHAL);
  }

private:
  // Layer, wenn der Slicer SET_PRINT_STATS_INFO sendet - sonst Z-Hoehe.
  String formatLayer() {
    char buf[20];
    int total = deps.klipperStreaming->total_layer;
    if (total > 0) {
      snprintf(buf, sizeof(buf), "L %d/%d", deps.klipperStreaming->current_layer, total);
    } else {
      snprintf(buf, sizeof(buf), "Z %.1f", deps.klipperStreaming->positionZ);
    }
    return String(buf);
  }

  String formatTemps() {
    char buf[20];
    snprintf(buf, sizeof(buf), "E%3d  B%2d", (int)lroundf(deps.klipperStreaming->extruderTemperature),
             (int)lroundf(deps.klipperStreaming->bedTemperature));
    return String(buf);
  }

  // Restzeit, wenn sie sich schaetzen laesst - sonst die bisherige Laufzeit.
  String formatEta(int progress) {
    char buf[20];
    int rest = deps.klipperStreaming->remainingSeconds((float)progress);
    if (rest > 0) {
      snprintf(buf, sizeof(buf), "ETA %d:%02d", rest / 3600, (rest % 3600) / 60);
    } else {
      int el = (int)deps.klipperStreaming->print_duration;
      snprintf(buf, sizeof(buf), "T %d:%02d", el / 3600, (el % 3600) / 60);
    }
    return String(buf);
  }

  int cachedProgress = 0;
  int ticksUntilRefresh = 0;

  int getPrintProgress() {
    // Drucker sind nicht so schnell - nicht bei jedem Tick neu rechnen.
    if (ticksUntilRefresh > 0) {
      ticksUntilRefresh--;
      return cachedProgress;
    }

    String method = deps.klipperConfig->getPrintPercentageMethod();
    float progress = 0;
    bool haveValue = false;

    if (method == "file-relative") {
      int gcodeStartByte = deps.klipperStreaming->file_gcode_start_byte;
      int gcodeEndByte = deps.klipperStreaming->file_gcode_end_byte;
      int filePosition = deps.klipperStreaming->virtual_sdcard_file_position;

      if (gcodeStartByte != 0 && gcodeEndByte != 0 && filePosition != 0) {
        float currentPos = filePosition - gcodeStartByte;
        float maxPos = gcodeEndByte - gcodeStartByte;
        if (currentPos > 0 && maxPos > 0) {
          progress = (currentPos / maxPos) * 100;
          haveValue = true;
        }
      }
      // FEHLERKORREKTUR gegenueber v2.5.0: dort wurde diese Zeile immer
      // ausgefuehrt und hat das Ergebnis oben ueberschrieben - "file-relative"
      // verhielt sich dadurch wie "file-absolute".
      if (!haveValue)
        progress = deps.klipperStreaming->virtual_sdcard_progress * 100;
    } else if (method == "file-absolute") {
      progress = deps.klipperStreaming->virtual_sdcard_progress * 100;
    } else if (method == "slicer") {
      // braucht M73 vom Slicer, sonst liefert Klipper virtual_sdcard
      progress = deps.klipperStreaming->display_status_progress;
    } else if (method == "filament") {
      // FEHLERKORREKTUR gegenueber v2.5.0: dort stand hier eine Zuweisung
      // (method = "filament") statt eines Vergleichs.
      if (deps.klipperStreaming->file_filament_total > 0)
        progress = (deps.klipperStreaming->filament_used / deps.klipperStreaming->file_filament_total) * 100;
    }

    if (progress < 0) progress = 0;
    if (progress > 100) progress = 100;

    LV_LOG_INFO("Print Progress: %f method: %s", progress, method.c_str());
    cachedProgress = floor(progress);
    ticksUntilRefresh = 200;
    return cachedProgress;
  }
};
