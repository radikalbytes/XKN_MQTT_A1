#ifndef __PRINTER_H__
#define __PRINTER_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <Arduino.h>


struct printer_values
{
  uint16_t bedtemp_actual = 0;
  uint16_t bedtemp_target = 0;
  uint16_t tooltemp_actual = 0;
  uint16_t tooltemp_target = 0;
  uint16_t layer_current = 0;
  uint16_t layer_total = 0;
  // uint16_t last_tooltemp_target = 0;
  // uint16_t last_bedtemp_target = 0;
  double progress = 0;
  uint16_t chamber_temp = 0;
  String message = "";
  char *message2;
  uint16_t fan_speed = 0;
  bool is_printing = false;
  bool has_error = false;
};

void get_printer_progress(printer_values *spValues);
void get_printer_status(printer_values *spValues);
void printer_mqtt_init();
void printer_mqtt_loop();

#endif