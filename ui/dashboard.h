// Rechenlogik des Energie Dashboards (ohne LVGL): Flussaufteilung, Formatierung,
// Akku-Prognose, Hinweise. Die Darstellung steckt in ui/scene.h.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"

namespace dashboard {

// Sensorwert oder 0, falls (noch) kein gültiger Wert vorliegt
inline float value_or_zero(esphome::sensor::Sensor *s) {
  return (s->has_state() && !std::isnan(s->state)) ? s->state : 0.0f;
}
inline float nz(float v) { return std::isnan(v) ? 0.0f : v; }

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

// ---------------------------------------------------------------------------
// Formatierung (deutsch, Komma als Dezimaltrenner)
// ---------------------------------------------------------------------------
inline void comma(char *s) {
  for (; *s; s++)
    if (*s == '.')
      *s = ',';
}

// Leistung: "0 W", "740 W", "2,3 kW" (Betrag, Richtung zeigt die Anzeige)
inline void fmt_power(char *buf, size_t n, float w) {
  w = std::fabs(nz(w));
  if (w < 15)
    snprintf(buf, n, "0 W");
  else if (w < 995)
    snprintf(buf, n, "%d W", (int) lroundf(w / 10) * 10);
  else {
    snprintf(buf, n, "%.1f kW", w / 1000);
    comma(buf);
  }
}

inline void fmt_num(char *buf, size_t n, float v, int decimals, const char *unit) {
  snprintf(buf, n, "%.*f%s", decimals, nz(v), unit);
  comma(buf);
}

inline void fmt_clock(char *buf, size_t n, int minutes) {
  minutes = ((minutes % 1440) + 1440) % 1440;
  snprintf(buf, n, "%02d:%02d", minutes / 60, minutes % 60);
}

// ---------------------------------------------------------------------------
// Uhrzeit aus ISO-8601 (z. B. Attribut next_rising von sun.sun, in UTC)
// ---------------------------------------------------------------------------
inline int64_t days_from_civil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned) (y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (int64_t) era * 146097 + (int64_t) doe - 719468;
}

// Liefert Unix-Zeit oder -1
inline int64_t parse_iso(const std::string &s) {
  int y, mo, d, h, mi, se;
  if (sscanf(s.c_str(), "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &se) != 6)
    return -1;
  int64_t t = days_from_civil(y, mo, d) * 86400 + h * 3600 + mi * 60 + se;
  // Zeitzonen-Versatz am Ende: +hh:mm / -hh:mm / Z
  size_t p = s.find_last_of("+-");
  if (p != std::string::npos && p > 10) {
    int oh = 0, om = 0;
    if (sscanf(s.c_str() + p + 1, "%d:%d", &oh, &om) >= 1) {
      int off = oh * 3600 + om * 60;
      t -= s[p] == '+' ? off : -off;
    }
  }
  return t;
}

// ---------------------------------------------------------------------------
// Akku-Prognose: "voll um ~14:30" / "reicht bis ~05:40"
// power: + Entladen / - Laden (W), geglättet
// ---------------------------------------------------------------------------
inline void battery_eta(char *buf, size_t n, float soc, float power, float capacity_kwh, float reserve,
                        int now_min) {
  if (std::isnan(soc)) {
    snprintf(buf, n, " ");
    return;
  }
  if (soc >= 99.5f) {
    snprintf(buf, n, "voll");
    return;
  }
  if (power < -80) {
    float hours = (100 - soc) / 100 * capacity_kwh / (-power / 1000);
    if (hours > 12) {
      snprintf(buf, n, "lädt langsam");
      return;
    }
    char t[8];
    fmt_clock(t, sizeof t, now_min + (int) (hours * 60));
    snprintf(buf, n, "voll um ~%s", t);
    return;
  }
  if (soc <= reserve + 1) {
    snprintf(buf, n, "Reserve %d %%", (int) lroundf(soc));
    return;
  }
  if (power > 80) {
    float hours = (soc - reserve) / 100 * capacity_kwh / (power / 1000);
    if (hours > 24) {
      snprintf(buf, n, "reicht > 1 Tag");
      return;
    }
    char t[8];
    fmt_clock(t, sizeof t, now_min + (int) (hours * 60));
    snprintf(buf, n, "reicht bis ~%s", t);
    return;
  }
  snprintf(buf, n, "bereit");
}

}  // namespace dashboard
