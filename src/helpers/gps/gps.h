#pragma once

#include <MicroNMEA.h>

extern MicroNMEA nmea;

extern void gps_setup(HardwareSerial& ser);
extern void gps_feed_nmea();
