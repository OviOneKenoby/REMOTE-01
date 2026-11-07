/*
* Copyright 2025 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "events_init.h"
#include "widgets_init.h"
#include "custom.h"
#include "esp_heap_caps.h"


void setup_scr_screen(lv_ui *ui)
{
    //Write codes screen
    ui->screen = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen, 172, 640);
    lv_obj_set_scrollbar_mode(ui->screen, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_carousel_1
    ui->screen_carousel_1 = lv_carousel_create(ui->screen);
    lv_carousel_set_element_width(ui->screen_carousel_1, 172);
    lv_obj_set_size(ui->screen_carousel_1, 172, 640);
    ui->screen_carousel_1_element_1 = lv_carousel_add_element(ui->screen_carousel_1, 0);
    ui->screen_carousel_1_element_2 = lv_carousel_add_element(ui->screen_carousel_1, 1);
    lv_obj_set_pos(ui->screen_carousel_1, 0, 0);
    lv_obj_set_size(ui->screen_carousel_1, 172, 640);
    lv_obj_set_scrollbar_mode(ui->screen_carousel_1, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_carousel_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_carousel_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_carousel_1, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_carousel_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_carousel_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_carousel_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for screen_carousel_1, Part: LV_PART_SCROLLBAR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_carousel_1, 0, LV_PART_SCROLLBAR|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_carousel_1, 0, LV_PART_SCROLLBAR|LV_STATE_DEFAULT);

    //Write style state: LV_STATE_DEFAULT for &style_screen_carousel_1_extra_element_items_default
    static lv_style_t style_screen_carousel_1_extra_element_items_default;
    ui_init_style(&style_screen_carousel_1_extra_element_items_default);

    lv_style_set_bg_opa(&style_screen_carousel_1_extra_element_items_default, 0);
    lv_style_set_outline_width(&style_screen_carousel_1_extra_element_items_default, 0);
    lv_style_set_border_width(&style_screen_carousel_1_extra_element_items_default, 0);
    lv_style_set_radius(&style_screen_carousel_1_extra_element_items_default, 0);
    lv_style_set_shadow_width(&style_screen_carousel_1_extra_element_items_default, 0);
    lv_obj_add_style(ui->screen_carousel_1_element_2, &style_screen_carousel_1_extra_element_items_default, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_add_style(ui->screen_carousel_1_element_1, &style_screen_carousel_1_extra_element_items_default, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style state: LV_STATE_FOCUSED for &style_screen_carousel_1_extra_element_items_focused
    static lv_style_t style_screen_carousel_1_extra_element_items_focused;
    ui_init_style(&style_screen_carousel_1_extra_element_items_focused);

    lv_style_set_bg_opa(&style_screen_carousel_1_extra_element_items_focused, 0);
    lv_style_set_outline_width(&style_screen_carousel_1_extra_element_items_focused, 0);
    lv_style_set_border_width(&style_screen_carousel_1_extra_element_items_focused, 0);
    lv_style_set_radius(&style_screen_carousel_1_extra_element_items_focused, 5);
    lv_style_set_shadow_width(&style_screen_carousel_1_extra_element_items_focused, 0);
    lv_obj_add_style(ui->screen_carousel_1_element_2, &style_screen_carousel_1_extra_element_items_focused, LV_PART_MAIN|LV_STATE_FOCUSED);
    lv_obj_add_style(ui->screen_carousel_1_element_1, &style_screen_carousel_1_extra_element_items_focused, LV_PART_MAIN|LV_STATE_FOCUSED);



    //Write codes screen_slider_1
    ui->screen_slider_1 = lv_slider_create(ui->screen_carousel_1_element_2);
    lv_slider_set_range(ui->screen_slider_1, 0, 255);
    lv_slider_set_mode(ui->screen_slider_1, LV_SLIDER_MODE_NORMAL);
    lv_slider_set_value(ui->screen_slider_1, 255, LV_ANIM_OFF);
    lv_obj_set_pos(ui->screen_slider_1, 6, 320);
    lv_obj_set_size(ui->screen_slider_1, 150, 20);

    //Write style for screen_slider_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_slider_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_slider_1, 50, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(ui->screen_slider_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_slider_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for screen_slider_1, Part: LV_PART_INDICATOR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_slider_1, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_slider_1, lv_color_hex(0xc2ff00), LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_slider_1, LV_GRAD_DIR_NONE, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_slider_1, 50, LV_PART_INDICATOR|LV_STATE_DEFAULT);

    //Write style for screen_slider_1, Part: LV_PART_KNOB, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_slider_1, 255, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_slider_1, lv_color_hex(0xff6c00), LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_slider_1, LV_GRAD_DIR_NONE, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_slider_1, 50, LV_PART_KNOB|LV_STATE_DEFAULT);

    //Write codes screen_label_22
    ui->screen_label_22 = lv_label_create(ui->screen_carousel_1_element_2);
    lv_label_set_text(ui->screen_label_22, "The slider controls the backlight\n");
    lv_label_set_long_mode(ui->screen_label_22, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_label_22, 8, 239);
    lv_obj_set_size(ui->screen_label_22, 157, 32);

    //Write style for screen_label_22, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_label_22, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_label_22, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_label_22, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_label_22, &lv_font_montserratMedium_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_label_22, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_label_22, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_label_22, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_label_22, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_label_22, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_label_22, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_label_22, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_label_22, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_label_22, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_label_22, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //The custom code of screen.


    //Update current screen layout.
    lv_obj_update_layout(ui->screen);

}
