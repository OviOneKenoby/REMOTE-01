#include "user_app.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "gui_guider.h"
#include "lcd_bl_pwm_bsp.h"
#include "control_home/api.hpp"
#include "power_management/api.hpp"
#include "network_manager/presentation.hpp"
#include "encoder_volume.hpp"
#include "button_inputs.hpp"
#include "driver/gpio.h"
#include "esp_timer.h"
#include <atomic>
#include <cstdio>
#include <cstring>

static lv_ui src_ui;

struct HomeWidgets {
	lv_obj_t *battery = nullptr, *status = nullptr, *title = nullptr, *subtitle = nullptr;
	lv_obj_t *playback = nullptr, *volume = nullptr, *level = nullptr;
	lv_obj_t *ip = nullptr;
	lv_obj_t *battery_icon = nullptr, *battery_tip = nullptr, *plug = nullptr;
	lv_obj_t *buttons[3]{}, *symbols[3]{};
	lv_timer_t *timer = nullptr;
	lv_obj_t *page = nullptr;
	std::uint64_t generation = 0;
} home;

namespace {
constexpr gpio_num_t encoder_a = GPIO_NUM_2;
constexpr gpio_num_t encoder_b = GPIO_NUM_5;
constexpr gpio_num_t button_pins[] = {GPIO_NUM_38, GPIO_NUM_39, GPIO_NUM_40,
                                      GPIO_NUM_41};
std::atomic<bool> encoder_home{false};

void button_event(remote_buttons::Id) {
	// Owner-approved physical identities are intentionally unassigned in Stage 5.
}

void encoder_task(void *) {
	remote_encoder::Router router;
	remote_encoder::View published{};
	remote_buttons::Router buttons;
	while (true) {
		const auto now = std::uint64_t(esp_timer_get_time() / 1000);
		router.sample(now,
		              gpio_get_level(encoder_a) != 0,
		              gpio_get_level(encoder_b) != 0,
		              encoder_home.load(std::memory_order_relaxed),
		              [](control_home::Action action) {
			              return control_home::request(action);
		              });
		const auto encoder = router.view();
		if (!(encoder == published)) {
			published = encoder;
			control_home::record_encoder_state(encoder.transitions, encoder.detents,
			                                   encoder.invalid, encoder.positive,
			                                   encoder.negative, encoder.rejected,
			                                   encoder.sample_last_ms,
			                                   encoder.sample_max_ms,
			                                   encoder.last_delta);
		}
		std::uint8_t pressed = 0;
		for (unsigned i = 0; i < 4; ++i)
			if (gpio_get_level(button_pins[i]) == 0) pressed |= std::uint8_t(1u << i);
		buttons.sample(now, pressed, button_event);
		vTaskDelay(pdMS_TO_TICKS(2));
	}
}
}

static void home_text(lv_obj_t *label, const char *text)
{
	if (std::strcmp(lv_label_get_text(label), text) != 0) lv_label_set_text(label, text);
}

