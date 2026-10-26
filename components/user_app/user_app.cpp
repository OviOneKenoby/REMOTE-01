#include "user_app.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "gui_guider.h"
#include "lcd_bl_pwm_bsp.h"
#include "control_home/api.hpp"
#include "control_home/full_control.hpp"
#include "control_home/full_control_navigation.hpp"
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

struct TunerControlWidgets {
	lv_obj_t *view = nullptr, *radio = nullptr, *bluetooth = nullptr;
	lv_obj_t *menu_status = nullptr;
	lv_obj_t *popup = nullptr, *popup_text = nullptr, *spinner = nullptr;
	lv_obj_t *popup_confirm = nullptr, *popup_cancel = nullptr, *popup_ok = nullptr;
	lv_obj_t *main_rows[4]{};
	lv_obj_t *list_panel = nullptr, *list_title = nullptr, *list_area = nullptr;
	lv_obj_t *empty_text = nullptr, *list_rows[27]{}, *list_labels[27]{};
	char row_names[27][49]{};
	lv_timer_t *timer = nullptr;
	control_home::FullControlSnapshot state{};
	control_home::CatalogSnapshot catalog{};
	control_home::MenuSnapshot menu{};
	control_home::Snapshot playback{};
	control_home::navigation::Model navigation{};
	control_home::navigation::Gesture gestures[27]{};
	control_home::BrowserSnapshot browser{};
	std::uint64_t generation = 0;
	std::uint64_t catalog_generation = 0, browser_generation = 0, playback_generation = 0,
	              menu_generation = 0;
	std::uint64_t catalog_revision = 0, browser_revision = 0, browser_session = 0;
	std::uint64_t browser_sequence_applied = 0;
	std::uint32_t popup_until = 0;
	std::uint32_t browser_poll_at = 0;
	std::uint16_t pending_token = 0;
	std::uint8_t row_count = 0;
	std::uint8_t row_kind[27]{}, country_filter = 0;
	std::uint16_t browser_page_start = 0;
	control_home::CatalogOperation pending_operation = control_home::CatalogOperation::PLAY;
	control_home::BrowserOperation pending_browser_operation = control_home::BrowserOperation::START;
	control_home::CatalogCollection collection = control_home::CatalogCollection::SAVED;
	bool awaiting_action = false, awaiting_playback = false;
	bool return_home_after_popup = false;
	bool delete_confirmation = false, disconnect_popup = false;
	bool source_result_handled = false;
	bool visible = false, last_commands_allowed = true;
} tuner_control;

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
		              },
		              [] { power_management::activity(); });
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

static void home_scrolling_text(lv_obj_t *label, const char *text, bool active)
{
	const auto wanted = active ? LV_LABEL_LONG_SCROLL : LV_LABEL_LONG_CLIP;
	const bool changed = std::strcmp(lv_label_get_text(label), text) != 0;
	if (!changed && lv_label_get_long_mode(label) == wanted) return;
	// Switching modes deletes LVGL's old offset animation and resets its offset,
	// so replacement metadata always starts with its first character visible.
	lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
	if (changed) lv_label_set_text(label, text);
	if (!active) return;
	lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL);
	if (auto *animation = lv_anim_get(label, nullptr)) {
		lv_anim_set_delay(animation, 1000);
		lv_anim_set_playback_delay(animation, 1000);
		lv_anim_set_repeat_delay(animation, 1000);
	}
}

