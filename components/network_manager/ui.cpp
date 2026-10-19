#include "network_manager/api.hpp"
#include "lvgl.h"
namespace network_manager {
namespace {
lv_obj_t *card = nullptr;
lv_obj_t *key_label = nullptr;
unsigned displayed = 0;
void dismiss(lv_event_t *) {
  if (key_label) { auto *text = lv_label_get_text(key_label); wipe(text, std::strlen(text)); key_label = nullptr; }
  if (card) { lv_obj_del(card); card = nullptr; }
}
lv_obj_t *label(const char *text, int y, const lv_font_t *font) {
  auto *l = lv_label_create(card); lv_label_set_text(l, text); lv_obj_set_pos(l, 10, y);
  lv_obj_set_width(l, 140); lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(l, font, 0); lv_obj_set_style_text_color(l, lv_color_hex(0xF3F5F2), 0);
  return l;
}
}
void ui_tick() {
  LocalSetup s{}; if (!local_setup(s)) return;
  if (!s.active) { dismiss(nullptr); displayed = 0; wipe(&s, sizeof s); return; }
  if (displayed != s.show) {
    dismiss(nullptr); displayed = s.show;
    card = lv_obj_create(lv_layer_top()); lv_obj_remove_style_all(card);
    lv_obj_set_pos(card, 6, 148); lv_obj_set_size(card, 160, 268);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1C292E), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0); lv_obj_set_style_radius(card, 6, 0);
    label("WI-FI SETUP", 12, &lv_font_montserrat_14);
    label("Connect to this AP:", 42, &lv_font_montserrat_12);
    lv_label_set_text_fmt(label("", 62, &lv_font_montserrat_12), "REMOTE-01-Setup-\n%.4s", s.ssid + std::strlen(s.ssid) - 4);
    label("AP key: join both lines", 102, &lv_font_montserrat_12);
    key_label = label("", 122, &lv_font_montserrat_12);
    lv_label_set_text_fmt(key_label, "%.8s\n%.8s", s.key, s.key + 8);
    label("Open in browser:", 164, &lv_font_montserrat_12);
    label("http://192.168.4.1", 184, &lv_font_montserrat_12);
    auto *b = lv_btn_create(card); lv_obj_set_pos(b, 10, 216); lv_obj_set_size(b, 140, 36);
    lv_obj_add_event_cb(b, dismiss, LV_EVENT_CLICKED, nullptr);
    auto *l = lv_label_create(b); lv_label_set_text(l, "Continue offline");
    lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0); lv_obj_center(l);
  }
  wipe(&s, sizeof s);
}
}
