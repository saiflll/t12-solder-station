#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// wifi set
#define WIFI_SSID "Your_SSID"
#define WIFI_PASS "Your_Password"
#define SERVER_URL "http://192.168.1.100:3000/post" 

// pins
#define OLED_SDA 22
#define OLED_SCL 21
#define ROT_CLK 34  
#define ROT_DT 35
#define ROT_SW 25  
#define T12_SW 33   
#define PIN_PWM 18  
#define PIN_DS18 23 
#define PIN_LM358 26 
#define PIN_VOLT 32  

// div R
#define R1 10000.0 
#define R2 1500.0  

// sys def
#define DEF_PWM 30
#define DEF_FREQ 2000
#define DEF_DELAY 50
#define T_THRESHOLD 5
#define T_HOLD 1000
#define COLD_MS 300000 

struct SystemSettings {
    int pwm;          
    int freq;         
    int loopDelay;    
};

#endif
