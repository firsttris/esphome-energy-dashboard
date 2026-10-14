// Hilfsfunktionen für das Energie Dashboard (eingebunden über esphome.includes)
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <lvgl.h>
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"

namespace dashboard {

// Sensorwert oder 0, falls (noch) kein gültiger Wert vorliegt
inline float value_or_zero(esphome::sensor::Sensor *s) {
  return (s->has_state() && !std::isnan(s->state)) ? s->state : 0.0f;
}

// Sichtbarkeit nur umschalten, wenn sie sich wirklich ändert (spart Redraws)
inline void set_visible(lv_obj_t *obj, bool visible) {
  bool hidden = lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN);
  if (visible && hidden) {
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
  } else if (!visible && !hidden) {
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  }
}

// Setzt einen Bogen des Haus-Rings (Winkel in Grad, 0 = oben)
inline void set_arc_segment(lv_obj_t *arc, int start, int end) {
  if (end <= start) {
    set_visible(arc, false);
    return;
  }
  set_visible(arc, true);
  lv_arc_set_angles(arc, start, end);
}

// Ein animiertes Segment, das entlang eines Linienzugs (max. 3 Punkte) läuft
struct Flow {
  lv_obj_t *obj;
  int16_t pts[3][2];
  uint8_t n;
  float pos = 0;
  int8_t orientation = -1;  // 0 = horizontal, 1 = vertikal, 2 = diagonal

  int length() const {
    int total = 0;
    for (uint8_t i = 1; i < n; i++) {
      total += std::max(std::abs(pts[i][0] - pts[i - 1][0]), std::abs(pts[i][1] - pts[i - 1][1]));
    }
    return total;
  }

  // Bewegt das Segment um `speed` Pixel weiter; speed <= 0 blendet es aus.
  // reverse = true läuft den Linienzug vom Ziel zum Start.
  void step(float speed, bool reverse = false) {
    if (speed <= 0) {
      set_visible(obj, false);
      return;
    }
    int len = length();
    // Kurze Leitungen (z. B. Heizstab) nicht durchrasen lassen:
    // ein Durchlauf dauert mindestens 36 Frames (~1,4 s bei 40 ms)
    speed = std::min(speed, len / 36.0f);
    pos += speed;
    if (pos >= len)
      pos -= len;

    // Aktuellen Abschnitt und Position darauf bestimmen
    float remaining = reverse ? len - pos : pos;
    for (uint8_t i = 1; i < n; i++) {
      int dx = pts[i][0] - pts[i - 1][0];
      int dy = pts[i][1] - pts[i - 1][1];
      int leg = std::max(std::abs(dx), std::abs(dy));
      if (remaining > leg && i < n - 1) {
        remaining -= leg;
        continue;
      }
      float t = leg > 0 ? remaining / leg : 0;
      int x = pts[i - 1][0] + (int) (dx * t);
      int y = pts[i - 1][1] + (int) (dy * t);
      int8_t o = dy == 0 ? 0 : (dx == 0 ? 1 : 2);
      if (o != orientation) {
        orientation = o;
        if (o == 0)
          lv_obj_set_size(obj, 15, 4);
        else if (o == 1)
          lv_obj_set_size(obj, 4, 15);
        else
          lv_obj_set_size(obj, 8, 8);
      }
      int w = o == 0 ? 15 : (o == 1 ? 4 : 8);
      int h = o == 0 ? 4 : (o == 1 ? 15 : 8);
      lv_obj_set_pos(obj, x - w / 2, y - h / 2);
      break;
    }
    set_visible(obj, true);
  }
};

// Animationsgeschwindigkeit (Pixel pro Frame) abhängig von der Leistung
inline float flow_speed(float watts, float threshold, float full_speed) {
  if (watts <= threshold)
    return 0;
  float ratio = std::min(watts / full_speed, 1.0f);
  return 1.0f + ratio * 4.0f;
}

// Aufteilung der aktuellen Leistung (W) auf die einzelnen Flüsse
struct FlowSplit {
  float pv_home, pv_battery, pv_grid;
  float battery_home, battery_grid;
  float grid_home, grid_battery;
};

// Verteilt die Leistung wie ein Wechselrichter: PV versorgt zuerst das Haus,
// dann die Batterie, der Rest geht ins Netz. Die Batterie versorgt zuerst das
// Haus, den Rest speist sie ein. Das Netz deckt den restlichen Hausverbrauch
// und lädt danach die Batterie.
// Der Hausverbrauch kommt vom Sensor; die Sensoren gehen selten exakt auf, aus
// der Bilanz geschätzt fiele das Haus sonst schnell auf 0. Nur ohne gültigen
// Sensorwert (NAN) wird er aus der Bilanz berechnet.
// grid: + Bezug / - Einspeisung, battery: + Entladen / - Laden
inline FlowSplit split_flows(float solar, float grid, float battery, float home = NAN) {
  solar = std::max(solar, 0.0f);
  float grid_import = std::max(grid, 0.0f);
  float feed_in = std::max(-grid, 0.0f);
  float discharge = std::max(battery, 0.0f);
  float charge = std::max(-battery, 0.0f);
  if (std::isnan(home))
    home = solar + grid_import + discharge - feed_in - charge;
  home = std::max(home, 0.0f);

  FlowSplit f{};
  f.pv_home = std::min(solar, home);
  float pv_rest = solar - f.pv_home;
  f.pv_battery = std::min(pv_rest, charge);
  f.pv_grid = std::min(pv_rest - f.pv_battery, feed_in);

  float home_rest = home - f.pv_home;
  f.battery_home = std::min(discharge, home_rest);
  f.battery_grid = std::min(discharge - f.battery_home, std::max(feed_in - f.pv_grid, 0.0f));
  home_rest -= f.battery_home;

  f.grid_home = std::min(grid_import, home_rest);
  f.grid_battery = std::min(grid_import - f.grid_home, std::max(charge - f.pv_battery, 0.0f));
  return f;
}

// Erkennt, ob ein Tageszähler (z. B. Gas, Wasser) kürzlich gestiegen ist
struct ActivityTracker {
  float last = NAN;
  uint32_t active_until = 0;

  void feed(float value, uint32_t hold_ms = 5 * 60 * 1000) {
    if (!std::isnan(value) && !std::isnan(last) && value > last)
      active_until = esphome::millis() + hold_ms;
    last = value;
  }
  bool active() const { return active_until != 0 && (int32_t) (active_until - esphome::millis()) > 0; }
};

inline ActivityTracker gas_activity;
inline ActivityTracker water_activity;

}  // namespace dashboard
