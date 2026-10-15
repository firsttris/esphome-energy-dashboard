// Darstellung des Energie Dashboards (LVGL 9).
//
// Seite 1 "Energiefluss": Haus in der Mitte (Live-Leistung, Tagesmix-Ring, Autarkie),
// PV oben mit Sonnenbogen, Netz links, Akku rechts (Welle), Warmwasser unten,
// Kometen auf den Leitungen, Hinweis unten. Seite 2 "Heute": Tagesverlauf.
//
// Für flüssige Bilder auf dem ESP32-S3 gilt: pro Bild bewegen sich nur kleine
// Objekte. Glow, Sonne, Akku-Welle und Akku-Maske sind fertige Bilder (images/),
// LVGL rechnet keine Schatten, Masken oder Transparenz-Ebenen. Große Flächen
// (Himmel-Stimmung) ändern sich nur selten.
#pragma once

#include <array>
#include <functional>
#include <vector>
#include <lvgl.h>
#include "src/display/lv_display_private.h"  // inv_areas: wirklich neu gezeichnete Fläche
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "esphome/core/time.h"
#include "dashboard.h"

namespace dash {

using dashboard::fmt_power;
using dashboard::nz;

// ---------------------------------------------------------------------------
// Eingänge: werden von ui/bindings.yml gesetzt
// ---------------------------------------------------------------------------
struct Inputs {
  // Leistung (W): grid + Bezug / - Einspeisung, bat + Entladen / - Laden
  float pv = NAN, grid = NAN, bat = NAN, home = NAN, rod = NAN;
  float soc = NAN, boiler_temp = NAN;
  // Tageswerte (kWh bzw. m³)
  float pv_day = NAN, hoy_day = 0, import_day = NAN, export_day = NAN, home_day = NAN;
  float charge_day = NAN, discharge_day = NAN, rod_day = NAN, gas_day = NAN, water_day = NAN;
  float forecast_today = NAN, forecast_tomorrow = NAN;
  std::string next_rising, next_setting;
  bool online = false;
};
inline Inputs in;

struct Config {
  float threshold = 20, full_speed = 5000;
  float capacity_kwh = 9, reserve = 20;
  float price_import = 0.30f, price_export = 0.08f;
  bool show_fps = true;
};
inline Config cfg;

// Zeit: von YAML gesetzt (HA-Zeit). Für die Vorschau überschreibbar.
inline std::function<esphome::ESPTime()> now_fn;
inline int demo_minutes = -1;

struct Fonts {
  const lv_font_t *f32b, *f24b, *f20b, *f14, *f12, *f10b, *i20, *i16;
};
inline Fonts font{};

enum Dot { D_PV, D_GRID, D_EXP, D_BAT, D_BOIL, D_WHITE, D_COUNT };
struct Images {
  const lv_image_dsc_t *dot[D_COUNT];
  const lv_image_dsc_t *sun, *moon, *home_glow, *wave_front, *wave_back, *bat_mask;
};
inline Images img{};

// Farben
constexpr uint32_t BG = 0x0B0D10, DISC = 0x080A0D, FG = 0xEEF2F5, MUTED = 0x8693A0, FAINT = 0x4A5562;
constexpr uint32_t C_PV = 0xFFB627, C_GRID = 0x2EB8FF, C_EXP = 0xB57BFF, C_BAT = 0xFF3D8B, C_HOME = 0x2EE6A6,
                   C_BOIL = 0xFF6A2B, C_GAS = 0xFF7A6B, C_WATER = 0x33D6FF, C_MONEY = 0x7FF0C4;

inline lv_color_t hex(uint32_t c) { return lv_color_hex(c); }

// ---------------------------------------------------------------------------
// Kleine Bausteine
// ---------------------------------------------------------------------------
inline lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}

inline lv_obj_t *disc(lv_obj_t *parent, int cx, int cy, int r, uint32_t fill, lv_opa_t fill_opa = LV_OPA_COVER) {
  lv_obj_t *o = box(parent, cx - r, cy - r, 2 * r, 2 * r);
  lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(o, hex(fill), 0);
  lv_obj_set_style_bg_opa(o, fill_opa, 0);
  return o;
}

inline void ring_border(lv_obj_t *o, uint32_t col, lv_opa_t opa, int w = 2) {
  lv_obj_set_style_border_color(o, hex(col), 0);
  lv_obj_set_style_border_opa(o, opa, 0);
  lv_obj_set_style_border_width(o, w, 0);
}

// Label mit Ankerpunkt x: LEFT = linke Kante, CENTER = Mitte, RIGHT = rechte Kante
inline lv_obj_t *label(lv_obj_t *parent, const lv_font_t *f, uint32_t col, int x, int y, int w,
                       lv_text_align_t align, const char *t, int letter_space = 0) {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_set_width(l, w);
  int left = align == LV_TEXT_ALIGN_CENTER ? x - w / 2 : align == LV_TEXT_ALIGN_RIGHT ? x - w : x;
  lv_obj_set_pos(l, left, y);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, hex(col), 0);
  lv_obj_set_style_text_align(l, align, 0);
  if (letter_space)
    lv_obj_set_style_text_letter_space(l, letter_space, 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_CLIP);
  lv_label_set_text(l, t);
  return l;
}

// Text nur setzen, wenn er sich ändert (sonst zeichnet LVGL unnötig neu)
inline void set_text(lv_obj_t *l, const char *t) {
  if (l && strcmp(lv_label_get_text(l), t) != 0)
    lv_label_set_text(l, t);
}

inline void set_visible(lv_obj_t *o, bool v) {
  bool hidden = lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN);
  if (v && hidden)
    lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  else if (!v && !hidden)
    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

inline void set_pos(lv_obj_t *o, int x, int y) {
  if (lv_obj_get_x(o) != x || lv_obj_get_y(o) != y)
    lv_obj_set_pos(o, x, y);
}

inline lv_obj_t *image(lv_obj_t *parent, const lv_image_dsc_t *src, int x, int y) {
  lv_obj_t *o = lv_image_create(parent);
  lv_image_set_src(o, src);
  lv_obj_set_pos(o, x, y);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  return o;
}

