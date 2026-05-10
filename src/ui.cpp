#include <Arduino.h>
#include "User_Setup.h"
#include <TFT_eSPI.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <Ticker.h>
#include <lvgl.h>
#include <WiFi.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <test.h>
#include "printer.h"
#include "ui.h"

TFT_eSPI tft = TFT_eSPI(240, 240);
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[TFT_WIDTH * 10];

lv_style_t screenstyle;
lv_style_t spinnerstyle;
lv_style_t spinnerstyle2;
lv_style_t spinnerstyle3;
lv_style_t spinnerstyle4;
lv_style_t style_label_logo;
lv_style_t style_label_status;
lv_style_t style_label_progress;
lv_style_t style_label_current;
lv_style_t style_label_current_cold;
lv_style_t style_label_target;
lv_style_t style_label_message;
lv_style_t knob_style;
lv_style_t style_label_fan;

lv_obj_t *screen1;
lv_obj_t *screen2;
lv_obj_t *screen3;

lv_obj_t *label2;
lv_obj_t *preload;
lv_obj_t *updatearc;

lv_obj_t *tool_cur_temp_label;
lv_obj_t *bed_cur_temp_label;
lv_obj_t *tool_target_temp_label;
lv_obj_t *bed_target_temp_label;
lv_obj_t *progress_label;
lv_obj_t *progress_arc;
lv_obj_t *label_fan_speed;
lv_obj_t *label_chamber_temp;
lv_obj_t *label_layer_total;
lv_obj_t *wifi_bars[4] = {nullptr, nullptr, nullptr, nullptr};
lv_obj_t *arc_fan;
lv_obj_t *label_message;

String lastMessage = "";
bool screensaver_active = false;
unsigned long lastUpdate;
unsigned long centerToggleLastMs = 0;
bool centerShowProgress = true;
bool centerFadeInitialized = false;
const uint16_t centerFadeMs = 380;
const uint16_t centerToggleMs = 2000;
String latchedCenterStatus = "";
int16_t latchedArcValue = 0;

static void set_label_text_opa(void *obj, int32_t value)
{
  lv_obj_set_style_text_opa((lv_obj_t *)obj, (lv_opa_t)value, LV_PART_MAIN);
}

static void animate_label_fade(lv_obj_t *obj, lv_opa_t from, lv_opa_t to, uint16_t duration)
{
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, obj);
  lv_anim_set_values(&a, from, to);
  lv_anim_set_time(&a, duration);
  lv_anim_set_exec_cb(&a, set_label_text_opa);
  lv_anim_start(&a);
}

static void set_arc_value_anim(void *obj, int32_t value)
{
  lv_arc_set_value((lv_obj_t *)obj, value);
}

static void animate_progress_arc(lv_obj_t *arc, int16_t target, uint16_t duration)
{
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, arc);
  lv_anim_set_values(&a, 0, target);
  lv_anim_set_time(&a, duration);
  lv_anim_set_exec_cb(&a, set_arc_value_anim);
  lv_anim_start(&a);
}

