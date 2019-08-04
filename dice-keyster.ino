// Copyright 2019 Bonsai Software, Inc.  All Rights Reserved.

#include <bip39.h>
#include <GxEPD2_BW.h>
#include <Keypad.h>

#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>

#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>

// Display
GxEPD2_BW<GxEPD2_154, GxEPD2_154::HEIGHT>
    g_display(GxEPD2_154(/*CS=*/   21,
                         /*DC=*/   17,
                         /*RST=*/  16,
                         /*BUSY=*/ 4));

// Keypad
const byte rows_ = 4;
const byte cols_ = 4;
char keys_[rows_][cols_] = {
                         {'1','2','3','A'},
                         {'4','5','6','B'},
                         {'7','8','9','C'},
                         {'*','0','#','D'}
};
byte rowPins_[rows_] = {13, 12, 27, 33};
byte colPins_[cols_] = {15, 32, 14, 22};
Keypad g_keypad = Keypad(makeKeymap(keys_), rowPins_, colPins_, rows_, cols_);

String g_rolls;
bool g_submitted;
Bip39 g_bip39;

void setup() {
    pinMode(25, OUTPUT);	// Blue LED
    digitalWrite(25, HIGH);
    
    pinMode(26, OUTPUT);	// Green LED
    digitalWrite(26, LOW);
    
    g_display.init(115200);

    Serial.begin(115200);
    while (!Serial);	// wait for serial to come online

    reset_state();
}

void loop() {
    if (g_rolls.length() * 2.5850 >= 128.0) {
        digitalWrite(26, HIGH);
    } else {
        digitalWrite(26, LOW);
    }
    
    display_status();
        
    if (!g_submitted) {
        char key;
        do {
            key = g_keypad.getKey();
        } while (key == NO_KEY);
        Serial.printf("keypad saw %c\n", key);

        switch (key) {
        case NO_KEY:
            break;
        case 'A': case 'B': case 'C': case 'D':
            break;
        case '1': case '2': case '3':
        case '4': case '5': case '6':
            g_rolls += key;
            break;
        case '*':
            g_rolls = "";
            break;
        case '#':
            g_submitted = true;
            generate_key();
            break;
        default:
            break;
        }
        Serial.printf("g_rolls: %s\n", g_rolls.c_str());
    } else {
        display_wordlist();
        
        // Wait for a keypress
        char key;
        do {
            key = g_keypad.getKey();
        } while (key == NO_KEY);
        Serial.printf("keypad saw %c\n", key);

        reset_state();
    }
}

void reset_state() {
    g_rolls = "";
    g_submitted = false;
}

void generate_key() {
    Sha256Class sha256;
    sha256.init();
    for(uint8_t ii=0; ii < g_rolls.length(); ii++) {
        sha256.write(g_rolls[ii]);
    }
    uint8_t payload[16];
    memcpy(payload, sha256.result(), sizeof(payload));
    g_bip39.setPayloadBytes(sizeof(payload));
    g_bip39.setPayload(sizeof(payload), (uint8_t *)payload);
    for (int ndx = 0; ndx < 12; ++ndx) {
        uint16_t word = g_bip39.getWord(ndx);
        Serial.printf("%d %s\n", ndx, g_bip39.getMnemonic(word));
    }
}

void display_status() {
    g_display.firstPage();
    do
    {
        g_display.setRotation(1);
        g_display.setPartialWindow(0, 0, 200, 200);
        g_display.fillScreen(GxEPD_WHITE);
        g_display.setTextColor(GxEPD_BLACK);
        
        g_display.setFont(&FreeSansBold12pt7b);
        g_display.setCursor(2, 30);
        g_display.println("BIP39 Generator");
        g_display.setCursor(2, 60);
        g_display.println("Enter Dice Rolls");
        
        g_display.setFont(&FreeMonoBold12pt7b);
        g_display.setCursor(10, 95);
        g_display.printf("Rolls: %d\n", g_rolls.length());
        g_display.setCursor(10, 123);
        g_display.printf(" Bits: %0.1f\n", g_rolls.length() * 2.5850);
        
        g_display.setFont(&FreeSansBold9pt7b);
        g_display.setCursor(0, 160);
        g_display.println("   Press * to clear");
        g_display.println("   Press # to submit");
    }
    while (g_display.nextPage());
}

void display_wordlist() {
    g_display.firstPage();
    do
    {
        g_display.setRotation(1);
        g_display.setPartialWindow(0, 0, 200, 200);
        g_display.fillScreen(GxEPD_WHITE);
        g_display.setFont(&FreeSansBold9pt7b);
        for (int ndx = 0; ndx < 12; ndx += 2) {
            int col = 15;
            int row = ndx*10 + 40;
            String word0 = g_bip39.getMnemonic(g_bip39.getWord(ndx));
            String word1 = g_bip39.getMnemonic(g_bip39.getWord(ndx+1));
            g_display.setCursor(col, row);
            g_display.printf("%s  %s", word0.c_str(), word1.c_str());
        }
        g_display.setFont(&FreeSansBold9pt7b);
        g_display.setCursor(20, 180);
        g_display.println("Press * to clear");
    }
    while (g_display.nextPage());
}