inline lv_obj_t *arc(lv_obj_t *parent, int cx, int cy, int r, int width, uint32_t col, lv_opa_t opa) {
  lv_obj_t *a = lv_arc_create(parent);
  lv_obj_remove_style_all(a);
  lv_obj_set_pos(a, cx - r, cy - r);
  lv_obj_set_size(a, 2 * r, 2 * r);
  lv_obj_remove_flag(a, LV_OBJ_FLAG_CLICKABLE);
  lv_arc_set_rotation(a, 270);
  lv_arc_set_bg_angles(a, 0, 360);
  lv_arc_set_angles(a, 0, 0);
  lv_obj_set_style_arc_width(a, width, LV_PART_MAIN);
  lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_arc_width(a, width, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(a, hex(col), LV_PART_INDICATOR);
  lv_obj_set_style_arc_opa(a, opa, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(a, false, LV_PART_INDICATOR);
  return a;
}

inline void arc_span(lv_obj_t *a, int start, int end) {
  if (end - start < 2) {
    set_visible(a, false);
    return;
  }
  set_visible(a, true);
  if (lv_arc_get_angle_start(a) != start || lv_arc_get_angle_end(a) != end)
    lv_arc_set_angles(a, start, end);
}

// ---------------------------------------------------------------------------
// Leitungen und Kometen
// ---------------------------------------------------------------------------
struct Path {
  std::vector<float> x, y, cum;
  std::vector<lv_point_precise_t> pts;
  float len = 0;
  lv_obj_t *line = nullptr, *glow = nullptr;
  int shown = -1;           // sichtbare Punkte (Startanimation)
  int state = -1;           // 0 = ruhig, 1 = aktiv
  uint32_t color = 0;
};

inline void path_finish(Path &p) {
  p.cum.assign(p.x.size(), 0);
  for (size_t i = 1; i < p.x.size(); i++)
    p.cum[i] = p.cum[i - 1] + std::hypot(p.x[i] - p.x[i - 1], p.y[i] - p.y[i - 1]);
  p.len = p.cum.back();
  p.pts.resize(p.x.size());
  for (size_t i = 0; i < p.x.size(); i++)
    p.pts[i] = {(lv_value_precise_t) lroundf(p.x[i]), (lv_value_precise_t) lroundf(p.y[i])};
}

inline Path make_line(float x0, float y0, float x1, float y1, int n = 8) {
  Path p;
  for (int i = 0; i <= n; i++) {
    float t = (float) i / n;
    p.x.push_back(x0 + (x1 - x0) * t);
    p.y.push_back(y0 + (y1 - y0) * t);
  }
  path_finish(p);
  return p;
}

inline Path make_cubic(float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy, int n = 24) {
  Path p;
  for (int i = 0; i <= n; i++) {
    float t = (float) i / n, u = 1 - t;
    p.x.push_back(u * u * u * ax + 3 * u * u * t * bx + 3 * u * t * t * cx + t * t * t * dx);
    p.y.push_back(u * u * u * ay + 3 * u * u * t * by + 3 * u * t * t * cy + t * t * t * dy);
  }
  path_finish(p);
  return p;
}

inline void path_at(const Path &p, float d, float &x, float &y) {
  d = std::clamp(d, 0.0f, p.len);
  size_t i = 1;
  while (i < p.cum.size() - 1 && p.cum[i] < d)
    i++;
  float seg = p.cum[i] - p.cum[i - 1];
  float t = seg > 0 ? (d - p.cum[i - 1]) / seg : 0;
  x = p.x[i - 1] + (p.x[i] - p.x[i - 1]) * t;
  y = p.y[i - 1] + (p.y[i] - p.y[i - 1]) * t;
}

struct Comet {
  lv_obj_t *head = nullptr, *tail = nullptr;
};

struct Flow {
  int path;
  bool rev;
  Dot dot;
  uint32_t color;
  float power = 0, ofs = 0;
  bool on = false;
  std::vector<Comet> comets;
};

enum PathId { P_PV_HOME, P_PV_GRID, P_PV_BAT, P_GRID_HOME, P_BAT_HOME, P_GRID_BAT, P_HOME_BOIL, P_COUNT };

// ---------------------------------------------------------------------------
// Zustand der Oberfläche
// ---------------------------------------------------------------------------
struct Node {
  int x, y, r;
};
constexpr Node N_PV{240, 96, 40}, N_GRID{66, 250, 46}, N_HOME{240, 250, 64}, N_BAT{414, 250, 46}, N_BOIL{240, 392, 32};

struct Particle {
  lv_obj_t *o;
  float x, y, vx, vy, life;
};

struct UI {
  lv_obj_t *page = nullptr, *page2 = nullptr;
  // Himmel
  lv_obj_t *sky = nullptr, *arc_full = nullptr, *arc_done = nullptr, *sun = nullptr, *moon = nullptr;
  lv_obj_t *rise_lbl = nullptr, *set_lbl = nullptr;
  std::array<lv_point_precise_t, 41> sky_pts{};
  // Kopf
  lv_obj_t *clock = nullptr, *date = nullptr, *money = nullptr, *money_sub = nullptr;
  // Knoten
  lv_obj_t *g_pv = nullptr, *g_grid = nullptr, *g_home = nullptr, *g_bat = nullptr, *g_boil = nullptr, *g_util = nullptr;
  lv_obj_t *pv_disc, *pv_ring, *pv_val, *pv_day;
  lv_obj_t *grid_disc, *grid_icon, *grid_val, *grid_state, *grid_in, *grid_out;
  lv_obj_t *home_mix[3], *home_val, *home_aut;
  lv_obj_t *bat_wave_back, *bat_wave_front, *bat_ring, *bat_soc, *bat_pow, *bat_eta;
  lv_obj_t *bubbles[5];
  lv_obj_t *boil_disc, *boil_temp, *boil_rod, *sparks[4];
  lv_obj_t *gas_icon, *gas_val, *water_icon, *water_val;
  // Hinweis
  lv_obj_t *banner = nullptr, *banner_dot = nullptr, *banner_lbl = nullptr;
  // Partikel bei "Akku voll"
  std::array<Particle, 24> burst{};
  lv_obj_t *shock = nullptr;
  float shock_t = -1;
  // Seitenpunkte, FPS
  lv_obj_t *dots[2] = {nullptr, nullptr};
  lv_obj_t *fps = nullptr;
  // Seite 2
  lv_obj_t *p2_sub, *p2_val[4], *p2_bars[96], *p2_now, *p2_now_lbl, *p2_soc_lbl;
  lv_obj_t *p2_pv_line, *p2_home_line, *p2_soc_line;
  std::array<lv_point_precise_t, 96> p2_pv_pts{}, p2_home_pts{}, p2_soc_pts{};
};
inline UI ui;

inline std::array<Path, P_COUNT> paths;
inline std::vector<Flow> flows;

// Angezeigte (weich nachgeführte) Werte
struct Shown {
  float pv = 0, grid = 0, bat = 0, home = 0, rod = 0, soc = 0;
  float bat_slow = 0;  // für die Akku-Prognose, stark geglättet
};
inline Shown shown;

inline float boot_t = 99;  // Sekunden seit Start der Startanimation
inline bool booting = false;
inline float wave_front = 0, wave_back = 0;
inline uint32_t last_tick = 0;

// ---------------------------------------------------------------------------
// Zeit
// ---------------------------------------------------------------------------
inline bool time_valid() {
  if (demo_minutes >= 0)
    return true;
  return now_fn && now_fn().is_valid();
}
inline int now_minutes() {
  if (demo_minutes >= 0)
    return demo_minutes;
  if (!now_fn)
    return 0;
  auto t = now_fn();
  return t.is_valid() ? t.hour * 60 + t.minute : 0;
}

// ---------------------------------------------------------------------------
// Tagesverlauf (Seite 2): alle 15 min ein Mittelwert, übersteht Neustarts
// ---------------------------------------------------------------------------
struct History {
  uint32_t day = 0;
  uint16_t pv[96];
  uint16_t home[96];
  uint8_t soc[96];
  uint8_t valid[96];
};
inline History hist{};
inline esphome::ESPPreferenceObject hist_pref;
inline bool hist_loaded = false;
inline int hist_slot = -1;
inline float acc_pv = 0, acc_home = 0;
inline int acc_n = 0;

inline uint32_t day_key() {
  if (demo_minutes >= 0)
    return 1;
  auto t = now_fn();
  return t.year * 1000 + t.day_of_year;
}

inline void hist_clear(uint32_t day) {
  memset(&hist, 0, sizeof hist);
  hist.day = day;
}

inline void hist_sample() {
  if (!time_valid())
    return;
  uint32_t day = day_key();
  if (!hist_loaded) {
    hist_loaded = true;
    hist_pref = esphome::global_preferences->make_preference<History>(0x45484953UL);
    if (!hist_pref.load(&hist) || hist.day != day)
      hist_clear(day);
  }
  if (hist.day != day) {
    hist_clear(day);
    hist_slot = -1;
  }
  int slot = now_minutes() / 15;
  if (hist_slot >= 0 && slot != hist_slot && acc_n > 0) {
    hist.pv[hist_slot] = (uint16_t) std::clamp(acc_pv / acc_n, 0.0f, 60000.0f);
    hist.home[hist_slot] = (uint16_t) std::clamp(acc_home / acc_n, 0.0f, 60000.0f);
    hist.soc[hist_slot] = (uint8_t) std::clamp(nz(in.soc), 0.0f, 100.0f);
    hist.valid[hist_slot] = 1;
    hist_pref.save(&hist);
    acc_pv = acc_home = 0;
    acc_n = 0;
  }
  hist_slot = slot;
  if (in.online && !std::isnan(in.pv)) {
    acc_pv += std::max(in.pv, 0.0f);
    acc_home += std::max(nz(in.home), 0.0f);
    acc_n++;
  }
}

// ---------------------------------------------------------------------------
// Aufbau Seite 1
// ---------------------------------------------------------------------------
inline void build_sky(lv_obj_t *p) {
  ui.sky = box(p, 0, 0, 480, 200);
  lv_obj_set_style_bg_opa(ui.sky, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(ui.sky, hex(0x14171C), 0);
  lv_obj_set_style_bg_grad_color(ui.sky, hex(BG), 0);
  lv_obj_set_style_bg_grad_dir(ui.sky, LV_GRAD_DIR_VER, 0);

  for (int i = 0; i <= 40; i++) {
    float th = M_PI * (1 - i / 40.0f);
    ui.sky_pts[i] = {(lv_value_precise_t) lroundf(240 + 190 * cosf(th)), (lv_value_precise_t) lroundf(150 - 105 * sinf(th))};
  }
  ui.arc_full = lv_line_create(p);
  lv_line_set_points(ui.arc_full, ui.sky_pts.data(), 41);
  lv_obj_set_style_line_width(ui.arc_full, 1, 0);
  lv_obj_set_style_line_color(ui.arc_full, hex(C_PV), 0);
  lv_obj_set_style_line_opa(ui.arc_full, 50, 0);
  ui.arc_done = lv_line_create(p);
  lv_line_set_points(ui.arc_done, ui.sky_pts.data(), 1);
  lv_obj_set_style_line_width(ui.arc_done, 2, 0);
  lv_obj_set_style_line_color(ui.arc_done, hex(C_PV), 0);
  lv_obj_set_style_line_opa(ui.arc_done, 120, 0);
  lv_obj_set_style_line_rounded(ui.arc_done, true, 0);
  ui.rise_lbl = label(p, font.f12, MUTED, 34, 158, 50, LV_TEXT_ALIGN_CENTER, "--:--");
  ui.set_lbl = label(p, font.f12, MUTED, 446, 158, 50, LV_TEXT_ALIGN_CENTER, "--:--");
  ui.sun = image(p, img.sun, 0, 0);
  ui.moon = image(p, img.moon, 0, 0);
  lv_obj_add_flag(ui.moon, LV_OBJ_FLAG_HIDDEN);
}

inline void build_paths(lv_obj_t *p) {
  paths[P_PV_HOME] = make_line(240, 136, 240, 186);
  paths[P_PV_GRID] = make_cubic(200, 96, 128, 96, 66, 140, 66, 204);
  paths[P_PV_BAT] = make_cubic(280, 96, 352, 96, 414, 140, 414, 204);
  paths[P_GRID_HOME] = make_line(112, 250, 176, 250);
  paths[P_BAT_HOME] = make_line(368, 250, 304, 250);
  paths[P_GRID_BAT] = make_cubic(66, 296, 66, 374, 414, 374, 414, 296);
  paths[P_HOME_BOIL] = make_line(240, 314, 240, 360);
  for (auto &pa : paths) {
    pa.glow = lv_line_create(p);
    lv_line_set_points(pa.glow, pa.pts.data(), pa.pts.size());
    lv_obj_set_style_line_width(pa.glow, 9, 0);
    lv_obj_set_style_line_rounded(pa.glow, true, 0);
    lv_obj_set_style_line_opa(pa.glow, 0, 0);
    pa.line = lv_line_create(p);
    lv_line_set_points(pa.line, pa.pts.data(), pa.pts.size());
    lv_obj_set_style_line_rounded(pa.line, true, 0);
  }
  flows = {
      {P_PV_HOME, false, D_PV, C_PV},       {P_PV_GRID, false, D_EXP, C_EXP},   {P_PV_BAT, false, D_PV, C_PV},
      {P_GRID_HOME, false, D_GRID, C_GRID}, {P_BAT_HOME, false, D_BAT, C_BAT},  {P_GRID_BAT, false, D_WHITE, 0xFFFFFF},
      {P_GRID_BAT, true, D_BAT, C_BAT},     {P_HOME_BOIL, false, D_BOIL, C_BOIL},
  };
  for (auto &f : flows) {
    int n = std::clamp((int) (paths[f.path].len / 34), 1, 6);
    for (int i = 0; i < n; i++) {
      Comet c;
      c.tail = disc(p, 0, 0, 2, f.color, 130);
      c.head = image(p, img.dot[f.dot], 0, 0);
      lv_obj_add_flag(c.tail, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(c.head, LV_OBJ_FLAG_HIDDEN);
      f.comets.push_back(c);
    }
  }
}

inline void build_nodes(lv_obj_t *p) {
  // PV
  ui.g_pv = box(p, 0, 0, 480, 480);
  {
    auto n = N_PV;
    ui.pv_disc = disc(ui.g_pv, n.x, n.y, n.r, DISC);
    ring_border(ui.pv_disc, C_PV, 50, 2);
    ui.pv_ring = arc(ui.g_pv, n.x, n.y, n.r, 4, C_PV, LV_OPA_COVER);
    label(ui.g_pv, font.f10b, C_PV, n.x, n.y - 25, 60, LV_TEXT_ALIGN_CENTER, "PV", 2);
    ui.pv_val = label(ui.g_pv, font.f20b, FG, n.x, n.y - 10, 76, LV_TEXT_ALIGN_CENTER, "--");
    ui.pv_day = label(ui.g_pv, font.f12, MUTED, n.x, n.y + 12, 70, LV_TEXT_ALIGN_CENTER, "");
  }
  // Netz
  ui.g_grid = box(p, 0, 0, 480, 480);
  {
    auto n = N_GRID;
    ui.grid_disc = disc(ui.g_grid, n.x, n.y, n.r, DISC);
    ring_border(ui.grid_disc, FAINT, 120, 2);
    ui.grid_icon = label(ui.g_grid, font.i20, FAINT, n.x, n.y - 34, 30, LV_TEXT_ALIGN_CENTER, "\U000F0D3E");
    ui.grid_val = label(ui.g_grid, font.f20b, FG, n.x, n.y - 10, 88, LV_TEXT_ALIGN_CENTER, "--");
    ui.grid_state = label(ui.g_grid, font.f10b, FAINT, n.x, n.y + 14, 88, LV_TEXT_ALIGN_CENTER, "", 1);
    label(ui.g_grid, font.i16, C_GRID, 14, n.y + n.r + 8, 16, LV_TEXT_ALIGN_LEFT, "\U000F0045");
    ui.grid_in = label(ui.g_grid, font.f12, MUTED, 30, n.y + n.r + 9, 40, LV_TEXT_ALIGN_LEFT, "--");
    label(ui.g_grid, font.i16, C_EXP, 66, n.y + n.r + 8, 16, LV_TEXT_ALIGN_LEFT, "\U000F005D");
    ui.grid_out = label(ui.g_grid, font.f12, MUTED, 82, n.y + n.r + 9, 60, LV_TEXT_ALIGN_LEFT, "--");
  }
  // Haus
  ui.g_home = box(p, 0, 0, 480, 480);
  {
    auto n = N_HOME;
    image(ui.g_home, img.home_glow, n.x - 100, n.y - 100);
    disc(ui.g_home, n.x, n.y, n.r, DISC);
    lv_obj_t *bg = arc(ui.g_home, n.x, n.y, n.r - 1, 6, 0xFFFFFF, 16);
    lv_arc_set_angles(bg, 0, 360);
    const uint32_t cols[3] = {C_PV, C_BAT, C_GRID};
    for (int i = 0; i < 3; i++)
      ui.home_mix[i] = arc(ui.g_home, n.x, n.y, n.r - 1, 6, cols[i], LV_OPA_COVER);
    label(ui.g_home, font.f10b, C_HOME, n.x, n.y - 33, 80, LV_TEXT_ALIGN_CENTER, "HAUS", 3);
    ui.home_val = label(ui.g_home, font.f32b, FG, n.x, n.y - 19, 116, LV_TEXT_ALIGN_CENTER, "--");
    ui.home_aut = label(ui.g_home, font.f12, MUTED, n.x, n.y + 18, 100, LV_TEXT_ALIGN_CENTER, "");
  }
  // Akku: Scheibe, Welle (hinten/vorne), Bläschen, Maske, Ring, Text
  ui.g_bat = box(p, N_BAT.x - 48, N_BAT.y - 48, 96, 96);
  {
    disc(ui.g_bat, 48, 48, 46, DISC);
    ui.bat_wave_back = image(ui.g_bat, img.wave_back, 0, 96);
    ui.bat_wave_front = image(ui.g_bat, img.wave_front, 0, 96);
    for (auto &b : ui.bubbles) {
      b = disc(ui.g_bat, 0, 0, 2, 0xFFDCEB, 140);
      lv_obj_add_flag(b, LV_OBJ_FLAG_HIDDEN);
    }
    image(ui.g_bat, img.bat_mask, 0, 0);
    ui.bat_ring = disc(ui.g_bat, 48, 48, 46, 0, LV_OPA_TRANSP);
    ring_border(ui.bat_ring, C_BAT, 140, 2);
    ui.bat_soc = label(ui.g_bat, font.f24b, FG, 48, 24, 90, LV_TEXT_ALIGN_CENTER, "--");
    ui.bat_pow = label(ui.g_bat, font.f12, 0xFFD1E4, 48, 52, 90, LV_TEXT_ALIGN_CENTER, "AKKU");
  }
  ui.bat_eta = label(p, font.f12, MUTED, N_BAT.x - 2, N_BAT.y + N_BAT.r + 9, 120, LV_TEXT_ALIGN_CENTER, "");
  // Warmwasser / Heizstab
  ui.g_boil = box(p, 0, 0, 480, 480);
  {
    auto n = N_BOIL;
    ui.boil_disc = disc(ui.g_boil, n.x, n.y, n.r, DISC);
    lv_obj_set_style_bg_grad_dir(ui.boil_disc, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_grad_color(ui.boil_disc, hex(DISC), 0);
    ring_border(ui.boil_disc, C_BOIL, 110, 2);
    for (auto &s : ui.sparks) {
      s = disc(ui.g_boil, 0, 0, 1, 0xFFC878, 200);
      lv_obj_add_flag(s, LV_OBJ_FLAG_HIDDEN);
    }
    ui.boil_temp = label(ui.g_boil, font.f20b, FG, n.x + 2, n.y - 12, 60, LV_TEXT_ALIGN_CENTER, "--");
    label(ui.g_boil, font.f10b, MUTED, n.x + n.r + 12, n.y - 14, 70, LV_TEXT_ALIGN_LEFT, "HEIZSTAB", 1);
    ui.boil_rod = label(ui.g_boil, font.f14, MUTED, n.x + n.r + 12, n.y - 1, 70, LV_TEXT_ALIGN_LEFT, "");
  }
  // Gas und Wasser (untere Ecken)
  ui.g_util = box(p, 0, 0, 480, 480);
  {
    ui.gas_icon = label(ui.g_util, font.i20, C_GAS, 18, 382, 24, LV_TEXT_ALIGN_LEFT, "\U000F0238");
    ui.gas_val = label(ui.g_util, font.f14, FG, 42, 380, 80, LV_TEXT_ALIGN_LEFT, "--");
    label(ui.g_util, font.f10b, MUTED, 42, 398, 90, LV_TEXT_ALIGN_LEFT, "GAS HEUTE", 1);
    ui.water_icon = label(ui.g_util, font.i20, C_WATER, 462, 382, 24, LV_TEXT_ALIGN_RIGHT, "\U000F058C");
    ui.water_val = label(ui.g_util, font.f14, FG, 438, 380, 80, LV_TEXT_ALIGN_RIGHT, "--");
    label(ui.g_util, font.f10b, MUTED, 438, 398, 100, LV_TEXT_ALIGN_RIGHT, "WASSER HEUTE", 1);
  }
}

inline void build_header(lv_obj_t *p) {
  ui.clock = label(p, font.f32b, FG, 20, 8, 120, LV_TEXT_ALIGN_LEFT, "--:--");
  ui.date = label(p, font.f12, MUTED, 21, 44, 120, LV_TEXT_ALIGN_LEFT, "");
  ui.money = label(p, font.f24b, C_MONEY, 460, 12, 150, LV_TEXT_ALIGN_RIGHT, "");
  ui.money_sub = label(p, font.f12, MUTED, 460, 42, 150, LV_TEXT_ALIGN_RIGHT, "heute gespart");
}

inline void build_banner(lv_obj_t *p) {
  ui.banner = box(p, 40, 480, 400, 28);
  lv_obj_set_style_radius(ui.banner, 14, 0);
  lv_obj_set_style_bg_color(ui.banner, hex(0x101318), 0);
  lv_obj_set_style_bg_opa(ui.banner, LV_OPA_COVER, 0);
  ring_border(ui.banner, C_PV, 110, 1);
  ui.banner_dot = disc(ui.banner, 16, 14, 3, C_PV);
  ui.banner_lbl = label(ui.banner, font.f14, FG, 28, 5, 360, LV_TEXT_ALIGN_LEFT, "");
}

inline void build_burst(lv_obj_t *p) {
  const uint32_t cols[5] = {C_BAT, C_PV, 0xFFFFFF, C_HOME, C_EXP};
  for (size_t i = 0; i < ui.burst.size(); i++) {
    ui.burst[i].o = disc(p, 0, 0, 2, cols[i % 5]);
    ui.burst[i].life = 0;
    lv_obj_add_flag(ui.burst[i].o, LV_OBJ_FLAG_HIDDEN);
  }
  ui.shock = disc(p, N_BAT.x, N_BAT.y, N_BAT.r, 0, LV_OPA_TRANSP);
  ring_border(ui.shock, C_BAT, 200, 3);
  lv_obj_add_flag(ui.shock, LV_OBJ_FLAG_HIDDEN);
}

// ---------------------------------------------------------------------------
// Aufbau Seite 2
// ---------------------------------------------------------------------------
constexpr int CX0 = 50, CX1 = 452, CY0 = 384, CY1 = 150;
inline int chart_x(float slot) { return CX0 + (int) lroundf((CX1 - CX0) * slot / 96.0f); }
inline int chart_y(float watts) { return CY0 - (int) lroundf((CY0 - CY1) * std::clamp(watts, 0.0f, 10000.0f) / 10000.0f); }
inline int chart_ys(float soc) { return CY0 - (int) lroundf((CY0 - CY1) * std::clamp(soc, 0.0f, 100.0f) / 100.0f); }

inline lv_obj_t *line_obj(lv_obj_t *p, uint32_t col, int w, lv_opa_t opa = LV_OPA_COVER) {
  lv_obj_t *l = lv_line_create(p);
  lv_obj_set_style_line_width(l, w, 0);
  lv_obj_set_style_line_color(l, hex(col), 0);
  lv_obj_set_style_line_opa(l, opa, 0);
  lv_obj_set_style_line_rounded(l, true, 0);
  return l;
}

inline void build_page2(lv_obj_t *p) {
  label(p, font.f24b, FG, 22, 14, 120, LV_TEXT_ALIGN_LEFT, "Heute");
  ui.p2_sub = label(p, font.f12, MUTED, 458, 22, 260, LV_TEXT_ALIGN_RIGHT, "");
  const char *names[4] = {"PV", "VERBRAUCH", "AUTARKIE", "EINSPEISUNG"};
  const uint32_t cols[4] = {C_PV, C_HOME, FG, C_EXP};
  for (int i = 0; i < 4; i++) {
    int x = 22 + i * 112;
    ui.p2_val[i] = label(p, font.f20b, FG, x, 58, 110, LV_TEXT_ALIGN_LEFT, "--");
    lv_obj_t *bar = box(p, x, 92, 14, 3);
    lv_obj_set_style_bg_color(bar, hex(cols[i]), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    label(p, font.f10b, MUTED, x + 20, 86, 90, LV_TEXT_ALIGN_LEFT, names[i], 1);
  }
  // Raster und Achsen
  for (int k = 0; k <= 4; k++) {
    int y = chart_y(k * 2500);
    lv_obj_t *g = box(p, CX0, y, CX1 - CX0, 1);
    lv_obj_set_style_bg_color(g, hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(g, 16, 0);
    static const char *ylab[5] = {"0", "2,5", "5", "7,5", "10"};
    label(p, font.f12, MUTED, CX0 - 8, y - 8, 30, LV_TEXT_ALIGN_RIGHT, ylab[k]);
  }
  label(p, font.f12, MUTED, CX0 - 8, CY1 - 26, 30, LV_TEXT_ALIGN_RIGHT, "kW");
  static const char *xlab[5] = {"00:00", "06:00", "12:00", "18:00", "24:00"};
  for (int h = 0; h <= 4; h++)
    label(p, font.f12, MUTED, chart_x(h * 24), CY0 + 8, 50, LV_TEXT_ALIGN_CENTER, xlab[h]);
  // PV als Fläche aus schmalen Säulen
  for (int i = 0; i < 96; i++) {
    int x0 = chart_x(i), x1 = chart_x(i + 1);
    ui.p2_bars[i] = box(p, x0, CY0, x1 - x0, 0);
    lv_obj_set_style_bg_color(ui.p2_bars[i], hex(C_PV), 0);
    lv_obj_set_style_bg_grad_color(ui.p2_bars[i], hex(0x3A2A08), 0);
    lv_obj_set_style_bg_grad_dir(ui.p2_bars[i], LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(ui.p2_bars[i], 150, 0);
    lv_obj_add_flag(ui.p2_bars[i], LV_OBJ_FLAG_HIDDEN);
  }
  ui.p2_pv_line = line_obj(p, C_PV, 2);
  ui.p2_home_line = line_obj(p, C_HOME, 2);
  ui.p2_soc_line = line_obj(p, C_BAT, 2, 200);
  ui.p2_soc_lbl = label(p, font.f12, C_BAT, 0, 0, 70, LV_TEXT_ALIGN_LEFT, "");
  ui.p2_now = box(p, 0, CY1 - 6, 1, CY0 - CY1 + 6);
  lv_obj_set_style_bg_color(ui.p2_now, hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(ui.p2_now, 90, 0);
  ui.p2_now_lbl = label(p, font.f12, FG, 0, CY1 - 24, 50, LV_TEXT_ALIGN_CENTER, "jetzt");
  // Legende
  const char *leg[3] = {"PV", "Verbrauch", "Akku %"};
  const uint32_t lcol[3] = {C_PV, C_HOME, C_BAT};
  int x = 128;
  for (int i = 0; i < 3; i++) {
    lv_obj_t *b = box(p, x, 431, 14, 3);
    lv_obj_set_style_bg_color(b, hex(lcol[i]), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    label(p, font.f12, MUTED, x + 20, 424, 80, LV_TEXT_ALIGN_LEFT, leg[i]);
    x += i == 0 ? 54 : 96;
  }
}

// ---------------------------------------------------------------------------
// FPS-Anzeige: fertige Bilder pro Sekunde, Zeit pro Bild (Zeichnen + Kopieren),
// Anteil des Bildschirms, der pro Bild neu gezeichnet wird
// ---------------------------------------------------------------------------
inline uint32_t fps_frames = 0, fps_busy = 0, fps_t0 = 0, fps_window = 0, fps_flush = 0, fps_f0 = 0;
inline uint64_t fps_px = 0;

inline void fps_init() {
  lv_display_t *d = lv_display_get_default();
  lv_display_add_event_cb(d, [](lv_event_t *e) {
    fps_t0 = lv_tick_get();
    // zusammengefasste Bereiche, die LVGL in diesem Bild neu zeichnet
    auto *disp = (lv_display_t *) lv_event_get_target(e);
    for (uint32_t i = 0; i < disp->inv_p; i++)
      if (!disp->inv_area_joined[i])
        fps_px += (uint64_t) lv_area_get_width(&disp->inv_areas[i]) * lv_area_get_height(&disp->inv_areas[i]);
  }, LV_EVENT_RENDER_START, nullptr);
  lv_display_add_event_cb(d, [](lv_event_t *) {
    fps_frames++;
    fps_busy += lv_tick_elaps(fps_t0);
  }, LV_EVENT_RENDER_READY, nullptr);
  lv_display_add_event_cb(d, [](lv_event_t *) { fps_f0 = lv_tick_get(); }, LV_EVENT_FLUSH_START, nullptr);
  lv_display_add_event_cb(d, [](lv_event_t *) { fps_flush += lv_tick_elaps(fps_f0); }, LV_EVENT_FLUSH_FINISH, nullptr);
  fps_window = lv_tick_get();
  if (cfg.show_fps) {
    ui.fps = label(lv_layer_top(), font.f12, MUTED, 6, 462, 220, LV_TEXT_ALIGN_LEFT, "-- fps");
  }
}

inline void fps_update() {
  uint32_t ms = lv_tick_elaps(fps_window);
  if (ms < 900)
    return;
  float fps = fps_frames * 1000.0f / ms;
  float per = fps_frames ? (float) fps_busy / fps_frames : 0;
  float copy = fps_frames ? (float) fps_flush / fps_frames : 0;
  float share = fps_frames ? 100.0f * fps_px / fps_frames / (480.0f * 480.0f) : 0;
  char buf[64];
  snprintf(buf, sizeof buf, "%d fps · %.1f ms (Kopie %.1f) · %d %%", (int) lroundf(fps), per, copy,
           (int) lroundf(share));
  dashboard::comma(buf);
  if (ui.fps)
    set_text(ui.fps, buf);
  if (cfg.show_fps)
    ESP_LOGI("dash", "%s", buf);
  fps_frames = fps_busy = fps_flush = 0;
  fps_px = 0;
  fps_window = lv_tick_get();
}

// ---------------------------------------------------------------------------
// Aufbau gesamt
// ---------------------------------------------------------------------------
inline void build(lv_obj_t *page1, lv_obj_t *page2) {
  ui.page = page1;
  ui.page2 = page2;
  for (lv_obj_t *p : {page1, page2}) {
    lv_obj_set_style_bg_color(p, hex(BG), 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(p, 0, 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_remove_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(p, LV_SCROLLBAR_MODE_OFF);
  }
  build_sky(page1);
  build_paths(page1);
  build_nodes(page1);
  build_header(page1);
  build_banner(page1);
  build_burst(page1);
  build_page2(page2);
  for (int i = 0; i < 2; i++) {
    ui.dots[i] = box(lv_layer_top(), 0, 472, 4, 4);
    lv_obj_set_style_radius(ui.dots[i], 2, 0);
    lv_obj_set_style_bg_color(ui.dots[i], hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(ui.dots[i], LV_OPA_COVER, 0);
  }
  fps_init();
  last_tick = esphome::millis();
}

// Startanimation: Leitungen zeichnen sich, dann erscheinen die Knoten nacheinander
inline void start_boot() {
  boot_t = 0;
  booting = true;
  for (lv_obj_t *g : {ui.g_pv, ui.g_grid, ui.g_home, ui.g_bat, ui.g_boil, ui.g_util, ui.bat_eta})
    lv_obj_add_flag(g, LV_OBJ_FLAG_HIDDEN);
  for (auto &pa : paths) {
    pa.shown = 1;
    lv_line_set_points(pa.line, pa.pts.data(), 1);
  }
}

inline void boot_step() {
  if (!booting)
    return;
  float lp = std::clamp((boot_t - 0.15f) / 1.0f, 0.0f, 1.0f);
  for (auto &pa : paths) {
    int n = std::max(1, (int) lroundf(lp * (pa.pts.size() - 1)) + 1);
    if (n != pa.shown) {
      pa.shown = n;
      lv_line_set_points(pa.line, pa.pts.data(), n);
    }
  }
  const float at[] = {0.2f, 0.35f, 0.5f, 0.65f, 0.8f, 0.95f, 0.95f};
  lv_obj_t *g[] = {ui.g_pv, ui.g_grid, ui.g_home, ui.g_bat, ui.g_boil, ui.g_util, ui.bat_eta};
  for (int i = 0; i < 7; i++)
    if (boot_t >= at[i])
      set_visible(g[i], true);
  if (boot_t > 1.3f)
    booting = false;
}

// ---------------------------------------------------------------------------
// Takt (alle ~33 ms, nur wenn Seite 1 sichtbar ist)
// ---------------------------------------------------------------------------
inline void party() {
  ui.shock_t = 0;
  for (size_t i = 0; i < ui.burst.size(); i++) {
    auto &b = ui.burst[i];
    float a = (float) i / ui.burst.size() * 2 * M_PI + (esphome::random_float() - 0.5f) * 0.4f;
    float v = 70 + esphome::random_float() * 150;
    b.x = N_BAT.x;
    b.y = N_BAT.y;
    b.vx = cosf(a) * v;
    b.vy = sinf(a) * v - 40;
    b.life = 1;
  }
}

inline void step_flows(float dt) {
  float pv = std::max(shown.pv, 0.0f);
  auto f = dashboard::split_flows(pv, shown.grid, shown.bat, std::isnan(in.home) ? NAN : shown.home);
  const float power[] = {f.pv_home, f.pv_grid, f.pv_battery, f.grid_home, f.battery_home, f.grid_battery,
                         f.battery_grid, shown.rod};
  bool live = in.online && !booting;
  std::array<int, P_COUNT> active{};
  std::array<uint32_t, P_COUNT> col{};
  for (size_t i = 0; i < flows.size(); i++) {
    auto &fl = flows[i];
    fl.power = live ? power[i] : 0;
    fl.on = fl.on ? fl.power > cfg.threshold * 0.5f : fl.power > cfg.threshold;
    if (fl.on) {
      active[fl.path] = 1;
      col[fl.path] = fl.color;
    }
  }
  // Leitungen: ruhig = schwach, aktiv = hell mit Schein
  for (int p = 0; p < P_COUNT; p++) {
    auto &pa = paths[p];
    // ruhig in der Farbe des ersten Flusses auf dieser Leitung
    uint32_t c = col[p];
    if (!active[p])
      for (auto &fl : flows)
        if (fl.path == p) {
          c = fl.color;
          break;
        }
    // Netz <-> Akku ist selten aktiv und kreuzt unten: ruhig ganz ausgeblendet
    if (pa.state != active[p] || pa.color != c) {
      pa.state = active[p];
      pa.color = c;
      lv_obj_set_style_line_color(pa.line, hex(c), 0);
      lv_obj_set_style_line_opa(pa.line, active[p] ? 110 : (p == P_GRID_BAT ? 0 : 28), 0);
      lv_obj_set_style_line_width(pa.line, active[p] ? 3 : 2, 0);
      lv_obj_set_style_line_color(pa.glow, hex(c), 0);
      lv_obj_set_style_line_opa(pa.glow, active[p] ? 26 : 0, 0);
    }
  }
  // Kometen
  for (auto &fl : flows) {
    auto &pa = paths[fl.path];
    if (!fl.on) {
      for (auto &c : fl.comets) {
        set_visible(c.head, false);
        set_visible(c.tail, false);
      }
      continue;
    }
    float k = std::clamp(fl.power / cfg.full_speed, 0.0f, 1.0f);
    float speed = 28 + 120 * k;
    float spacing = 95 - 61 * sqrtf(k);
    int n = std::clamp((int) (pa.len / spacing), 1, (int) fl.comets.size());
    fl.ofs = fmodf(fl.ofs + speed * dt, pa.len);
    for (int i = 0; i < (int) fl.comets.size(); i++) {
      auto &c = fl.comets[i];
      if (i >= n) {
        set_visible(c.head, false);
        set_visible(c.tail, false);
        continue;
      }
      float d = fmodf(fl.ofs + i * pa.len / n, pa.len);
      auto place = [&](lv_obj_t *o, float dd, int half) {
        if (dd < 0) {
          set_visible(o, false);
          return;
        }
        float x, y;
        path_at(pa, fl.rev ? pa.len - dd : dd, x, y);
        set_pos(o, (int) lroundf(x) - half, (int) lroundf(y) - half);
        set_visible(o, true);
      };
      place(c.head, d, 10);
      place(c.tail, d - 8, 2);
    }
  }
}

inline void step_battery(float dt) {
  float soc = std::clamp(shown.soc, 0.0f, 100.0f);
  bool full = soc >= 99.5f;
  wave_front = fmodf(wave_front + dt * (full ? 6 : 15), 48);
  wave_back = fmodf(wave_back + dt * (full ? 4 : 9), 64);
  // Wasserstand: Kreis r = 43 um (48, 48)
  int level = 48 + 43 - (int) lroundf(86 * soc / 100);
  set_pos(ui.bat_wave_back, 5 - 64 + (int) wave_back, level - 4);
  set_pos(ui.bat_wave_front, 5 - (int) wave_front, level - 3);
  bool charging = shown.bat < -cfg.threshold && in.online;
  static float bub[5] = {0.1f, 0.35f, 0.6f, 0.8f, 0.95f};
  static const float bx[5] = {-22, -9, 4, 15, 26};
  for (int i = 0; i < 5; i++) {
    if (!charging || level > 84) {
      set_visible(ui.bubbles[i], false);
      continue;
    }
    bub[i] += dt * (0.3f + 0.08f * i) * (0.6f + std::clamp(-shown.bat / 4000, 0.0f, 1.0f));
    if (bub[i] > 1)
      bub[i] -= 1;
    int y = 86 - (int) ((86 - level) * bub[i]);
    float half = sqrtf(std::max(0.0f, 40.0f * 40.0f - (y - 48.0f) * (y - 48.0f)));
    int x = 48 + (int) std::clamp(bx[i], -half + 4, half - 4);
    set_pos(ui.bubbles[i], x - 2, y - 2);
    set_visible(ui.bubbles[i], y > level + 3);
  }
}

inline void step_boiler(float dt) {
  bool heating = shown.rod > cfg.threshold && in.online;
  static float sp[4] = {0.1f, 0.4f, 0.65f, 0.9f};
  static const int sx[4] = {-10, -3, 5, 11};
  for (int i = 0; i < 4; i++) {
    if (!heating) {
      set_visible(ui.sparks[i], false);
      continue;
    }
    sp[i] += dt * (0.45f + 0.1f * i);
    if (sp[i] > 1)
      sp[i] -= 1;
    int y = N_BOIL.y + 18 - (int) (sp[i] * 34);
    set_pos(ui.sparks[i], N_BOIL.x + sx[i] - 1, y - 1);
    set_visible(ui.sparks[i], sp[i] < 0.92f);
  }
}

inline void step_burst(float dt) {
  if (ui.shock_t >= 0) {
    ui.shock_t += dt;
    int r = N_BAT.r + (int) (ui.shock_t * 150);
    float a = std::clamp(1 - ui.shock_t / 0.8f, 0.0f, 1.0f);
    lv_obj_set_pos(ui.shock, N_BAT.x - r, N_BAT.y - r);
    lv_obj_set_size(ui.shock, 2 * r, 2 * r);
    lv_obj_set_style_border_opa(ui.shock, (lv_opa_t) (a * 200), 0);
    set_visible(ui.shock, a > 0);
    if (a <= 0)
      ui.shock_t = -1;
  }
  for (auto &b : ui.burst) {
    if (b.life <= 0)
      continue;
    b.vy += 120 * dt;
    b.vx *= 0.985f;
    b.x += b.vx * dt;
    b.y += b.vy * dt;
    b.life -= dt * 0.7f;
    bool vis = b.life > 0 && b.x > 0 && b.x < 478 && b.y > 0 && b.y < 478;
    if (vis)
      set_pos(b.o, (int) b.x - 2, (int) b.y - 2);
    set_visible(b.o, vis);
  }
}

// Zahlen, die weich mitlaufen (bei jedem Takt; Text nur bei Änderung)
inline void step_numbers() {
  char buf[24];
  // PV
  if (shown.pv > 15 || !in.online)
    fmt_power(buf, sizeof buf, shown.pv);
  else
    snprintf(buf, sizeof buf, "ruht");
  set_text(ui.pv_val, buf);
  // Netz
  fmt_power(buf, sizeof buf, shown.grid);
  set_text(ui.grid_val, buf);
  // Haus
  fmt_power(buf, sizeof buf, shown.home);
  set_text(ui.home_val, buf);
  // Akku
  snprintf(buf, sizeof buf, "%d %%", (int) lroundf(std::clamp(shown.soc, 0.0f, 100.0f)));
  set_text(ui.bat_soc, buf);
  if (std::fabs(shown.bat) > cfg.threshold) {
    char p[16];
    fmt_power(p, sizeof p, shown.bat);
    snprintf(buf, sizeof buf, "%s%s", shown.bat < 0 ? "+" : "−", p);
  } else {
    snprintf(buf, sizeof buf, "AKKU");
  }
  set_text(ui.bat_pow, buf);
}

inline void tick() {
  uint32_t now = esphome::millis();
  float dt = std::min((now - last_tick) / 1000.0f, 0.1f);
  last_tick = now;
  boot_t = std::min(boot_t + dt, 99.0f);
  // weich nachführen (Zeitkonstante ~0,4 s)
  float k = 1 - expf(-dt * 2.5f);
  bool on = in.online;
  shown.pv += ((on ? nz(in.pv) : 0) - shown.pv) * k;
  shown.grid += ((on ? nz(in.grid) : 0) - shown.grid) * k;
  shown.bat += ((on ? nz(in.bat) : 0) - shown.bat) * k;
  shown.home += ((on ? nz(in.home) : 0) - shown.home) * k;
  shown.rod += ((on ? nz(in.rod) : 0) - shown.rod) * k;
  shown.soc += (nz(in.soc) - shown.soc) * k;

  // Akku voll -> einmal feiern
  static bool was_full = true;
  bool full = in.online && !std::isnan(in.soc) && shown.soc >= 99.4f;
  if (full && !was_full && !booting)
    party();
  was_full = full || booting || std::isnan(in.soc);

  boot_step();
  step_flows(dt);
  step_battery(dt);
  step_boiler(dt);
  step_burst(dt);
  step_numbers();

  // Hinweis-Punkt pulsiert
  float pulse = 0.6f + 0.4f * sinf(now / 1000.0f * 3);
  lv_obj_set_style_bg_opa(ui.banner_dot, (lv_opa_t) (255 * pulse), 0);
  // Gas/Wasser-Symbol pulsiert, solange der Zähler läuft
  float up = 0.55f + 0.45f * (0.5f + 0.5f * sinf(now / 1000.0f * 4));
  lv_obj_set_style_text_opa(ui.gas_icon, (lv_opa_t) (255 * (dashboard::gas_activity.active() && on ? up : 0.55f)), 0);
  lv_obj_set_style_text_opa(ui.water_icon,
                            (lv_opa_t) (255 * (dashboard::water_activity.active() && on ? up : 0.55f)), 0);
}

// ---------------------------------------------------------------------------
// Langsamer Takt (1 s): Texte, Ringe, Sonne, Stimmung, Hinweis, Verlauf
// ---------------------------------------------------------------------------
struct Hint {
  std::string text;
  uint32_t color;
};
inline std::string hint_text;
inline uint32_t mood_color = 0, mood_candidate = 0;
inline uint32_t mood_since = 0;

inline Hint make_hint() {
  char buf[96], p[16];
  float feed = std::max(-nz(in.grid), 0.0f);
  float pv = nz(in.pv), soc = nz(in.soc);
  if (!in.online)
    return {"Keine Verbindung zu Home Assistant", MUTED};
  if (soc >= 99 && feed > 500) {
    fmt_power(p, sizeof p, feed);
    snprintf(buf, sizeof buf, "Akku voll · %s gehen ins Netz", p);
    return {buf, C_BAT};
  }
  if (feed > 1500) {
    fmt_power(p, sizeof p, feed);
    snprintf(buf, sizeof buf, "%s Überschuss · guter Moment für die Waschmaschine", p);
    return {buf, C_PV};
  }
  if (nz(in.rod) > 300) {
    fmt_power(p, sizeof p, in.rod);
    snprintf(buf, sizeof buf, "Heizstab macht Warmwasser mit %s Sonnenstrom", p);
    return {buf, C_BOIL};
  }
  float today = nz(in.pv_day) + nz(in.hoy_day);
  if (pv > 50 && !std::isnan(in.forecast_today) && in.forecast_today > 0) {
    char a[16], b[16];
    dashboard::fmt_num(a, sizeof a, today, 1, "");
    dashboard::fmt_num(b, sizeof b, in.forecast_today, 1, " kWh");
    snprintf(buf, sizeof buf, "Heute %s von %s Sonne erwartet", a, b);
    return {buf, C_PV};
  }
  if (!std::isnan(in.forecast_tomorrow) && pv <= 50) {
    char b[16];
    dashboard::fmt_num(b, sizeof b, in.forecast_tomorrow, 0, " kWh");
    if (nz(in.bat) > 80)
      snprintf(buf, sizeof buf, "Akku versorgt das Haus · morgen %s Sonne", b);
    else
      snprintf(buf, sizeof buf, "Morgen %s Sonne erwartet", b);
    return {buf, nz(in.bat) > 80 ? C_BAT : C_GRID};
  }
  return {"", MUTED};
}

inline void show_hint(const Hint &h) {
  if (h.text == hint_text)
    return;
  hint_text = h.text;
  if (h.text.empty()) {
    lv_obj_set_y(ui.banner, 480);
    return;
  }
  lv_label_set_text(ui.banner_lbl, h.text.c_str());
  lv_obj_update_layout(ui.banner_lbl);
  lv_point_t sz;
  lv_text_get_size(&sz, h.text.c_str(), font.f14, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  int w = std::min((int) sz.x + 44, 440);
  lv_obj_set_width(ui.banner, w);
  lv_obj_set_x(ui.banner, 240 - w / 2);
  lv_obj_set_width(ui.banner_lbl, w - 36);
  ring_border(ui.banner, h.color, 110, 1);
  lv_obj_set_style_bg_color(ui.banner_dot, hex(h.color), 0);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, ui.banner);
  lv_anim_set_values(&a, 480, 432);
  lv_anim_set_duration(&a, 420);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_set_exec_cb(&a, [](void *o, int32_t v) { lv_obj_set_y((lv_obj_t *) o, v); });
  lv_anim_start(&a);
}

inline void update_sky() {
  int m = now_minutes();
  int rise = 469, set = 1116;
  if (demo_minutes < 0) {
    int64_t r = dashboard::parse_iso(in.next_rising), s = dashboard::parse_iso(in.next_setting);
    if (r > 0) {
      auto t = esphome::ESPTime::from_epoch_local((time_t) r);
      rise = t.hour * 60 + t.minute;
    }
    if (s > 0) {
      auto t = esphome::ESPTime::from_epoch_local((time_t) s);
      set = t.hour * 60 + t.minute;
    }
  }
  bool day = m >= rise && m <= set;
  float s = day ? (float) (m - rise) / std::max(set - rise, 1)
                : (float) ((m - set + 1440) % 1440) / std::max(1440 - set + rise, 1);
  s = std::clamp(s, 0.0f, 1.0f);
  char a[8], b[8];
  dashboard::fmt_clock(a, sizeof a, day ? rise : set);
  dashboard::fmt_clock(b, sizeof b, day ? set : rise);
  set_text(ui.rise_lbl, a);
  set_text(ui.set_lbl, b);
  float th = M_PI * (1 - s);
  int x = (int) lroundf(240 + 190 * cosf(th)), y = (int) lroundf(150 - 105 * sinf(th));
  set_visible(ui.sun, day);
  set_visible(ui.moon, !day);
  if (day)
    set_pos(ui.sun, x - 28, y - 28);
  else
    set_pos(ui.moon, x - 16, y - 16);
  static int last_day = -1;
  if (last_day != (int) day) {
    last_day = day;
    uint32_t c = day ? C_PV : 0x9FB4D6;
    lv_obj_set_style_line_color(ui.arc_full, hex(c), 0);
    lv_obj_set_style_line_color(ui.arc_done, hex(c), 0);
  }
  int n = 1 + (int) lroundf(s * 40);
  static int last_n = -1;
  if (n != last_n) {
    last_n = n;
    lv_line_set_points(ui.arc_done, ui.sky_pts.data(), n);
  }
}

// Stimmung: Farbe oben im Himmel. Wechselt erst, wenn ein Zustand 20 s anhält
// (eine große Fläche neu zu zeichnen kostet einmalig Zeit, ständiges Umfärben ruckelt).
inline void update_mood() {
  float grid = nz(in.grid), bat = nz(in.bat);
  uint32_t c = 0x14171C;
  if (!in.online)
    c = 0x14171C;
  else if (grid < -800)
    c = 0x3A2A0A;  // Einspeisung: golden
  else if (bat > 300)
    c = 0x2E0F24;  // Akku versorgt: magenta
  else if (grid > 300)
    c = 0x0C2238;  // Netzbezug: blau
  else if (nz(in.pv) > 300)
    c = 0x2A220E;  // Sonne deckt das Haus
  uint32_t now = esphome::millis();
  if (c != mood_candidate) {
    mood_candidate = c;
    mood_since = now;
  }
  static bool was_online = false;
  bool first = mood_color == 0 || (in.online && !was_online);
  was_online = in.online;
  if (c != mood_color && (first || now - mood_since > 20000)) {
    mood_color = c;
    lv_obj_set_style_bg_color(ui.sky, hex(c), 0);
  }
}

inline void update_page2() {
  char buf[32];
  float pv_day = nz(in.pv_day) + nz(in.hoy_day);
  dashboard::fmt_num(buf, sizeof buf, pv_day, 1, " kWh");
  set_text(ui.p2_val[0], buf);
  dashboard::fmt_num(buf, sizeof buf, nz(in.home_day), 1, " kWh");
  set_text(ui.p2_val[1], buf);
  float home = nz(in.home_day);
  int aut = home > 0.05f ? (int) lroundf(std::clamp(100 * (1 - nz(in.import_day) / home), 0.0f, 100.0f)) : 0;
  snprintf(buf, sizeof buf, "%d %%", aut);
  set_text(ui.p2_val[2], buf);
  dashboard::fmt_num(buf, sizeof buf, nz(in.export_day), 1, " kWh");
  set_text(ui.p2_val[3], buf);
  if (!std::isnan(in.forecast_today)) {
    char f[16];
    dashboard::fmt_num(f, sizeof f, in.forecast_today, 1, " kWh");
    snprintf(buf, sizeof buf, "Prognose PV %s", f);
    set_text(ui.p2_sub, buf);
  }
  int n = 0, last = -1;
  for (int i = 0; i < 96; i++) {
    if (!hist.valid[i]) {
      set_visible(ui.p2_bars[i], false);
      continue;
    }
    int y = chart_y(hist.pv[i]);
    int h = CY0 - y;
    lv_obj_set_y(ui.p2_bars[i], y);
    lv_obj_set_height(ui.p2_bars[i], h);
    set_visible(ui.p2_bars[i], h > 0);
    float x = chart_x(i + 0.5f);
    ui.p2_pv_pts[n] = {(lv_value_precise_t) x, (lv_value_precise_t) y};
    ui.p2_home_pts[n] = {(lv_value_precise_t) x, (lv_value_precise_t) chart_y(hist.home[i])};
    ui.p2_soc_pts[n] = {(lv_value_precise_t) x, (lv_value_precise_t) chart_ys(hist.soc[i])};
    n++;
    last = i;
  }
  lv_line_set_points(ui.p2_pv_line, ui.p2_pv_pts.data(), n);
  lv_line_set_points(ui.p2_home_line, ui.p2_home_pts.data(), n);
  lv_line_set_points(ui.p2_soc_line, ui.p2_soc_pts.data(), n);
  if (last >= 0) {
    snprintf(buf, sizeof buf, "Akku %d %%", hist.soc[last]);
    set_text(ui.p2_soc_lbl, buf);
    set_pos(ui.p2_soc_lbl, std::min((int) ui.p2_soc_pts[n - 1].x + 6, 400), (int) ui.p2_soc_pts[n - 1].y + 2);
  }
  set_visible(ui.p2_soc_lbl, last >= 0);
  int nx = chart_x(now_minutes() / 15.0f);
  set_pos(ui.p2_now, nx, CY1 - 6);
  set_pos(ui.p2_now_lbl, nx - 25, CY1 - 24);
}

inline void update_dots() {
  bool p2 = lv_screen_active() == ui.page2;
  for (int i = 0; i < 2; i++) {
    bool on = (i == 1) == p2;
    int w = on ? 10 : 4;
    int x = 233 + i * 12 - (on ? 3 : 0);
    if (lv_obj_get_width(ui.dots[i]) != w || lv_obj_get_x(ui.dots[i]) != x) {
      lv_obj_set_size(ui.dots[i], w, 4);
      lv_obj_set_x(ui.dots[i], x);
      lv_obj_set_style_bg_opa(ui.dots[i], on ? 200 : 60, 0);
    }
  }
}

inline void slow_tick();

// Nach einem Seitenwechsel (Tippen): Punkte und Seite 2 sofort aktualisieren
inline void on_page_change() { slow_tick(); }

inline void slow_tick() {
  char buf[48];
  hist_sample();
  fps_update();
  update_dots();
  // Langsame Glättung der Akku-Leistung für die Prognose (~2 min)
  shown.bat_slow += (nz(in.bat) - shown.bat_slow) * 0.02f;
  if (lv_screen_active() == ui.page2) {
    update_page2();
    return;
  }

  // Uhr und Datum
  if (time_valid()) {
    int m = now_minutes();
    dashboard::fmt_clock(buf, sizeof buf, m);
    set_text(ui.clock, buf);
    if (demo_minutes < 0) {
      auto t = now_fn();
      static const char *const wd[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
      static const char *const mo[] = {"Jan", "Feb", "Mär", "Apr", "Mai", "Jun",
                                       "Jul", "Aug", "Sep", "Okt", "Nov", "Dez"};
      snprintf(buf, sizeof buf, "%s, %d. %s", wd[(t.day_of_week + 6) % 7], t.day_of_month, mo[(t.month + 11) % 12]);
      set_text(ui.date, buf);
    }
  }

  // Geld: eigener Verbrauch spart den Bezugspreis, Einspeisung bringt die Vergütung
  float home_day = nz(in.home_day), imp = nz(in.import_day), exp = nz(in.export_day);
  float saved = std::max(home_day - imp, 0.0f) * cfg.price_import + exp * cfg.price_export;
  snprintf(buf, sizeof buf, "+%.2f €", saved);
  dashboard::comma(buf);
  set_text(ui.money, buf);

  // Haus: Autarkie und Tagesmix PV/Akku/Netz
  int aut = home_day > 0.05f ? (int) lroundf(std::clamp(100 * (1 - imp / home_day), 0.0f, 100.0f)) : 0;
  snprintf(buf, sizeof buf, "Autarkie %d %%", aut);
  set_text(ui.home_aut, buf);
  lv_obj_set_style_text_color(ui.home_aut, hex(aut >= 90 ? C_HOME : MUTED), 0);
  float pv_total = nz(in.pv_day) + nz(in.hoy_day);
  float bat_share = std::max(nz(in.discharge_day), 0.0f);
  float pv_share = std::clamp(pv_total - exp - nz(in.charge_day), 0.0f, home_day);
  if (home_day > 0.01f) {
    int a_pv = (int) (pv_share / home_day * 360), a_bat = (int) (bat_share / home_day * 360);
    a_pv = std::min(a_pv, 360);
    a_bat = std::min(a_bat, 360 - a_pv);
    const int gap = 3;
    arc_span(ui.home_mix[0], gap, a_pv - gap);
    arc_span(ui.home_mix[1], a_pv + gap, a_pv + a_bat - gap);
    arc_span(ui.home_mix[2], a_pv + a_bat + gap, 360 - gap);
  } else {
    for (auto *a : ui.home_mix)
      set_visible(a, false);
  }

  // PV: Ertrag heute, Ring gegen Prognose
  dashboard::fmt_num(buf, sizeof buf, pv_total, 1, " kWh");
  set_text(ui.pv_day, buf);
  if (!std::isnan(in.forecast_today) && in.forecast_today > 0)
    arc_span(ui.pv_ring, 0, (int) (std::clamp(pv_total / in.forecast_today, 0.0f, 1.0f) * 360));
  else
    set_visible(ui.pv_ring, false);

  // Netz: Richtung und Tageswerte
  float grid = nz(in.grid);
  bool imp_now = in.online && grid > cfg.threshold, exp_now = in.online && grid < -cfg.threshold;
  uint32_t gc = imp_now ? C_GRID : exp_now ? C_EXP : FAINT;
  set_text(ui.grid_state, imp_now ? "BEZUG" : exp_now ? "EINSPEISUNG" : "RUHT");
  lv_obj_set_style_text_color(ui.grid_state, hex(gc), 0);
  lv_obj_set_style_text_color(ui.grid_icon, hex(gc), 0);
  lv_obj_set_style_border_color(ui.grid_disc, hex(gc), 0);
  dashboard::fmt_num(buf, sizeof buf, imp, 1, "");
  set_text(ui.grid_in, buf);
  dashboard::fmt_num(buf, sizeof buf, exp, 1, " kWh");
  set_text(ui.grid_out, buf);

  // Akku-Prognose
  dashboard::battery_eta(buf, sizeof buf, in.soc, shown.bat_slow, cfg.capacity_kwh, cfg.reserve, now_minutes());
  set_text(ui.bat_eta, buf);
  lv_obj_set_style_text_color(ui.bat_eta, hex(nz(in.soc) >= 99.5f ? C_BAT : MUTED), 0);

  // Warmwasser: Farbe nach Temperatur (40 °C blau ... 65 °C orange)
  if (!std::isnan(in.boiler_temp)) {
    snprintf(buf, sizeof buf, "%d°", (int) lroundf(in.boiler_temp));
    set_text(ui.boil_temp, buf);
    float t = std::clamp((in.boiler_temp - 40) / 25, 0.0f, 1.0f);
    uint8_t r = (uint8_t) (46 + (255 - 46) * t), g = (uint8_t) (140 + (106 - 140) * t), b = (uint8_t) (255 + (43 - 255) * t);
    uint32_t c = (r << 16) | (g << 8) | b;
    static uint32_t last_c = 0;
    if (c != last_c) {
      last_c = c;
      // Farbe etwa 30 % gedeckt über dem dunklen Grund
      lv_color_t mixed = lv_color_mix(hex(c), hex(DISC), 90);
      lv_obj_set_style_bg_color(ui.boil_disc, mixed, 0);
      lv_obj_set_style_border_color(ui.boil_disc, hex(c), 0);
    }
  }
  bool heating = in.online && nz(in.rod) > cfg.threshold;
  if (heating) {
    fmt_power(buf, sizeof buf, in.rod);
  } else if (nz(in.rod_day) > 0.05f) {
    dashboard::fmt_num(buf, sizeof buf, in.rod_day, 1, " kWh");
  } else {
    snprintf(buf, sizeof buf, "aus");
  }
  set_text(ui.boil_rod, buf);
  lv_obj_set_style_text_color(ui.boil_rod, hex(heating ? C_BOIL : MUTED), 0);

  // Gas und Wasser
  dashboard::fmt_num(buf, sizeof buf, nz(in.gas_day), 2, " m³");
  set_text(ui.gas_val, buf);
  dashboard::fmt_num(buf, sizeof buf, nz(in.water_day), 2, " m³");
  set_text(ui.water_val, buf);

  update_sky();
  update_mood();
  if (!booting && boot_t > 2.0f)
    show_hint(make_hint());
}

// Für die Vorschau: Tagesverlauf bis `minutes` mit Beispielwerten füllen
inline void demo_history(int minutes) {
  hist_loaded = true;
  hist_clear(1);
  float soc = 24, cap = cfg.capacity_kwh;
  for (int i = 0; i < 96 && i * 15 < minutes; i++) {
    float t = i * 0.25f;
    float pv = 0;
    if (t > 7.82f && t < 18.6f) {
      pv = 9400 * powf(sinf(M_PI * (t - 7.82f) / 10.78f), 1.35f);
      pv *= 1 - 0.45f * expf(-(t - 10.6f) * (t - 10.6f) / 0.06f) - 0.3f * expf(-(t - 15.3f) * (t - 15.3f) / 0.12f);
    }
    float home = 360 + 1700 * expf(-(t - 6.9f) * (t - 6.9f) / 0.12f) + (t > 11.5f && t < 13.6f ? 2100 : 0) +
                 1300 * expf(-(t - 18.6f) * (t - 18.6f) / 0.3f) + 500 * expf(-(t - 20.5f) * (t - 20.5f) / 1.2f);
    float net = pv - home;
    if (net > 0)
      soc = std::min(100.0f, soc + std::min(net, 5000.0f) * 0.25f / 1000 / cap * 100);
    else
      soc = std::max(cfg.reserve, soc - std::min(-net, 5000.0f) * 0.25f / 1000 / cap * 100);
    hist.pv[i] = (uint16_t) pv;
    hist.home[i] = (uint16_t) home;
    hist.soc[i] = (uint8_t) soc;
    hist.valid[i] = 1;
  }
}

}  // namespace dash