static void home_click(lv_event_t *event)
{
	// Bounded intent enqueue only. Core derives PLAY/PAUSE on its own owner.
	const auto action = static_cast<control_home::Action>(reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
	(void)control_home::request(action); // Full queue rejects this press; no retry/optimism.
}

static void tuner_control_close()
{
	if (!tuner_control.view) return;
	if (tuner_control.browser_session)
		(void)control_home::request_browser_action(control_home::BrowserOperation::CANCEL,
			0, tuner_control.browser_session, 0);
	lv_obj_add_flag(tuner_control.view, LV_OBJ_FLAG_HIDDEN);
	tuner_control.visible = false;
	tuner_control.navigation.exit();
	tuner_control.popup_until = 0;
	tuner_control.return_home_after_popup = false;
	tuner_control.awaiting_action = false;
	tuner_control.awaiting_playback = false;
	tuner_control.browser_session = tuner_control.browser_revision = 0;
	lv_obj_add_flag(tuner_control.popup, LV_OBJ_FLAG_HIDDEN);
}

static void tuner_control_popup(const char *text, bool busy,
	                            bool confirm = false, bool ok = false)
{
	home_text(tuner_control.popup_text, text);
	if (busy) lv_obj_clear_flag(tuner_control.spinner, LV_OBJ_FLAG_HIDDEN);
	else lv_obj_add_flag(tuner_control.spinner, LV_OBJ_FLAG_HIDDEN);
	if (confirm) {
		lv_obj_clear_flag(tuner_control.popup_confirm, LV_OBJ_FLAG_HIDDEN);
		lv_obj_clear_flag(tuner_control.popup_cancel, LV_OBJ_FLAG_HIDDEN);
	} else {
		lv_obj_add_flag(tuner_control.popup_confirm, LV_OBJ_FLAG_HIDDEN);
		lv_obj_add_flag(tuner_control.popup_cancel, LV_OBJ_FLAG_HIDDEN);
	}
	if (ok) lv_obj_clear_flag(tuner_control.popup_ok, LV_OBJ_FLAG_HIDDEN);
	else lv_obj_add_flag(tuner_control.popup_ok, LV_OBJ_FLAG_HIDDEN);
	lv_obj_clear_flag(tuner_control.popup, LV_OBJ_FLAG_HIDDEN);
	lv_obj_move_foreground(tuner_control.popup);
}

static void tuner_control_popup_hide()
{
	lv_obj_add_flag(tuner_control.popup, LV_OBJ_FLAG_HIDDEN);
	tuner_control.delete_confirmation = false;
	tuner_control.disconnect_popup = false;
	tuner_control.popup_until = 0;
}

static const char *source_error(std::uint8_t error)
{
	switch (error) {
	case 2: return "Not supported";
	case 3: return "TUNER unavailable";
	case 4: return "State changed locally";
	case 5: return "Switch failed";
	case 6: return "TUNER busy";
	default: return "Source switch failed";
	}
}

static const char *menu_mode_name(control_home::TunerMenuMode mode)
{
	switch (mode) {
	case control_home::TunerMenuMode::STATION_SELECT: return "Station Select";
	case control_home::TunerMenuMode::PLAYING_RADIO: return "Playing Radio";
	case control_home::TunerMenuMode::BLUETOOTH: return "Bluetooth";
	case control_home::TunerMenuMode::BROWSE_LETTER:
	case control_home::TunerMenuMode::BROWSE_COUNTRY:
	case control_home::TunerMenuMode::BROWSE_STATION_LETTER:
	case control_home::TunerMenuMode::BROWSE_TAG:
	case control_home::TunerMenuMode::BROWSE_RESULTS: return "Radio Browser";
	case control_home::TunerMenuMode::FAVORITES: return "Favorites";
	case control_home::TunerMenuMode::RECENT: return "Recent";
	case control_home::TunerMenuMode::IDLE: return "Idle";
	case control_home::TunerMenuMode::ERROR: return "Error";
	case control_home::TunerMenuMode::OTHER: return "Other";
	default: return "Unavailable";
	}
}

static void tuner_control_show_country_filters();
static void tuner_control_show_name_filters();

static void tuner_control_exit_event(lv_event_t *) { tuner_control_close(); }
static void tuner_control_back_event(lv_event_t *)
{
	if (!lv_obj_has_flag(tuner_control.popup, LV_OBJ_FLAG_HIDDEN)) {
		if (!lv_obj_has_flag(tuner_control.spinner, LV_OBJ_FLAG_HIDDEN)) return;
		tuner_control_popup_hide();
		return;
	}
	const auto before = tuner_control.navigation.page;
	tuner_control.navigation.back();
	if (tuner_control.navigation.page == control_home::navigation::Page::HOME) {
		tuner_control_close();
	} else if (before != tuner_control.navigation.page) {
		if (before == control_home::navigation::Page::BROWSER_COUNTRY) {
			if (tuner_control.browser_session)
				(void)control_home::request_browser_action(control_home::BrowserOperation::CANCEL,0,
					tuner_control.browser_session,0);
			tuner_control.browser_session=0;
			lv_obj_add_flag(tuner_control.list_panel, LV_OBJ_FLAG_HIDDEN);
		} else if(tuner_control.navigation.page==control_home::navigation::Page::BROWSER_GENRE){
			(void)control_home::request_browser_page(control_home::BrowserStage::GENRES,0,0,tuner_control.browser_session);
			tuner_control_popup("Loading...",true);
		} else if(tuner_control.navigation.page==control_home::navigation::Page::BROWSER_NAME)
			tuner_control_show_name_filters();
		else if(tuner_control.navigation.page==control_home::navigation::Page::BROWSER_COUNTRY){
			tuner_control.country_filter=0;tuner_control_show_country_filters();
		}
	}
}

static void tuner_control_source_event(lv_event_t *event)
{
	if (!lv_obj_has_flag(tuner_control.popup, LV_OBJ_FLAG_HIDDEN) &&
	    !lv_obj_has_flag(tuner_control.spinner, LV_OBJ_FLAG_HIDDEN)) return;
	const auto target = static_cast<control_home::TunerSource>(
		reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
	if (!tuner_control.state.connected || !tuner_control.state.supported ||
	    tuner_control.state.command_pending || tuner_control.state.source == target)
		return;
	if (!control_home::request_source(target)) {
		tuner_control_popup("TUNER unavailable", false);
		return;
	}
	tuner_control.source_result_handled = false;
	tuner_control_popup(target == control_home::TunerSource::BLUETOOTH
		? "Starting Bluetooth..." : "Switching to Radio...", true);
}

static void tuner_control_open_event(lv_event_t *)
{
	if (!tuner_control.view) return;
	tuner_control.navigation.open_control();
	lv_obj_add_flag(tuner_control.list_panel, LV_OBJ_FLAG_HIDDEN);
	tuner_control.visible = true;
	lv_obj_clear_flag(tuner_control.view, LV_OBJ_FLAG_HIDDEN);
	lv_obj_move_foreground(tuner_control.view);
	if (!tuner_control.state.connected) {
		tuner_control.disconnect_popup = true;
		tuner_control_popup("TUNER disconnected", false, false, true);
	}
}

static void tuner_control_clear_rows()
{
	tuner_control.row_count = 0;
	tuner_control.catalog_revision = 0;
	std::memset(tuner_control.row_kind, 0, sizeof tuner_control.row_kind);
	for (auto *row : tuner_control.list_rows)
		lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
	lv_obj_add_flag(tuner_control.empty_text, LV_OBJ_FLAG_HIDDEN);
	lv_obj_scroll_to_y(tuner_control.list_area, 0, LV_ANIM_OFF);
}

static const char *catalog_title(control_home::CatalogCollection collection)
{
	switch (collection) {
	case control_home::CatalogCollection::SAVED: return "SAVED STATIONS";
	case control_home::CatalogCollection::FAVORITES: return "FAVORITES";
	case control_home::CatalogCollection::RECENT: return "RECENT";
	}
	return "STATIONS";
}

enum : std::uint8_t { ROW_CATALOG=1,ROW_COUNTRY_FILTER,ROW_COUNTRY,ROW_NAME_FILTER,
	ROW_GENRE_ANY,ROW_GENRE,ROW_RESULT,ROW_PREVIOUS,ROW_MORE };

static void tuner_control_add_row(std::uint8_t kind,std::uint16_t token,
	                              const char *name,bool active=false)
{
	if(tuner_control.row_count>=27)return;
	const auto row=tuner_control.row_count++;
	tuner_control.row_kind[row]=kind;lv_obj_set_user_data(tuner_control.list_rows[row],
		reinterpret_cast<void *>(static_cast<std::uintptr_t>(token)));
	std::snprintf(tuner_control.row_names[row],sizeof tuner_control.row_names[row],"%s",name);
	char display[56];std::snprintf(display,sizeof display,"%s%s",active?LV_SYMBOL_PLAY " ":"",name);
	lv_label_set_text(tuner_control.list_labels[row],display);
	lv_obj_set_style_text_color(tuner_control.list_labels[row],active?lv_color_hex(0xc2ff00):lv_color_hex(0xf3f5f2),LV_PART_MAIN);
	lv_obj_set_style_border_width(tuner_control.list_rows[row],active?2:0,LV_PART_MAIN);
	lv_obj_clear_flag(tuner_control.list_rows[row],LV_OBJ_FLAG_HIDDEN);
}

static void tuner_control_show_country_filters()
{
	tuner_control_clear_rows();home_text(tuner_control.list_title,"COUNTRY");
	tuner_control_add_row(ROW_COUNTRY_FILTER,0,"Any Country");
	for(std::uint8_t i=0;i<26;++i){char letter[2]={char('A'+i),0};tuner_control_add_row(ROW_COUNTRY_FILTER,i+1,letter);}
	lv_obj_clear_flag(tuner_control.list_panel,LV_OBJ_FLAG_HIDDEN);
}
static void tuner_control_show_name_filters()
{
	tuner_control_clear_rows();home_text(tuner_control.list_title,"STATION NAME");
	tuner_control_add_row(ROW_NAME_FILTER,0,"Popular Stations");
	for(std::uint8_t i=0;i<26;++i){char letter[2]={char('A'+i),0};tuner_control_add_row(ROW_NAME_FILTER,i+1,letter);}
	lv_obj_clear_flag(tuner_control.list_panel,LV_OBJ_FLAG_HIDDEN);
}

static void tuner_control_open_browser()
{
	if(!tuner_control.browser.connected||!tuner_control.browser.supported){tuner_control_popup("Radio Browser unavailable",false);return;}
	tuner_control.navigation.open_browser();tuner_control.country_filter=0;
	tuner_control.browser_session=tuner_control.browser_revision=0;tuner_control_clear_rows();
	home_text(tuner_control.list_title,"RADIO BROWSER");lv_obj_clear_flag(tuner_control.list_panel,LV_OBJ_FLAG_HIDDEN);
	if(!control_home::request_browser_start()){tuner_control_popup("TUNER busy",false);return;}
	tuner_control.pending_browser_operation=control_home::BrowserOperation::START;
	tuner_control.awaiting_action=true;tuner_control_popup("Searching stations...",true);
}

static void tuner_control_open_catalog(control_home::CatalogCollection collection,
	                                   bool from_home)
{
	if (!tuner_control.view) return;
	tuner_control.visible = true;
	lv_obj_clear_flag(tuner_control.view, LV_OBJ_FLAG_HIDDEN);
	lv_obj_move_foreground(tuner_control.view);
	tuner_control.collection = collection;
	if (collection == control_home::CatalogCollection::SAVED)
		tuner_control.navigation.open_saved();
	else if (collection == control_home::CatalogCollection::FAVORITES)
		tuner_control.navigation.open_favorites(from_home);
	else
		tuner_control.navigation.open_recent();
	home_text(tuner_control.list_title, catalog_title(collection));
	tuner_control_clear_rows();
	lv_obj_clear_flag(tuner_control.list_panel, LV_OBJ_FLAG_HIDDEN);
	if (!control_home::request_catalog(collection, 0))
		tuner_control_popup("TUNER unavailable", false);
}

static void tuner_control_main_row_event(lv_event_t *event)
{
	if (!lv_obj_has_flag(tuner_control.popup, LV_OBJ_FLAG_HIDDEN) &&
	    !lv_obj_has_flag(tuner_control.spinner, LV_OBJ_FLAG_HIDDEN)) return;
	const auto index = reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event));
	if (index < 3)
		tuner_control_open_catalog(static_cast<control_home::CatalogCollection>(index + 1), false);
	else
		tuner_control_open_browser();
}

