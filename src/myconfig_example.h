#ifndef __MYCONFIG_H__
#define __MYCONFIG_H__


#include <string.h>
#include <stdlib.h>

static const char *ssid = "MyWIFI";
static const char *password = "MyPASSWORD";

static const char *mqtt_host = "Printer_IP";
static const char *mqtt_serial = "Serial_Printer";
static const char *mqtt_access_code = "Printer_Access_Code";
static const uint16_t mqtt_port = 8883;
static const bool mqtt_reject_unauthorized = false;

#endif