void setCenterLabelsTheme(lv_color_t textColor)
{
  lv_obj_set_style_text_color(progress_label, textColor, LV_PART_MAIN);
  lv_obj_set_style_text_color(label_message, textColor, LV_PART_MAIN);

  lv_obj_set_style_bg_opa(progress_label, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(label_message, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_pad_left(progress_label, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_right(progress_label, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_top(progress_label, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(progress_label, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_left(label_message, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_right(label_message, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_top(label_message, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(label_message, 0, LV_PART_MAIN);
}

void updateWifiIndicator()
{
  int quality = 0;
  if (WiFi.status() == WL_CONNECTED)
  {
    int32_t rssi = WiFi.RSSI();
    if (rssi <= -100)
      quality = 0;
    else if (rssi >= -50)
      quality = 100;
    else
      quality = 2 * (rssi + 100);
  }

  int level = 0;
  if (quality > 0 && quality <= 25)
    level = 1;
  else if (quality > 25 && quality <= 50)
    level = 2;
  else if (quality > 50 && quality <= 75)
    level = 3;
  else if (quality > 75)
    level = 4;

  lv_color_t activeColor = lv_color_hex(0xff0000);
  if (quality > 25 && quality <= 50)
    activeColor = lv_color_hex(0xffd400);
  else if (quality > 50)
    activeColor = lv_color_hex(0x00ff00);

  for (int i = 0; i < 4; i++)
  {
    if (wifi_bars[i] == nullptr)
      continue;

    if (i < level)
    {
      lv_obj_set_style_bg_color(wifi_bars[i], activeColor, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(wifi_bars[i], LV_OPA_COVER, LV_PART_MAIN);
    }
    else
    {
      lv_obj_set_style_bg_color(wifi_bars[i], lv_color_hex(0x404040), LV_PART_MAIN);
      lv_obj_set_style_bg_opa(wifi_bars[i], LV_OPA_60, LV_PART_MAIN);
    }
  }
}

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft.startWrite();                                       
  tft.setAddrWindow(area->x1, area->y1, w, h);            
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();                                         

  lv_disp_flush_ready(disp); 
}

void lv_display_Init()
{
  tft.init();
  tft.setRotation(0);
  lv_init();
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, TFT_WIDTH * 10);

  /*Initialize the display*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  /*Change the following line to your display resolution*/
  disp_drv.hor_res = TFT_WIDTH;
  disp_drv.ver_res = TFT_HEIGHT;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);
}

void lv_display_led_On()
{
  pinMode(16, OUTPUT);    
  digitalWrite(16, HIGH); 

  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH); 
}

void lv_display_led_Off()
{
  pinMode(16, OUTPUT);   
  digitalWrite(16, LOW); 

  pinMode(2, OUTPUT);
  digitalWrite(2, LOW); 
}



void init_styles(){
 // Styles

  lv_style_set_text_font(&style_label_current_cold, &lv_font_montserrat_22);
  lv_style_set_text_color(&style_label_current_cold, lv_color_hex(0xa0a0a0));

  lv_style_set_text_font(&style_label_logo, &lv_font_montserrat_40);
  lv_style_set_text_color(&style_label_logo, lv_color_hex(0x00FF000));

  lv_style_set_text_font(&style_label_progress, &lv_font_montserrat_40);
  lv_style_set_text_color(&style_label_progress, COLOR_LABEL_PROGRESS);

  lv_style_set_text_font(&style_label_message, &lv_font_montserrat_22);
  lv_style_set_text_color(&style_label_message, COLOR_LABEL_MESSAGE);

  lv_style_set_text_font(&style_label_current, &lv_font_montserrat_22);
  lv_style_set_text_color(&style_label_current, lv_color_hex(0x00ff00));

  lv_style_set_text_font(&style_label_target, &lv_font_montserrat_18);
  lv_style_set_text_color(&style_label_target, lv_color_hex(0xa0a0a0));

  lv_style_set_arc_color(&spinnerstyle4, COLOR_SPINNER_FAN_BACK);
  lv_style_set_arc_color(&spinnerstyle3, COLOR_SPINNER_FAN);
  lv_style_set_arc_width(&spinnerstyle4, 4);
  lv_style_set_arc_width(&spinnerstyle3, 4);

  lv_style_set_arc_color(&spinnerstyle, COLOR_SPINNER_BACK);
  lv_style_set_arc_color(&spinnerstyle2, COLOR_SPINNER);

  lv_style_set_bg_color(&knob_style, COLOR_SPINNER_KNOB);
  lv_style_set_pad_all(&knob_style, -1);

  lv_style_set_bg_color(&screenstyle, COLOR_BLACK);
}

void init_ui()
{
  lv_display_Init();
  lv_display_led_On();
  Serial.println("Booting");

  lv_disp_set_bg_color(NULL, lv_color_hex(0x000000));

  init_styles();

  // Welcome Screen

  screen1 = lv_obj_create(NULL);

  lv_obj_add_style(screen1, &screenstyle, LV_PART_MAIN);

  lv_obj_t *label1 = lv_label_create(screen1);
  lv_obj_add_style(label1, &style_label_logo, LV_PART_MAIN);
  lv_label_set_text(label1, "XKN 2");
  lv_obj_align(label1, LV_ALIGN_CENTER, 0, -10);

  label2 = lv_label_create(screen1);
  lv_style_set_text_font(&style_label_status, &lv_font_montserrat_22);
  lv_style_set_text_color(&style_label_status, lv_color_hex(0xa0a0a0));
  lv_obj_add_style(label2, &style_label_status, LV_PART_MAIN);
  lv_label_set_text(label2, "Booting");
  lv_obj_align(label2, LV_ALIGN_CENTER, 0, 28);

  preload = lv_spinner_create(screen1, 5000, 60);
  lv_obj_set_size(preload, 239, 239);
  lv_obj_align(preload, LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_style(preload, &spinnerstyle, LV_PART_MAIN);
  lv_obj_add_style(preload, &spinnerstyle2, LV_PART_INDICATOR);
  lv_scr_load(screen1);

  updatearc = lv_arc_create(screen1);
  lv_obj_set_size(updatearc, 239, 239);
  lv_obj_align(updatearc, LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_style(updatearc, &spinnerstyle, LV_PART_MAIN);
  lv_obj_add_style(updatearc, &spinnerstyle2, LV_PART_INDICATOR);
  lv_obj_add_style(updatearc, &knob_style, LV_PART_KNOB);
  lv_arc_set_rotation(updatearc, 270);
  lv_arc_set_bg_angles(updatearc, 0, 360);
  lv_arc_set_value(updatearc, 0);
  lv_obj_clear_flag(updatearc, LV_OBJ_FLAG_CLICKABLE);
  lv_scr_load(screen1);
  lv_obj_add_flag(updatearc, LV_OBJ_FLAG_HIDDEN);

  // Printing Screen
  screen2 = lv_obj_create(NULL);
  // lv_style_set_bg_color(&screenstyle, lv_color_hex(0x000000));
  lv_obj_add_style(screen2, &screenstyle, LV_PART_MAIN);

  progress_label = lv_label_create(screen2);
  lv_obj_add_style(progress_label, &style_label_progress, LV_PART_MAIN);
  lv_label_set_text(progress_label, "-");
  lv_obj_align(progress_label, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_text_opa(progress_label, LV_OPA_COVER, LV_PART_MAIN);

  label_message = lv_label_create(screen2);
  lv_obj_add_style(label_message, &style_label_message, LV_PART_MAIN);
  lv_label_set_text(label_message, "-");
  lv_obj_align(label_message, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_text_opa(label_message, LV_OPA_TRANSP, LV_PART_MAIN);

  bed_cur_temp_label = lv_label_create(screen2);
  lv_obj_add_style(bed_cur_temp_label, &style_label_current, LV_PART_MAIN);
  lv_label_set_text(bed_cur_temp_label, "-");
  lv_obj_align(bed_cur_temp_label, LV_ALIGN_CENTER, 35, 45);

  bed_target_temp_label = lv_label_create(screen2);
  lv_obj_add_style(bed_target_temp_label, &style_label_target, LV_PART_MAIN);
  lv_label_set_text(bed_target_temp_label, "-");
  lv_obj_align(bed_target_temp_label, LV_ALIGN_CENTER, 35, 65);

  tool_cur_temp_label = lv_label_create(screen2);
  lv_obj_add_style(tool_cur_temp_label, &style_label_current, LV_PART_MAIN);
  lv_label_set_text(tool_cur_temp_label, "-");
  lv_obj_align(tool_cur_temp_label, LV_ALIGN_CENTER, -35, 45);

  tool_target_temp_label = lv_label_create(screen2);
  lv_obj_add_style(tool_target_temp_label, &style_label_target, LV_PART_MAIN);
  lv_label_set_text(tool_target_temp_label, "-");
  lv_obj_align(tool_target_temp_label, LV_ALIGN_CENTER, -35, 65);

  // lv_style_copy(&style_label_fan,&style_label_current);
  label_fan_speed = lv_label_create(screen2);
  lv_obj_add_style(label_fan_speed, &style_label_current, LV_PART_MAIN);
  lv_label_set_text(label_fan_speed, "-");
  lv_obj_align(label_fan_speed, LV_ALIGN_CENTER, 35, -60);

  arc_fan = lv_arc_create(screen2);
  lv_obj_set_size(arc_fan, 30, 30);
  // lv_obj_align(arc_fan, LV_ALIGN_CENTER, 62, -38);
  lv_obj_align_to(arc_fan, label_fan_speed, LV_ALIGN_OUT_RIGHT_MID, -12, 13);
  //  lv_obj_align_to(arc_fan, label_fan_speed, LV_ALIGN_RIGHT_MID, 23, 13);
  lv_obj_add_style(arc_fan, &spinnerstyle4, LV_PART_MAIN);
  lv_obj_add_style(arc_fan, &spinnerstyle3, LV_PART_INDICATOR);
  lv_arc_set_rotation(arc_fan, 270);
  lv_arc_set_bg_angles(arc_fan, 0, 270);
  lv_arc_set_value(arc_fan, 0);
  lv_obj_remove_style(arc_fan, NULL, LV_PART_KNOB);
  lv_obj_clear_flag(arc_fan, LV_OBJ_FLAG_CLICKABLE);

  // ---

  label_chamber_temp = lv_label_create(screen2);
  lv_obj_add_style(label_chamber_temp, &style_label_current, LV_PART_MAIN);
  lv_label_set_text(label_chamber_temp, "-");
  lv_obj_align(label_chamber_temp, LV_ALIGN_CENTER, -35, -60);

  label_layer_total = lv_label_create(screen2);
  lv_obj_add_style(label_layer_total, &style_label_target, LV_PART_MAIN);
  lv_label_set_text(label_layer_total, "-");
  lv_obj_align(label_layer_total, LV_ALIGN_CENTER, -35, -40);

  progress_arc = lv_arc_create(screen2);
  lv_obj_set_size(progress_arc, 239, 239);
  lv_obj_align(progress_arc, LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_style(progress_arc, &spinnerstyle, LV_PART_MAIN);
  lv_obj_add_style(progress_arc, &spinnerstyle2, LV_PART_INDICATOR);
  lv_obj_add_style(progress_arc, &knob_style, LV_PART_KNOB);
  lv_arc_set_rotation(progress_arc, 270);
  lv_arc_set_bg_angles(progress_arc, 0, 360);
  lv_arc_set_value(progress_arc, 0);
  lv_obj_remove_style(progress_arc, NULL, LV_PART_KNOB);
  lv_obj_clear_flag(progress_arc, LV_OBJ_FLAG_CLICKABLE);

  // WiFi quality indicator (4 bars) on the left side
  const int16_t wifiCenterOffsetX = -10;
  const int16_t wifiTopOffsetY = 15;
  const int16_t wifiBarHeights[4] = {6, 10, 14, 18};
  for (int i = 0; i < 4; i++)
  {
    wifi_bars[i] = lv_obj_create(screen2);
    lv_obj_clear_flag(wifi_bars[i], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_width(wifi_bars[i], 0, LV_PART_MAIN);
    lv_obj_set_style_radius(wifi_bars[i], 1, LV_PART_MAIN);
    lv_obj_set_size(wifi_bars[i], 5, wifiBarHeights[i]);
    lv_obj_align(wifi_bars[i], LV_ALIGN_TOP_MID, wifiCenterOffsetX + i * 7, wifiTopOffsetY + (wifiBarHeights[3] - wifiBarHeights[i]));
  }

  // Screensaver
  screen3 = lv_obj_create(NULL);
}



void update_screen_values(printer_values pValues)
{
  String text_ext_actual_temp = String(pValues.tooltemp_actual, 10) + "°C";
  String text_ext_target_temp = String(pValues.tooltemp_target, 10) + "°C";
  String text_bed_actual_temp = String(pValues.bedtemp_actual, 10) + "°C";
  String text_bed_target_temp = String(pValues.bedtemp_target, 10) + "°C";

  lv_style_value_t v_off = {.color = COLOR_OFF};
  lv_style_value_t v_on = {.color = COLOR_ON};

  // Bed Temp

  if (pValues.bedtemp_actual < 50 && pValues.bedtemp_target == 0)
  {
    lv_obj_set_local_style_prop(bed_cur_temp_label, LV_STYLE_TEXT_COLOR, v_off, LV_PART_MAIN);
  }
  else
  {
    lv_obj_set_local_style_prop(bed_cur_temp_label, LV_STYLE_TEXT_COLOR, v_on, LV_PART_MAIN);
  }

  lv_label_set_text(bed_cur_temp_label, text_bed_actual_temp.c_str());
  lv_label_set_text(bed_target_temp_label, text_bed_target_temp.c_str());

  if (pValues.bedtemp_target == 0)
  {
    lv_obj_add_flag(bed_target_temp_label, LV_OBJ_FLAG_HIDDEN);
  }
  else
  {
    lv_obj_clear_flag(bed_target_temp_label, LV_OBJ_FLAG_HIDDEN);
  }

  // Tool Temp

  if (pValues.tooltemp_actual < 50 && pValues.tooltemp_target == 0)
  {
    lv_obj_set_local_style_prop(tool_cur_temp_label, LV_STYLE_TEXT_COLOR, v_off, LV_PART_MAIN);
  }
  else
  {
    lv_obj_set_local_style_prop(tool_cur_temp_label, LV_STYLE_TEXT_COLOR, v_on, LV_PART_MAIN);
  }

  lv_label_set_text(tool_cur_temp_label, text_ext_actual_temp.c_str());
  lv_label_set_text(tool_target_temp_label, text_ext_target_temp.c_str());

  if (pValues.tooltemp_target == 0)
  {
    lv_obj_add_flag(tool_target_temp_label, LV_OBJ_FLAG_HIDDEN);
  }
  else
  {
    lv_obj_clear_flag(tool_target_temp_label, LV_OBJ_FLAG_HIDDEN);
  }

  // Layer info (A1 has no chamber temp)
  if (pValues.layer_current == 0 && pValues.layer_total == 0)
  {
    lv_obj_set_local_style_prop(label_chamber_temp, LV_STYLE_TEXT_COLOR, v_off, LV_PART_MAIN);
    lv_label_set_text(label_chamber_temp, "-");
    lv_label_set_text(label_layer_total, "-");
  }
  else
  {
    lv_obj_set_local_style_prop(label_chamber_temp, LV_STYLE_TEXT_COLOR, v_on, LV_PART_MAIN);
    lv_label_set_text(label_chamber_temp, String(pValues.layer_current, 10).c_str());
    if (pValues.layer_total > 0)
    {
      lv_label_set_text(label_layer_total, String(pValues.layer_total, 10).c_str());
      lv_obj_clear_flag(label_layer_total, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
      lv_label_set_text(label_layer_total, "-");
      lv_obj_add_flag(label_layer_total, LV_OBJ_FLAG_HIDDEN);
    }
  }

  // Message
  String currentMessage = pValues.message;
  currentMessage.trim();
  if (currentMessage.length() > 0 && currentMessage != "null")
  {
    latchedCenterStatus = currentMessage;
  }

  if (latchedCenterStatus.length() > 0)
  {
    lv_label_set_text(label_message, latchedCenterStatus.c_str());
  }

  int16_t progress_data = 0;

  // Progress

  String nameStrpriting = "0";
  progress_data = (int16_t)pValues.progress;
  if (progress_data < 0)
  {
    progress_data = 0;
  }
  if (progress_data > 100)
  {
    progress_data = 100;
  }
  uint16_t datas = (uint16_t)(progress_data);

  if (datas == 0)
  {
    nameStrpriting = "0%";
  }
  else
  {
    nameStrpriting = String(datas, 10) + "%";
  }
  lv_color_t progress_color = lv_color_hex(0x505050);
  if (progress_data >= 1 && progress_data <= 25)
  {
    progress_color = lv_color_hex(0xff7a00);
  }
  else if (progress_data > 25 && progress_data <= 50)
  {
    progress_color = lv_color_hex(0x00c8ff);
  }
  else if (progress_data > 50 && progress_data <= 75)
  {
    progress_color = lv_color_hex(0xffd400);
  }
  else if (progress_data > 75)
  {
    progress_color = lv_color_hex(0x00ff00);
  }
  lv_obj_set_style_arc_color(progress_arc, progress_color, LV_PART_INDICATOR);
  updateWifiIndicator();

  lv_label_set_text(progress_label, nameStrpriting.c_str());

  // FAN Speed

  if (pValues.fan_speed == 0)
  {
    lv_obj_set_local_style_prop(label_fan_speed, LV_STYLE_TEXT_COLOR, v_off, LV_PART_MAIN);
  }
  else
  {
    lv_obj_set_local_style_prop(label_fan_speed, LV_STYLE_TEXT_COLOR, v_on, LV_PART_MAIN);
  }
  lv_label_set_text(label_fan_speed, (String(pValues.fan_speed, 10) + "%").c_str());
  lv_arc_set_value(arc_fan, pValues.fan_speed);
  lv_obj_align_to(arc_fan, label_fan_speed, LV_ALIGN_OUT_RIGHT_MID, -12, 13);

  // Screentype

  bool showProgress = pValues.is_printing || progress_data > 0;
  bool hasMessage = latchedCenterStatus.length() > 0;
  if (!pValues.has_error && latchedCenterStatus.startsWith("ERROR"))
  {
    latchedCenterStatus = "";
  }

  bool hasErrorState = pValues.has_error;

  if (hasErrorState)
  {
    setCenterLabelsTheme(lv_color_hex(0xff0000));
    lv_label_set_text(label_message, "Error");
    lv_obj_add_flag(progress_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(label_message, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_opa(progress_label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_text_opa(label_message, LV_OPA_COVER, LV_PART_MAIN);
    centerFadeInitialized = false;
  }
  else if (showProgress)
  {
    setCenterLabelsTheme(progress_color);
    lv_obj_clear_flag(progress_arc, LV_OBJ_FLAG_HIDDEN);

    unsigned long now = millis();
    bool toggledCenter = false;
    if (now - centerToggleLastMs >= centerToggleMs)
    {
      centerToggleLastMs = now;
      centerShowProgress = !centerShowProgress;
      toggledCenter = true;
    }

    if (hasMessage)
    {
      lv_obj_clear_flag(progress_label, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(label_message, LV_OBJ_FLAG_HIDDEN);

      if (!centerFadeInitialized)
      {
        lv_obj_set_style_text_opa(progress_label, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_text_opa(label_message, LV_OPA_TRANSP, LV_PART_MAIN);
        centerFadeInitialized = true;
        centerShowProgress = true;
        toggledCenter = true;
      }

      if (centerShowProgress)
      {
        if (toggledCenter)
        {
          latchedArcValue = progress_data;
          animate_progress_arc(progress_arc, latchedArcValue, 900);
        }
        else
        {
          lv_arc_set_value(progress_arc, latchedArcValue);
        }
        animate_label_fade(progress_label, lv_obj_get_style_text_opa(progress_label, LV_PART_MAIN), LV_OPA_COVER, centerFadeMs);
        animate_label_fade(label_message, lv_obj_get_style_text_opa(label_message, LV_PART_MAIN), LV_OPA_TRANSP, centerFadeMs);
      }
      else
      {
        lv_arc_set_value(progress_arc, latchedArcValue);
        animate_label_fade(progress_label, lv_obj_get_style_text_opa(progress_label, LV_PART_MAIN), LV_OPA_TRANSP, centerFadeMs);
        animate_label_fade(label_message, lv_obj_get_style_text_opa(label_message, LV_PART_MAIN), LV_OPA_COVER, centerFadeMs);
      }
    }
    else
    {
      latchedArcValue = progress_data;
      lv_arc_set_value(progress_arc, latchedArcValue);
      lv_obj_clear_flag(progress_label, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(label_message, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_style_text_opa(progress_label, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_text_opa(label_message, LV_OPA_TRANSP, LV_PART_MAIN);
    }
  }
  else
  {
    centerFadeInitialized = false;
    setCenterLabelsTheme(lv_color_hex(0xa0a0a0));
    latchedArcValue = 0;
    lv_arc_set_value(progress_arc, 0);
    lv_obj_add_flag(progress_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(progress_arc, LV_OBJ_FLAG_HIDDEN);
    if (hasMessage)
    {
      lv_obj_clear_flag(label_message, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
      lv_obj_add_flag(label_message, LV_OBJ_FLAG_HIDDEN);
    }
  }

  // Screensaver logic

  if (lastMessage != pValues.message)
  {
    lastUpdate = millis();
    lastMessage = pValues.message;
  }

  if (pValues.is_printing || pValues.bedtemp_actual > 50 || pValues.bedtemp_target > 0 || pValues.tooltemp_target > 0 || pValues.tooltemp_actual > 50)
  {
    lastUpdate = millis();
  }

  // Screensaver activate

  if (millis() - lastUpdate > 1000 * 60 * 10)
  {
    if (!screensaver_active)
    {
      screensaver_active = true;
      lv_display_led_Off();
      lv_scr_load(screen3);
    }
  }
  else
  {
    if (screensaver_active)
    {
      lv_scr_load(screen2);
      screensaver_active = false;
      lv_display_led_On();
    }
  }
}