static void tuner_control_favorites_event(lv_event_t *)
{
	tuner_control_open_catalog(control_home::CatalogCollection::FAVORITES, true);
}

static void tuner_control_request_action(std::uint8_t row,
	                                     control_home::CatalogOperation operation)
{
	if (row >= tuner_control.row_count || tuner_control.awaiting_action) return;
	// Rows can span pages; their stable guarded token is stored as LVGL user data.
	tuner_control.pending_token = static_cast<std::uint16_t>(
		reinterpret_cast<std::uintptr_t>(lv_obj_get_user_data(tuner_control.list_rows[row])));
	tuner_control.pending_operation = operation;
	if (!control_home::request_catalog_action(operation, tuner_control.collection,
	                                         tuner_control.pending_token,
	                                         tuner_control.catalog_revision)) {
		tuner_control_popup("TUNER busy", false);
		return;
	}
	tuner_control.awaiting_action = true;
	if (operation == control_home::CatalogOperation::PLAY) {
		char message[72];
		std::snprintf(message, sizeof message, "Tuning to %s...",
		              tuner_control.row_names[row]);
		tuner_control_popup(message, true);
	} else if (operation == control_home::CatalogOperation::REMOVE) {
		tuner_control_popup("Deleting...", true);
	} else {
		tuner_control_popup("Adding to Favorites...", true);
	}
}

