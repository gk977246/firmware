#include "core/powerSave.h"
#include "core/utils.h" //added//
#include <Arduino.h>   //added///
#include <Wire.h>
#include <globals.h> //caution added!!!!//
#include <interface.h>

/***************************************************************************************
** Function name: _setup_gpio()
** Location: main.cpp
** Description:   initial setup for the device
***************************************************************************************/

void _setup_gpio() {
    // Set buttons to use internal pullup resistors (for direct GND wiring)
    pinMode(UP_BTN, INPUT_PULLUP);
    pinMode(SEL_BTN, INPUT_PULLUP);
    pinMode(DW_BTN, INPUT_PULLUP);
    pinMode(R_BTN, INPUT_PULLUP);
    pinMode(L_BTN, INPUT_PULLUP);
    pinMode(ESC_BTN, INPUT_PULLUP); // added//
    pinMode(NRF24_CE_PIN, OUTPUT);

    // *** ADD THESE TWO LINES HERE - CC1101 GDO PINS ***
    pinMode(CC1101_GDO0_PIN, INPUT); // Pin 9 - CRITICAL for CC1101!
    pinMode(CC1101_GDO2_PIN, INPUT); // Pin 10 - CRITICAL for CC1101!
    // *** END OF NEW LINES ***

    pinMode(NRF24_SS_PIN, OUTPUT);
    pinMode(CC1101_SS_PIN, OUTPUT);
    pinMode(SDCARD_CS, OUTPUT);
    pinMode(TFT_CS, OUTPUT);

    digitalWrite(CC1101_SS_PIN, HIGH);
    digitalWrite(NRF24_SS_PIN, HIGH);
    digitalWrite(SDCARD_CS, HIGH);
    digitalWrite(TFT_CS, HIGH);
    digitalWrite(NRF24_CE_PIN, LOW);

    bruceConfigPins.rfModule = CC1101_SPI_MODULE;
    bruceConfigPins.rfidModule = PN532_I2C_MODULE; // Keep as bruceConfigPins
    bruceConfigPins.irRx = RXLED;
    bruceConfigPins.irTx = TXLED;
    bruceConfig.startupApp = "WebUI"; // This one uses bruceConfig (not Pins)

    // Start I2C for the PN532 only
    Wire.setPins(GROVE_SDA, GROVE_SCL);
    Wire.begin(GROVE_SDA, GROVE_SCL);
}

bool isCharging() {
    return false; // No chip to report charging
}

// int getBattery()
//{
//   return 100; // Always report 100% to keep UI happy
//}
// new charging code added below//
int getBattery() {
    static bool adcInitialized = false;
    if (!adcInitialized) {
        pinMode(ANALOG_BAT_PIN, INPUT);
        analogSetAttenuation(ADC_11db); // Full range for 0-3.3V input
        adcInitialized = true;
    }

    // Read ADC and convert to actual battery voltage (with x2 divider)
    uint32_t adcReading = analogReadMilliVolts(ANALOG_BAT_PIN);
    float actualVoltage = (float)adcReading * 2.0f; // x2 voltage divider

    // Battery voltage range per ES3C28P specs:
    // Min: 2500mV (cutoff/empty), Max: 4200mV (fully charged)
    const float MIN_VOLTAGE = 3000.0f;
    const float MAX_VOLTAGE = 4200.0f;

    int percent = (int)(((actualVoltage - MIN_VOLTAGE) / (MAX_VOLTAGE - MIN_VOLTAGE)) * 100.0f);

    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    return percent;
}

/*********************************************************************
** Function: setBrightness
** location: settings.cpp
** set brightness value
**********************************************************************/
void _setBrightness(uint8_t brightval) {
    if (brightval == 0) {
        analogWrite(TFT_BL, brightval);
    } else {
        int bl = MINBRIGHT + round(((255 - MINBRIGHT) * brightval / 100));
        analogWrite(TFT_BL, bl);
    }
}

/*********************************************************************
** Function: InputHandler
** Handles the variables PrevPress, NextPress, SelPress, AnyKeyPress and EscPress
**********************************************************************/
void InputHandler(void) {
    static unsigned long tm = 0;
    if (millis() - tm < 225 && !LongPress)
        ///////edit here for button issue????????/////////   trying 125, 150 ok///
        return;

    // DIY Buttons: Pressed = LOW (Connected to Ground)
    bool _u = (digitalRead(UP_BTN) == LOW);
    bool _d = (digitalRead(DW_BTN) == LOW);
    bool _l = (digitalRead(L_BTN) == LOW);
    bool _r = (digitalRead(R_BTN) == LOW);
    bool _e = (digitalRead(ESC_BTN) == LOW);
    bool _s = (digitalRead(SEL_BTN) == LOW);

    if (_s || _u || _d || _r || _l || _e) {
        tm = millis();
        if (!wakeUpScreen()) AnyKeyPress = true;
        else return;
    }
    if (_l) { PrevPress = true; }
    if (_r) { NextPress = true; }
    if (_u) {
        UpPress = true;
        PrevPagePress = true;
    }
    if (_d) {
        DownPress = true;
        NextPagePress = true;
    }
    if (_s) { SelPress = true; }
    if (_e) {
        EscPress = true;
        // NextPress = false;
        // PrevPress = false;
    }
}

/*********************************************************************
** Function: powerOff
** location: mykeyboard.cpp
** Turns off the device (or try to)
**********************************************************************/
void powerOff() {
    esp_sleep_enable_ext0_wakeup((gpio_num_t)SEL_BTN, BTN_ACT);
    esp_deep_sleep_start();
}

/*********************************************************************
** Function: checkReboot
** location: mykeyboard.cpp
** Btn logic to tornoff the device (name is odd btw)
**********************************************************************/
void checkReboot() {
    int countDown;
    /* Long press power off */
    if (digitalRead(L_BTN) == BTN_ACT && digitalRead(R_BTN) == BTN_ACT) {
        uint32_t time_count = millis();
        while (digitalRead(L_BTN) == BTN_ACT && digitalRead(R_BTN) == BTN_ACT) {
            // Display poweroff bar only if holding button
            if (millis() - time_count > 500) {
                tft.setTextSize(1);
                tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
                countDown = (millis() - time_count) / 1000 + 1;
                if (countDown < 4)
                    tft.drawCentreString("PWR OFF IN " + String(countDown) + "/3", tftWidth / 2, 12, 1);
                else {
                    tft.fillScreen(bruceConfig.bgColor);
                    while (digitalRead(L_BTN) == BTN_ACT || digitalRead(R_BTN) == BTN_ACT);
                    delay(200);
                    powerOff();
                }
                delay(10);
            }
        }

        // Clear text after releasing the button
        delay(30);
        tft.fillRect(60, 12, tftWidth - 60, tft.fontHeight(1), bruceConfig.bgColor);
    }
}
