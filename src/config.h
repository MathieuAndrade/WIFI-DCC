/**
 * @file config.h
 * @brief Configuration file for DCC++/DCCpp/DCCppS88 WiFi
 * @date 2021-01-02
 * @details This file contains the configuration for DCC++/DCCpp/DCCppS88 WiFi.
 *          It is used by the main program and the webserver.
 *          You can change the settings here.
*/

const char *ssid = "";
const char *password = "";

IPAddress local_IP(192, 168, 1, 90);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

// LiquidCrystal_I2C lcd(0x27, 16, 2); // 2x16 display, adress of 1602A LCD = 0x20..0x27
LiquidCrystal_I2C lcd(0x27, 20, 4); // 4x20 display, adress of 2004A LCD = // 0x38..0x3F
// LiquidCrystal_I2C lcd(0x3F, 20, 4);   // 4x20 display, adress of 2004A LCD = 0x38..0x3F

#define PROJECT "Controller DCC++/DCCpp/DCCppS88 WiFi"
#define NAME "DCCppS88 WiFi "
#define VERSION "V1.1"

#define DCCSerial Serial   // used to transmit data with the MEGA
#define pinLedD5 14        // D5, connected to onboard LED on MEGA R3 WiFi board
#define emergencyButton 12 // D6, LOW active
#define pinD7 13           // D7, connected to MODE button on MEGA R3 WiFi board
#define pinD8 15           // D8, connected to GND on MEGA R3 WiFi board
#define AD_KEY A0          // Analog input, connected externally to a pullup resistor