static void tuner_control_row_event(lv_event_t *event)
{
	if (!lv_obj_has_flag(tuner_control.popup, LV_OBJ_FLAG_HIDDEN) &&
	    !lv_obj_has_flag(tuner_control.spinner, LV_OBJ_FLAG_HIDDEN)) return;
	const auto row = static_cast<std::uint8_t>(
		reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
	if (row >= tuner_control.row_count) return;
	const auto kind=tuner_control.row_kind[row];
	const auto token=static_cast<std::uint16_t>(reinterpret_cast<std::uintptr_t>(
		lv_obj_get_user_data(tuner_control.list_rows[row])));
	auto &gesture = tuner_control.gestures[row];
	const auto code = lv_event_get_code(event);
	lv_point_t point{};
	auto *input = lv_indev_get_act();
	if (input) lv_indev_get_point(input, &point);
	const auto now = lv_tick_get();
	if (code == LV_EVENT_PRESSED) gesture.press(now, point.x, point.y);
	else if (code == LV_EVENT_PRESSING) {
		gesture.move(point.x, point.y);
		if (gesture.hold(now) == control_home::navigation::GestureResult::LONG_PRESS) {
			if(kind==ROW_RESULT){
				if(control_home::request_browser_action(control_home::BrowserOperation::ADD_FAVORITE,token,
					tuner_control.browser_session,tuner_control.browser_revision)){
					tuner_control.pending_browser_operation=control_home::BrowserOperation::ADD_FAVORITE;
					tuner_control.awaiting_action=true;tuner_control_popup("Adding to Favorites...",true);}
			}else if (kind==ROW_CATALOG && tuner_control.collection == control_home::CatalogCollection::RECENT)
				tuner_control_request_action(row, control_home::CatalogOperation::ADD_FAVORITE);
			else if(kind==ROW_CATALOG) {
				tuner_control.pending_token = static_cast<std::uint16_t>(
					reinterpret_cast<std::uintptr_t>(lv_obj_get_user_data(tuner_control.list_rows[row])));
				tuner_control.pending_operation = control_home::CatalogOperation::REMOVE;
				tuner_control.delete_confirmation = true;
				char message[80];
				std::snprintf(message, sizeof message, "Delete %s?",
				              tuner_control.row_names[row]);
				tuner_control_popup(message, false, true);
			}
		}
	} else if (code == LV_EVENT_SCROLL_BEGIN || code == LV_EVENT_PRESS_LOST) {
		gesture.cancel();
	} else if (code == LV_EVENT_RELEASED &&
	           gesture.release(now) == control_home::navigation::GestureResult::SHORT_TAP) {
		if(kind==ROW_CATALOG)tuner_control_request_action(row, control_home::CatalogOperation::PLAY);
		else if(kind==ROW_PREVIOUS||kind==ROW_MORE){
			const auto stage=tuner_control.browser.stage;
			(void)control_home::request_browser_page(stage,token,
				stage==control_home::BrowserStage::COUNTRIES?tuner_control.country_filter:0,
				tuner_control.browser_session);tuner_control_popup("Searching stations...",true);
		}else if(kind==ROW_COUNTRY_FILTER){
			if(token==0){
				if(control_home::request_browser_action(control_home::BrowserOperation::SELECT_COUNTRY,
					control_home::BROWSER_ANY,tuner_control.browser_session,tuner_control.browser_revision)){
					tuner_control.pending_browser_operation=control_home::BrowserOperation::SELECT_COUNTRY;
					tuner_control.awaiting_action=true;tuner_control_popup("Loading...",true);}
			}else{tuner_control.country_filter=std::uint8_t('A'+token-1);tuner_control_clear_rows();
				(void)control_home::request_browser_page(control_home::BrowserStage::COUNTRIES,0,
					tuner_control.country_filter,tuner_control.browser_session);tuner_control_popup("Loading...",true);}
		}else if(kind==ROW_COUNTRY||kind==ROW_NAME_FILTER||kind==ROW_GENRE_ANY||kind==ROW_GENRE){
			const auto operation=kind==ROW_COUNTRY?control_home::BrowserOperation::SELECT_COUNTRY:
				kind==ROW_NAME_FILTER?control_home::BrowserOperation::SELECT_NAME:control_home::BrowserOperation::SELECT_GENRE;
			const auto value=kind==ROW_GENRE_ANY?control_home::BROWSER_ANY:token;
			if(control_home::request_browser_action(operation,value,tuner_control.browser_session,tuner_control.browser_revision)){
				tuner_control.pending_browser_operation=operation;tuner_control.awaiting_action=true;
				tuner_control_popup(operation==control_home::BrowserOperation::SELECT_GENRE?"Searching stations...":"Loading...",true);}
		}else if(kind==ROW_RESULT&&control_home::request_browser_action(control_home::BrowserOperation::PLAY,token,
			tuner_control.browser_session,tuner_control.browser_revision)){
			tuner_control.pending_browser_operation=control_home::BrowserOperation::PLAY;
			tuner_control.awaiting_action=true;char message[72];std::snprintf(message,sizeof message,"Tuning to %s...",tuner_control.row_names[row]);tuner_control_popup(message,true);
		}
	}
}

static void tuner_control_confirm_event(lv_event_t *)
{
	if (!tuner_control.delete_confirmation) return;
	tuner_control.delete_confirmation = false;
	if (!control_home::request_catalog_action(
			control_home::CatalogOperation::REMOVE, tuner_control.collection,
			tuner_control.pending_token, tuner_control.catalog_revision)) {
		tuner_control_popup("TUNER busy", false);
		return;
	}
	tuner_control.pending_operation = control_home::CatalogOperation::REMOVE;
	tuner_control.awaiting_action = true;
	tuner_control_popup("Deleting...", true);
}

static void tuner_control_cancel_event(lv_event_t *) { tuner_control_popup_hide(); }
static void tuner_control_ok_event(lv_event_t *)
{
	const bool disconnected = tuner_control.disconnect_popup;
	tuner_control_popup_hide();
	if (disconnected) tuner_control_close();
}

static void tuner_control_apply_catalog()
{
	const auto &page = tuner_control.catalog;
	if (page.collection != tuner_control.collection) return;
	if (page.start == 0) {
		tuner_control_clear_rows();
		tuner_control.catalog_revision = page.revision;
	} else if (page.revision != tuner_control.catalog_revision ||
	           page.start != tuner_control.row_count) {
		tuner_control_clear_rows();
		(void)control_home::request_catalog(tuner_control.collection, 0);
		return;
	}
	for (std::uint8_t i = 0; i < page.item_count && tuner_control.row_count < 15; ++i) {
		const auto row = tuner_control.row_count++;
		tuner_control.row_kind[row]=ROW_CATALOG;
		auto *object = tuner_control.list_rows[row];
		auto *label = tuner_control.list_labels[row];
		lv_obj_set_user_data(object, reinterpret_cast<void *>(
			static_cast<std::uintptr_t>(page.items[i].token)));
		std::snprintf(tuner_control.row_names[row],
		              sizeof tuner_control.row_names[row], "%s",
		              page.items[i].name.data());
		char display[56];
		std::snprintf(display, sizeof display, "%s%s",
		              page.items[i].active ? LV_SYMBOL_PLAY " " : "",
		              tuner_control.row_names[row]);
		lv_label_set_text(label, display);
		lv_obj_set_style_text_color(label,
			page.items[i].active ? lv_color_hex(0xc2ff00) : lv_color_hex(0xf3f5f2),
			LV_PART_MAIN);
		lv_obj_set_style_border_width(object, page.items[i].active ? 2 : 0,
		                              LV_PART_MAIN);
		lv_obj_clear_flag(object, LV_OBJ_FLAG_HIDDEN);
	}
	if (page.count == 0) lv_obj_clear_flag(tuner_control.empty_text, LV_OBJ_FLAG_HIDDEN);
	if (tuner_control.row_count < page.count)
		(void)control_home::request_catalog(tuner_control.collection,
		                                  tuner_control.row_count);
}

static void tuner_control_apply_browser()
{
	const auto &page=tuner_control.browser;
	tuner_control.browser_session=page.session_generation;
	tuner_control.browser_revision=page.revision;
	tuner_control.browser_page_start=page.start;
	if(page.status==control_home::BrowserStatus::LOADING){
		tuner_control_popup("Searching stations...",true);
		tuner_control.browser_poll_at=lv_tick_get()+300;return;
	}
	if(page.status==control_home::BrowserStatus::FAILED){
		tuner_control_popup(page.last_error==9?"Results too large":"Radio Browser error",false);
		return;
	}
	tuner_control_popup_hide();
	if(page.stage==control_home::BrowserStage::COUNTRIES&&tuner_control.country_filter==0){
		tuner_control_show_country_filters();return;
	}
	tuner_control_clear_rows();
	if(page.stage==control_home::BrowserStage::COUNTRIES)home_text(tuner_control.list_title,"COUNTRY");
	else if(page.stage==control_home::BrowserStage::GENRES)home_text(tuner_control.list_title,"GENRE");
	else home_text(tuner_control.list_title,"STATIONS");
	if(page.stage==control_home::BrowserStage::GENRES&&page.start==0)
		tuner_control_add_row(ROW_GENRE_ANY,control_home::BROWSER_ANY,"Any Genre");
	for(std::uint8_t i=0;i<page.item_count;++i){
		const auto kind=page.stage==control_home::BrowserStage::COUNTRIES?ROW_COUNTRY:
			page.stage==control_home::BrowserStage::GENRES?ROW_GENRE:ROW_RESULT;
		tuner_control_add_row(kind,page.items[i].token,page.items[i].name.data(),page.items[i].active);
	}
	if(page.start)tuner_control_add_row(ROW_PREVIOUS,page.start>=5?page.start-5:0,LV_SYMBOL_LEFT " Previous");
	if(std::uint32_t(page.start)+page.item_count<page.count)
		tuner_control_add_row(ROW_MORE,page.start+page.item_count,"More " LV_SYMBOL_RIGHT);
	if(page.status==control_home::BrowserStatus::EMPTY||page.count==0){
		home_text(tuner_control.empty_text,page.stage==control_home::BrowserStage::RESULTS?"No stations found":"No options available");
		lv_obj_clear_flag(tuner_control.empty_text,LV_OBJ_FLAG_HIDDEN);
	}
	lv_obj_clear_flag(tuner_control.list_panel,LV_OBJ_FLAG_HIDDEN);
}

static void tuner_control_refresh(lv_timer_t *)
{
	const bool commands_allowed = power_management::commands_allowed();
	if (tuner_control.visible && !tuner_control.last_commands_allowed && commands_allowed)
		tuner_control_close(); // Wake always resumes at HOME; no submenu replay.
	tuner_control.last_commands_allowed = commands_allowed;
	if (tuner_control.popup_until &&
	    static_cast<std::int32_t>(lv_tick_get() - tuner_control.popup_until) >= 0) {
		const bool close = tuner_control.return_home_after_popup;
		tuner_control_popup_hide();
		tuner_control.return_home_after_popup = false;
		if (close) { tuner_control_close(); return; }
	}
	(void)control_home::full_control_read(tuner_control.state,
	                                    tuner_control.generation);
	(void)control_home::read(tuner_control.playback,
	                       tuner_control.playback_generation);
	const bool browser_changed=control_home::browser_read(tuner_control.browser,
	                                                     tuner_control.browser_generation);
	if (control_home::menu_read(tuner_control.menu,
	                           tuner_control.menu_generation)) {
		char status[48];
		if (tuner_control.menu.connected && tuner_control.menu.supported &&
		    tuner_control.menu.count)
			std::snprintf(status, sizeof status, "TUNER: %s %u/%u",
			              menu_mode_name(tuner_control.menu.mode),
			              unsigned(tuner_control.menu.selected + 1),
			              unsigned(tuner_control.menu.count));
		else
			std::snprintf(status, sizeof status, "TUNER: %s",
			              menu_mode_name(tuner_control.menu.mode));
		home_text(tuner_control.menu_status, status);
	}
	const auto source = tuner_control.state.source;
	lv_obj_set_style_border_width(tuner_control.radio,
	                              source == control_home::TunerSource::RADIO ? 2 : 0,
	                              LV_PART_MAIN);
	lv_obj_set_style_border_width(tuner_control.bluetooth,
	                              source == control_home::TunerSource::BLUETOOTH ? 2 : 0,
	                              LV_PART_MAIN);
	if (!tuner_control.visible) return;
	if (!tuner_control.state.connected) {
		if (!tuner_control.disconnect_popup) {
			tuner_control.disconnect_popup = true;
			tuner_control_popup("TUNER disconnected", false, false, true);
		}
		return;
	}
	if (tuner_control.disconnect_popup) tuner_control_popup_hide();
	if (tuner_control.state.command_pending) return;
	if (tuner_control.state.result_present && !tuner_control.source_result_handled) {
		tuner_control.source_result_handled = true;
		if (!tuner_control.state.result_accepted) {
			tuner_control_popup(source_error(tuner_control.state.last_error), false);
			return;
		}
		if (source == tuner_control.state.requested_source) {
			tuner_control_popup(source == control_home::TunerSource::BLUETOOTH
				? "Bluetooth ready" : "Radio ready", false);
			if (!tuner_control.popup_until) {
				tuner_control.popup_until = lv_tick_get() + 3000;
				tuner_control.return_home_after_popup = true;
			}
		}
	}
	if(browser_changed&&tuner_control.browser.sequence&&tuner_control.browser_session==0)
		tuner_control.browser_session=tuner_control.browser.session_generation;
	if(tuner_control.awaiting_action&&tuner_control.browser.result_present&&
	   !tuner_control.browser.command_pending){
		tuner_control.awaiting_action=false;
		if(!tuner_control.browser.result_accepted){
			if(tuner_control.pending_browser_operation==control_home::BrowserOperation::PLAY){
				tuner_control_close();tuner_control_popup("Station unavailable",false);
				tuner_control.popup_until=lv_tick_get()+3000;
			}else{tuner_control_popup("Radio Browser operation failed",false);}
		}else switch(tuner_control.pending_browser_operation){
		case control_home::BrowserOperation::START:
			(void)control_home::request_browser_page(control_home::BrowserStage::COUNTRIES,0,0,0);
			break;
		case control_home::BrowserOperation::SELECT_COUNTRY:
			tuner_control.navigation.browser_next();tuner_control_show_name_filters();break;
		case control_home::BrowserOperation::SELECT_NAME:
			tuner_control.navigation.browser_next();
			(void)control_home::request_browser_page(control_home::BrowserStage::GENRES,0,0,tuner_control.browser_session);break;
		case control_home::BrowserOperation::SELECT_GENRE:
			tuner_control.navigation.browser_next();
			(void)control_home::request_browser_page(control_home::BrowserStage::RESULTS,0,0,tuner_control.browser_session);break;
		case control_home::BrowserOperation::PLAY:
			tuner_control_popup("Now Playing",false);tuner_control.popup_until=lv_tick_get()+3000;
			tuner_control.return_home_after_popup=true;break;
		case control_home::BrowserOperation::ADD_FAVORITE:
			tuner_control_popup("Added to Favorites",false);tuner_control.popup_until=lv_tick_get()+3000;break;
		default:break;
		}
	}
	if(browser_changed&&tuner_control.browser.sequence&&tuner_control.browser_session&&
	   tuner_control.browser.sequence!=tuner_control.browser_sequence_applied){
		tuner_control.browser_sequence_applied=tuner_control.browser.sequence;
		tuner_control_apply_browser();
	}
	if(tuner_control.browser.status==control_home::BrowserStatus::LOADING&&
	   tuner_control.browser_poll_at&&static_cast<std::int32_t>(lv_tick_get()-tuner_control.browser_poll_at)>=0&&
	   !tuner_control.browser.query_pending&&!tuner_control.browser.command_pending){
		tuner_control.browser_poll_at=lv_tick_get()+300;
		(void)control_home::request_browser_page(tuner_control.browser.stage,tuner_control.browser.start,
			tuner_control.browser.stage==control_home::BrowserStage::COUNTRIES?tuner_control.country_filter:0,
			tuner_control.browser.session_generation);
	}
	if (control_home::catalog_read(tuner_control.catalog,
	                              tuner_control.catalog_generation)) {
		if (tuner_control.catalog.sequence) tuner_control_apply_catalog();
		if (tuner_control.awaiting_action &&
		    tuner_control.catalog.result_present &&
		    !tuner_control.catalog.command_pending) {
			tuner_control.awaiting_action = false;
			if (!tuner_control.catalog.result_accepted) {
				if (tuner_control.catalog.last_error == 4) {
					tuner_control_popup_hide();
					tuner_control_clear_rows();
					(void)control_home::request_catalog(tuner_control.collection, 0);
				} else if (tuner_control.catalog.last_error == 8) {
					tuner_control_popup("Already in Favorites", false);
					tuner_control.popup_until = lv_tick_get() + 3000;
				} else if (tuner_control.catalog.last_error == 9) {
					tuner_control_popup("Favorites full (10/10)", false);
					tuner_control.popup_until = lv_tick_get() + 3000;
				} else if (tuner_control.pending_operation == control_home::CatalogOperation::PLAY) {
					tuner_control_close();
					tuner_control_popup("Station unavailable", false);
					tuner_control.popup_until = lv_tick_get() + 3000;
				} else {
					tuner_control_popup("Operation failed", false);
					tuner_control.popup_until = lv_tick_get() + 3000;
				}
			} else if (tuner_control.pending_operation == control_home::CatalogOperation::PLAY) {
				tuner_control.awaiting_playback = true;
				tuner_control_clear_rows();
				(void)control_home::request_catalog(tuner_control.collection, 0);
			} else {
				tuner_control_popup(
					tuner_control.pending_operation == control_home::CatalogOperation::REMOVE
						? "Deleted" : "Added to Favorites", false);
				tuner_control.popup_until = lv_tick_get() + 3000;
				tuner_control_clear_rows();
				(void)control_home::request_catalog(tuner_control.collection, 0);
			}
		}
	}
	if (tuner_control.awaiting_playback &&
	    tuner_control.playback.playback == control_home::Playback::PLAYING) {
		bool active = false;
		for (std::uint8_t i = 0; i < tuner_control.catalog.item_count; ++i)
			active = active || tuner_control.catalog.items[i].active;
		if (active) {
			tuner_control.awaiting_playback = false;
			tuner_control_popup("Now Playing", false);
			tuner_control.popup_until = lv_tick_get() + 3000;
			tuner_control.return_home_after_popup = true;
		}
	}
}

static void home_refresh(lv_timer_t *)
{
	const bool home_active = home.page &&
	                         lv_carousel_get_element_act(src_ui.screen_carousel_1) ==
	                             home.page;
	encoder_home.store(home_active || tuner_control.visible,
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
	const bool metadata_active = home_active && power_management::commands_allowed();
	home_scrolling_text(home.title, s.subtitle[0] ? s.subtitle : "--",
	                    metadata_active);
	home_scrolling_text(home.subtitle, s.title[0] ? s.title : "--",
	                    metadata_active);
	if (tuner_control.state.source == control_home::TunerSource::BLUETOOTH &&
	    !tuner_control.state.bluetooth_connected)
		home_scrolling_text(home.subtitle, "Waiting for Bluetooth device...",
		                    metadata_active);
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

static lv_obj_t *tuner_control_button(lv_obj_t *parent, const char *text,
	                                  lv_coord_t y, lv_coord_t height = 44)
{
	const lv_color_t panel = lv_color_hex(0x1c292e);
	const lv_color_t foreground = lv_color_hex(0xf3f5f2);
	lv_obj_t *button = remote01_panel(parent, 12, y, 148, height, panel, 5);
	lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_set_style_border_color(button, lv_color_hex(0xc2ff00), LV_PART_MAIN);
	lv_obj_set_style_border_opa(button, LV_OPA_COVER, LV_PART_MAIN);
	auto *label = remote01_single_line(button, text, &lv_font_montserrat_14,
	                                  foreground, 10, (height - 14) / 2,
	                                  128, LV_LABEL_LONG_CLIP,
	                                  LV_TEXT_ALIGN_LEFT);
	(void)label;
	return button;
}

static void tuner_control_create()
{
	const lv_color_t background = lv_color_hex(0x11191d);
	const lv_color_t text = lv_color_hex(0xf3f5f2);
	const lv_color_t muted = lv_color_hex(0x91a3a6);
	tuner_control.view = remote01_panel(src_ui.screen, 0, 0, 172, 640,
	                                  background, 0);
	lv_obj_clear_flag(tuner_control.view, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_flag(tuner_control.view, LV_OBJ_FLAG_HIDDEN);
	lv_obj_t *back = tuner_control_button(tuner_control.view, LV_SYMBOL_LEFT " BACK", 8, 34);
	lv_obj_set_width(back, 70);
	lv_obj_t *exit = tuner_control_button(tuner_control.view, "EXIT", 8, 34);
	lv_obj_set_pos(exit, 90, 8); lv_obj_set_width(exit, 70);
	lv_obj_add_event_cb(back, tuner_control_back_event, LV_EVENT_CLICKED, nullptr);
	lv_obj_add_event_cb(exit, tuner_control_exit_event, LV_EVENT_CLICKED, nullptr);
	remote01_label(tuner_control.view, "TUNER CONTROL", &lv_font_montserrat_20,
	               text, 12, 60);
	tuner_control.radio = tuner_control_button(tuner_control.view, "RADIO", 100, 54);
	tuner_control.bluetooth = tuner_control_button(tuner_control.view, "BLUETOOTH", 162, 54);
	lv_obj_add_event_cb(tuner_control.radio, tuner_control_source_event,
	                    LV_EVENT_CLICKED,
	                    reinterpret_cast<void *>(static_cast<std::uintptr_t>(control_home::TunerSource::RADIO)));
	lv_obj_add_event_cb(tuner_control.bluetooth, tuner_control_source_event,
	                    LV_EVENT_CLICKED,
	                    reinterpret_cast<void *>(static_cast<std::uintptr_t>(control_home::TunerSource::BLUETOOTH)));
	static const char *entries[] = {"Saved Stations", "Favorites", "Recent",
	                                "Browse All Stations"};
	for (unsigned i = 0; i < 4; ++i) {
		auto *row = tuner_control_button(tuner_control.view, entries[i], 242 + i * 58, 48);
		tuner_control.main_rows[i] = row;
		lv_obj_add_event_cb(row, tuner_control_main_row_event, LV_EVENT_CLICKED,
		                    reinterpret_cast<void *>(static_cast<std::uintptr_t>(i)));
	}
	tuner_control.menu_status = remote01_single_line(tuner_control.view, "TUNER: Unavailable",
	                    &lv_font_montserrat_12, muted, 12, 594, 148,
	                    LV_LABEL_LONG_CLIP, LV_TEXT_ALIGN_CENTER);
	tuner_control.list_panel = remote01_panel(tuner_control.view, 0, 50, 172, 590,
	                                      background, 0);
	lv_obj_add_flag(tuner_control.list_panel, LV_OBJ_FLAG_HIDDEN);
	tuner_control.list_title = remote01_single_line(
		tuner_control.list_panel, "STATIONS", &lv_font_montserrat_16, text,
		12, 10, 148, LV_LABEL_LONG_CLIP, LV_TEXT_ALIGN_CENTER);
	tuner_control.list_area = remote01_panel(tuner_control.list_panel, 8, 48, 156,
	                                      530, background, 0);
	lv_obj_add_flag(tuner_control.list_area, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_scroll_dir(tuner_control.list_area, LV_DIR_VER);
	lv_obj_set_scrollbar_mode(tuner_control.list_area, LV_SCROLLBAR_MODE_AUTO);
	tuner_control.empty_text = remote01_single_line(
		tuner_control.list_area, "No stations available", &lv_font_montserrat_14,
		muted, 4, 20, 148, LV_LABEL_LONG_WRAP, LV_TEXT_ALIGN_CENTER);
	lv_obj_add_flag(tuner_control.empty_text, LV_OBJ_FLAG_HIDDEN);
	for (std::uint8_t i = 0; i < 27; ++i) {
		auto *row = remote01_panel(tuner_control.list_area, 2, i * 54, 148, 48,
		                          lv_color_hex(0x1c292e), 5);
		lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
		lv_obj_set_style_border_color(row, lv_color_hex(0xc2ff00), LV_PART_MAIN);
		lv_obj_set_style_border_opa(row, LV_OPA_COVER, LV_PART_MAIN);
		auto *label = remote01_single_line(row, "", &lv_font_montserrat_14, text,
		                                  8, 16, 132, LV_LABEL_LONG_SCROLL,
		                                  LV_TEXT_ALIGN_LEFT);
		lv_obj_set_style_anim_speed(label, 25, LV_PART_MAIN);
		tuner_control.list_rows[i] = row;
		tuner_control.list_labels[i] = label;
		lv_obj_add_event_cb(row, tuner_control_row_event, LV_EVENT_ALL,
		                    reinterpret_cast<void *>(static_cast<std::uintptr_t>(i)));
		lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
	}
	tuner_control.popup = remote01_panel(src_ui.screen, 16, 230, 140, 180,
	                                   lv_color_hex(0x24343a), 8);
	tuner_control.popup_text = remote01_single_line(tuner_control.popup, "", &lv_font_montserrat_14,
	                                               text, 8, 68, 124,
	                                               LV_LABEL_LONG_WRAP, LV_TEXT_ALIGN_CENTER);
	tuner_control.spinner = lv_spinner_create(tuner_control.popup, 1000, 70);
	lv_obj_set_size(tuner_control.spinner, 38, 38);
	lv_obj_align(tuner_control.spinner, LV_ALIGN_TOP_MID, 0, 16);
	tuner_control.popup_confirm = tuner_control_button(tuner_control.popup, "Delete", 132, 36);
	lv_obj_set_pos(tuner_control.popup_confirm, 8, 132);
	lv_obj_set_width(tuner_control.popup_confirm, 58);
	tuner_control.popup_cancel = tuner_control_button(tuner_control.popup, "Cancel", 132, 36);
	lv_obj_set_pos(tuner_control.popup_cancel, 74, 132);
	lv_obj_set_width(tuner_control.popup_cancel, 58);
	tuner_control.popup_ok = tuner_control_button(tuner_control.popup, "OK", 132, 36);
	lv_obj_set_pos(tuner_control.popup_ok, 41, 132);
	lv_obj_set_width(tuner_control.popup_ok, 58);
	lv_obj_add_event_cb(tuner_control.popup_confirm, tuner_control_confirm_event,
	                    LV_EVENT_CLICKED, nullptr);
	lv_obj_add_event_cb(tuner_control.popup_cancel, tuner_control_cancel_event,
	                    LV_EVENT_CLICKED, nullptr);
	lv_obj_add_event_cb(tuner_control.popup_ok, tuner_control_ok_event,
	                    LV_EVENT_CLICKED, nullptr);
	lv_obj_add_flag(tuner_control.popup, LV_OBJ_FLAG_HIDDEN);
	tuner_control.timer = lv_timer_create(tuner_control_refresh, 20, nullptr);
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
	                     LV_LABEL_LONG_SCROLL, LV_TEXT_ALIGN_LEFT);
	lv_obj_set_style_anim_speed(home.title, 25, LV_PART_MAIN);
	home.subtitle = remote01_single_line(page, "--", &lv_font_montserrat_14, muted, 12, 290, 148,
	                     LV_LABEL_LONG_SCROLL, LV_TEXT_ALIGN_LEFT);
	lv_obj_set_style_anim_speed(home.subtitle, 25, LV_PART_MAIN);
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

	static const char *navigation[] = {"FAVORITES", "DEVICES", "TUNER CONTROL"};
	for (int i = 0; i < 3; ++i) {
		lv_obj_t *row = remote01_panel(page, 12, 486 + i * 40, 148, 36, panel, 4);
		lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
		lv_obj_t *name = remote01_label(row, navigation[i], &lv_font_montserrat_14, text, 0, 0);
		lv_obj_align(name, LV_ALIGN_LEFT_MID, 10, 0);
		lv_obj_t *arrow = remote01_label(row, LV_SYMBOL_RIGHT, &lv_font_montserrat_14, text, 0, 0);
		lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -10, 0);
		if (i == 0)
			lv_obj_add_event_cb(row, tuner_control_favorites_event, LV_EVENT_CLICKED, nullptr);
		else if (i == 2)
			lv_obj_add_event_cb(row, tuner_control_open_event, LV_EVENT_CLICKED, nullptr);
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
	tuner_control_create();
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