static void home_click(lv_event_t *event)
{
	// Bounded intent enqueue only. Core derives PLAY/PAUSE on its own owner.
	const auto action = static_cast<control_home::Action>(reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
	(void)control_home::request(action); // Full queue rejects this press; no retry/optimism.
}

static void home_refresh(lv_timer_t *)
{
	encoder_home.store(home.page &&
	                   lv_carousel_get_element_act(src_ui.screen_carousel_1) ==
	                       home.page,
	                   std::memory_order_relaxed);
	network_manager::Status network{};
	(void)network_manager::read_status(network); // Never wait for NETWORK/HTTP.
	const auto ip = network_manager::ip_text(network);
	home_text(home.ip, ip.text);
	home_text(src_ui.screen_label_21, ip.text); // Factory page0's existing blank row.
	const auto battery = power_management::battery_view();
	char voltage[6];
	if (battery.simulated) std::snprintf(voltage, sizeof voltage, "SIM");
	else if (battery.valid) std::snprintf(voltage, sizeof voltage, "%u.%uV", (battery.millivolts + 50) / 1000 % 10, ((battery.millivolts + 50) / 100) % 10);
	else std::snprintf(voltage, sizeof voltage, "--");
	home_text(home.battery, voltage);
	const bool plugged=power_management::external_plugged_in(power_management::external_view());
	if(plugged) {
		lv_obj_clear_flag(home.plug,LV_OBJ_FLAG_HIDDEN);
		lv_obj_add_flag(home.battery_icon,LV_OBJ_FLAG_HIDDEN);
		lv_obj_add_flag(home.battery_tip,LV_OBJ_FLAG_HIDDEN);
	} else {
		lv_obj_add_flag(home.plug,LV_OBJ_FLAG_HIDDEN);
		lv_obj_clear_flag(home.battery_icon,LV_OBJ_FLAG_HIDDEN);
		lv_obj_clear_flag(home.battery_tip,LV_OBJ_FLAG_HIDDEN);
	}
	control_home::Snapshot s;
	if (!control_home::read(s, home.generation)) return;
	using control_home::Playback;
	using control_home::Connection;
	home_text(home.status, s.connection == Connection::CONNECTED ? "CONNECTED" :
	                      s.connection == Connection::UNAVAILABLE ? "UNAVAILABLE" : "UNKNOWN");
	// CCM retains media.title=StreamTitle and media.subtitle=station. HOME's
	// accepted visual hierarchy presents the station in the large upper label.
	home_text(home.title, s.subtitle[0] ? s.subtitle : "--");
	home_text(home.subtitle, s.title[0] ? s.title : "--");
	home_text(home.playback, s.playback == Playback::PLAYING ? LV_SYMBOL_PLAY " PLAYING" :
	                        s.playback == Playback::PAUSED ? LV_SYMBOL_PAUSE " PAUSED" :
	                        s.playback == Playback::CONFLICT ? "CONFLICT" :
	                        s.playback == Playback::OTHER ? "NOT READY" : "UNKNOWN");
	home_text(home.symbols[1], s.playback == Playback::PLAYING ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
	char volume[8];
	if (s.volume_known && s.desired_volume_known && s.desired_volume != s.volume)
		std::snprintf(volume, sizeof volume, "%u>%u", unsigned(s.volume), unsigned(s.desired_volume));
	else if (s.volume_known) std::snprintf(volume, sizeof volume, "%u", unsigned(s.volume));
	else std::snprintf(volume, sizeof volume, "--");
	home_text(home.volume, volume);
	const lv_coord_t width = s.volume_known ? (148 * s.volume + 50) / 100 : 0;
	if (lv_obj_get_width(home.level) != width) lv_obj_set_width(home.level, width);
	const bool enabled[] = {s.previous, s.toggle, s.next};
	for (unsigned i = 0; i < 3; ++i) {
		if (enabled[i]) lv_obj_clear_state(home.buttons[i], LV_STATE_DISABLED);
		else lv_obj_add_state(home.buttons[i], LV_STATE_DISABLED);
	}
}

static void home_delete(lv_event_t *)
{
	if (home.timer) lv_timer_del(home.timer);
	home = {}; // No retained objects/timer survive page destruction.
}

static void remote01_mockup_create(void);
static void example_color_task(void *arg);
static void lvgl_obj_event_callback(lv_event_t *event);

static lv_obj_t *remote01_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                              lv_color_t color, lv_coord_t x, lv_coord_t y)
{
	lv_obj_t *label = lv_label_create(parent);
	lv_label_set_text(label, text);
	lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
	lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
	lv_obj_set_pos(label, x, y);
	lv_obj_set_size(label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
	lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
	return label;
}

static lv_obj_t *remote01_panel(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                               lv_coord_t width, lv_coord_t height, lv_color_t color, lv_coord_t radius)
{
	lv_obj_t *obj = lv_obj_create(parent);
	lv_obj_remove_style_all(obj);
	lv_obj_set_pos(obj, x, y);
	lv_obj_set_size(obj, width, height);
	lv_obj_set_style_bg_color(obj, color, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_radius(obj, radius, LV_PART_MAIN);
	lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
	return obj;
}

static lv_obj_t *remote01_single_line(lv_obj_t *parent, const char *text, const lv_font_t *font,
                                     lv_color_t color, lv_coord_t x, lv_coord_t y, lv_coord_t width,
                                     lv_label_long_mode_t mode, lv_text_align_t align)
{
	lv_obj_t *label = remote01_label(parent, text, font, color, x, y);
	lv_label_set_long_mode(label, mode);
	lv_obj_set_size(label, width, font->line_height);
	lv_obj_set_style_text_align(label, align, LV_PART_MAIN);
	return label;
}

static void remote01_mockup_create(void)
{
	// Accepted HOME geometry; values arrive only as copied Core presentation.
	const lv_color_t background = lv_color_hex(0x11191d);
	const lv_color_t panel = lv_color_hex(0x1c292e);
	const lv_color_t text = lv_color_hex(0xf3f5f2);
	const lv_color_t muted = lv_color_hex(0x91a3a6);
	const lv_color_t accent = lv_color_hex(0xc2ff00);
	const lv_color_t track = lv_color_hex(0x344247);
	lv_obj_t *page = lv_carousel_add_element(src_ui.screen_carousel_1, 2);
	home.page = page;
	lv_obj_set_style_bg_color(page, background, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(page, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_border_width(page, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(page, 0, LV_PART_MAIN);

	remote01_label(page, "REMOTE//01", &lv_font_montserrat_12, text, 12, 10);
	home.battery = remote01_single_line(page, "--", &lv_font_montserrat_12, text, 122, 10, 38,
	                     LV_LABEL_LONG_CLIP, LV_TEXT_ALIGN_RIGHT);
	lv_obj_t *battery = home.battery_icon = remote01_panel(page, 104, 13, 13, 8, background, 1);
	lv_obj_set_style_bg_opa(battery, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(battery, 1, LV_PART_MAIN);
	lv_obj_set_style_border_color(battery, text, LV_PART_MAIN);
	lv_obj_set_style_border_opa(battery, LV_OPA_COVER, LV_PART_MAIN);
	home.battery_tip = remote01_panel(page, 117, 15, 2, 4, text, 0);
	home.plug = remote01_single_line(page, LV_SYMBOL_USB, &lv_font_montserrat_12, text, 104, 10, 16,
	                                LV_LABEL_LONG_CLIP, LV_TEXT_ALIGN_CENTER);
	lv_obj_add_flag(home.plug,LV_OBJ_FLAG_HIDDEN);
	remote01_panel(page, 12, 32, 148, 1, track, 0);

	remote01_label(page, "TUNER//01", &lv_font_montserrat_16, text, 12, 46);
	home.status = remote01_single_line(page, "UNKNOWN", &lv_font_montserrat_12, accent, 12, 68, 148,
	                                  LV_LABEL_LONG_CLIP, LV_TEXT_ALIGN_LEFT);

	lv_obj_t *artwork = remote01_panel(page, 12, 96, 148, 148, panel, 6);
	lv_obj_t *device = remote01_label(artwork, "TUNER//01", &lv_font_montserrat_20, text, 0, 0);
	lv_obj_align(device, LV_ALIGN_CENTER, 0, -10);
	lv_obj_t *artwork_caption = remote01_label(artwork, "ARTWORK", &lv_font_montserrat_12, muted, 0, 0);
	lv_obj_align(artwork_caption, LV_ALIGN_CENTER, 0, 18);

	home.title = remote01_single_line(page, "--", &lv_font_montserrat_20, text, 12, 262, 148,
	                     LV_LABEL_LONG_DOT, LV_TEXT_ALIGN_LEFT);
	home.subtitle = remote01_single_line(page, "--", &lv_font_montserrat_14, muted, 12, 290, 148,
	                     LV_LABEL_LONG_DOT, LV_TEXT_ALIGN_LEFT);
	home.playback = remote01_single_line(page, "UNKNOWN", &lv_font_montserrat_12, accent, 12, 316, 148,
	                                    LV_LABEL_LONG_CLIP, LV_TEXT_ALIGN_LEFT);

	static const char *playback_symbols[] = {LV_SYMBOL_PREV, LV_SYMBOL_PAUSE, LV_SYMBOL_NEXT};
	for (int i = 0; i < 3; ++i) {
		lv_obj_t *button = remote01_panel(page, 12 + i * 52, 350, 44, 52, panel, 6);
		lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
		home.buttons[i] = button;
		lv_obj_set_style_opa(button, LV_OPA_40, LV_STATE_DISABLED);
		lv_obj_add_state(button, LV_STATE_DISABLED);
		lv_obj_add_event_cb(button, home_click, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<std::uintptr_t>(i)));
		lv_obj_t *symbol = remote01_label(button, playback_symbols[i], &lv_font_montserrat_20,
		                                 i == 1 ? accent : text, 0, 0);
		lv_obj_center(symbol);
		home.symbols[i] = symbol;
	}

	remote01_label(page, "VOLUME", &lv_font_montserrat_12, muted, 12, 424);
	home.volume = remote01_single_line(page, "--", &lv_font_montserrat_20, text, 96, 418, 64,
	                     LV_LABEL_LONG_CLIP, LV_TEXT_ALIGN_RIGHT);
	lv_obj_t *volume_track = remote01_panel(page, 12, 450, 148, 6, track, 3);
	home.level = remote01_panel(volume_track, 0, 0, 0, 6, accent, 3);

	static const char *navigation[] = {"FAVORITES", "DEVICES", "REMOTES"};
	for (int i = 0; i < 3; ++i) {
		lv_obj_t *row = remote01_panel(page, 12, 486 + i * 40, 148, 36, panel, 4);
		lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
		lv_obj_t *name = remote01_label(row, navigation[i], &lv_font_montserrat_14, text, 0, 0);
		lv_obj_align(name, LV_ALIGN_LEFT_MID, 10, 0);
		lv_obj_t *arrow = remote01_label(row, LV_SYMBOL_RIGHT, &lv_font_montserrat_14, text, 0, 0);
		lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -10, 0);
	}

	home.ip = remote01_single_line(page, "--", &lv_font_montserrat_12, muted, 12, 618, 124,
	                              LV_LABEL_LONG_CLIP, LV_TEXT_ALIGN_LEFT);
	lv_obj_t *wifi = remote01_label(page, LV_SYMBOL_WIFI, &lv_font_montserrat_12, muted, 0, 618);
	lv_obj_align(wifi, LV_ALIGN_TOP_RIGHT, -12, 618);
	lv_obj_add_event_cb(page, home_delete, LV_EVENT_DELETE, nullptr);
	// Existing LVGL owner calls this timer; it reads a coalesced mailbox, never Core.
	home.timer = lv_timer_create(home_refresh, 20, nullptr);
}

static void example_color_task(void *arg)
{
	lv_ui *ui = (lv_ui *)arg;
	lv_obj_clear_flag(ui->screen_carousel_1, LV_OBJ_FLAG_SCROLLABLE);

	lv_obj_clear_flag(ui->screen_cont_1, LV_OBJ_FLAG_HIDDEN);
	lv_obj_add_flag(ui->screen_cont_2, LV_OBJ_FLAG_HIDDEN);
	lv_obj_add_flag(ui->screen_cont_3, LV_OBJ_FLAG_HIDDEN);

	lv_obj_clear_flag(ui->screen_img_1, LV_OBJ_FLAG_HIDDEN);
	lv_obj_add_flag(ui->screen_img_2, LV_OBJ_FLAG_HIDDEN);
	lv_obj_add_flag(ui->screen_img_3, LV_OBJ_FLAG_HIDDEN);
	vTaskDelay(pdMS_TO_TICKS(1500));
	lv_obj_clear_flag(ui->screen_img_2, LV_OBJ_FLAG_HIDDEN);
	lv_obj_add_flag(ui->screen_img_1, LV_OBJ_FLAG_HIDDEN);
	lv_obj_add_flag(ui->screen_img_3, LV_OBJ_FLAG_HIDDEN);
	vTaskDelay(pdMS_TO_TICKS(1500));
	lv_obj_clear_flag(ui->screen_img_3, LV_OBJ_FLAG_HIDDEN);
	lv_obj_add_flag(ui->screen_img_2, LV_OBJ_FLAG_HIDDEN);
	lv_obj_add_flag(ui->screen_img_1, LV_OBJ_FLAG_HIDDEN);
	vTaskDelay(pdMS_TO_TICKS(1500));

	lv_obj_clear_flag(ui->screen_cont_2, LV_OBJ_FLAG_HIDDEN);
	lv_obj_add_flag(ui->screen_cont_1, LV_OBJ_FLAG_HIDDEN);
	lv_obj_add_flag(ui->screen_cont_3, LV_OBJ_FLAG_HIDDEN);
	lv_obj_add_flag(ui->screen_carousel_1, LV_OBJ_FLAG_SCROLLABLE);
	vTaskDelete(NULL);
}

static void lvgl_obj_event_callback(lv_event_t *event)
{
	if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
		lv_obj_t *slider = lv_event_get_current_target(event);
		setUpduty(0xff - lv_slider_get_value(slider));
	}
}

void user_app_init(void)
{
	setup_ui(&src_ui);
	// Existing blank factory status row, no added container or touch target.
	lv_obj_set_style_text_font(src_ui.screen_label_21, &lv_font_montserrat_12, LV_PART_MAIN);
	lv_label_set_long_mode(src_ui.screen_label_21, LV_LABEL_LONG_CLIP);
	lv_obj_set_size(src_ui.screen_label_21, 162, lv_font_montserrat_12.line_height);
	lv_label_set_text(src_ui.screen_label_21, "--");
	remote01_mockup_create();
	lv_obj_set_element_id(src_ui.screen_carousel_1, 0, LV_ANIM_OFF);
	lcd_bl_pwm_bsp_init(LCD_PWM_MODE_255);
	lv_obj_add_event_cb(src_ui.screen_slider_1, lvgl_obj_event_callback, LV_EVENT_ALL, NULL);
	gpio_config_t encoder{};
	encoder.pin_bit_mask = (1ULL << encoder_a) | (1ULL << encoder_b);
	encoder.mode = GPIO_MODE_INPUT;
	encoder.pull_up_en = GPIO_PULLUP_ENABLE;
	encoder.pull_down_en = GPIO_PULLDOWN_DISABLE;
	encoder.intr_type = GPIO_INTR_DISABLE;
	ESP_ERROR_CHECK(gpio_config(&encoder));
	gpio_config_t buttons{};
	buttons.pin_bit_mask = (1ULL << GPIO_NUM_38) | (1ULL << GPIO_NUM_39) |
	                       (1ULL << GPIO_NUM_40) | (1ULL << GPIO_NUM_41);
	buttons.mode = GPIO_MODE_INPUT;
	buttons.pull_up_en = GPIO_PULLUP_DISABLE; // Board R33-R36 are fitted 10 kOhm pull-ups.
	buttons.pull_down_en = GPIO_PULLDOWN_DISABLE;
	buttons.intr_type = GPIO_INTR_DISABLE;
	ESP_ERROR_CHECK(gpio_config(&buttons));
	(void)xTaskCreate(encoder_task, "encoder_input", 3072, nullptr, 2, nullptr);
	xTaskCreate(example_color_task, "example_color_task", 4096, &src_ui, 2, NULL);
}
