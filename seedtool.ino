// Copyright 2019 Bonsai Software, Inc.  All Rights Reserved.

#include <bip39.h>
#include <GxEPD2_BW.h>
#include <Keypad.h>

#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>

extern "C" {
#include <slip39.h>
#include <slip39_wordlist.h>
}

#define ESP32 1

#if defined(SAMD51)
extern "C" {
#include "trngFunctions.h"
}
#endif

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

    Serial.println("seedtool starting");

#if defined(SAMD51)
    trngInit();
#endif
    
    // no_input_or_display();
    verify_slip39_multilevel();

    reset_state();
}

void loop() {
    // make_bip39_key();
    recover_slip39();
}

int g_wordndx[20] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, };

int g_ndx = 0;
int g_pos = 0;
int g_scroll = 0;
    
void recover_slip39() {
    display_words();
    Serial.println("reading keypad");
    char key;
    do {
        key = g_keypad.getKey();
    } while (key == NO_KEY);
    Serial.println("keypad saw " + String(key));
    switch (key) {
    case NO_KEY:
        break;
    case '4':
        move_left();
        break;
    case '6':
        move_right();
        break;
    case '2':
        jump_down();
        break;
    case '8':
        jump_up();
        break;
    case '1':
        prev_entry();
        break;
    case '7':
        next_entry();
        break;
    case '5':
        next_entry();
        break;
    default:
        break;
    }
}

String prefix(int word) {
    return String(slip39_wordlist[word]).substring(0, g_pos);
}

char current(int word) {
    return slip39_wordlist[word][g_pos];
}

bool unique_match() {
    // Does the previous word also match?
    if (g_wordndx[g_ndx] > 0)
        if (prefix(g_wordndx[g_ndx] - 1) == prefix(g_wordndx[g_ndx]))
            return false;
    // Does the next word also match?
    if (g_wordndx[g_ndx] < 1023)
        if (prefix(g_wordndx[g_ndx] + 1) == prefix(g_wordndx[g_ndx]))
            return false;
    return true;
}

void move_left() {
    if (g_pos >= 1)
        --g_pos;
}

void move_right() {
    if (g_pos < strlen(slip39_wordlist[g_wordndx[g_ndx]]) - 1)
        ++g_pos;
}

void jump_down() {
    // Find the previous word that differs in the cursor position.
    int wordndx0 = g_wordndx[g_ndx];	// remember starting wordndx
    String prefix0 = prefix(g_wordndx[g_ndx]);
    char curr0 = current(g_wordndx[g_ndx]);
    do {
        if (g_wordndx[g_ndx] == 0)
            g_wordndx[g_ndx] = 1023;
        else
            --g_wordndx[g_ndx];
        // If we've returned to the original there are no choices.
        if (g_wordndx[g_ndx] == wordndx0)
            break;
    } while (prefix(g_wordndx[g_ndx]) != prefix0 ||
             current(g_wordndx[g_ndx]) == curr0);
}

void jump_up() {
    // Find the next word that differs in the cursor position.
    int wordndx0 = g_wordndx[g_ndx];	// remember starting wordndx
    String prefix0 = prefix(g_wordndx[g_ndx]);
    char curr0 = current(g_wordndx[g_ndx]);
    do {
        if (g_wordndx[g_ndx] == 1023)
            g_wordndx[g_ndx] = 0;
        else
            ++g_wordndx[g_ndx];
        // If we've returned to the original there are no choices.
        if (g_wordndx[g_ndx] == wordndx0)
            break;
    } while (prefix(g_wordndx[g_ndx]) != prefix0 ||
             current(g_wordndx[g_ndx]) == curr0);
}

void prev_entry() {
    if (g_ndx == 0)
        g_ndx = 19;
    else
        --g_ndx;
    g_pos = 0;
}

void next_entry() {
    if (g_ndx == 19)
        g_ndx = 0;
    else
        ++g_ndx;
    g_pos = 0;
}

void display_words() {
    int xoff = 16;
    // int yoff = 95;
    int yoff = 25;
    int width = 14;
    int height = 20;
    int yborder = 4;
    int nrows = 6;

    compute_scroll();
    
    g_display.firstPage();
    do
    {
        g_display.setRotation(1);
        g_display.setPartialWindow(0, 0, 200, 200);
        g_display.fillScreen(GxEPD_WHITE);
        g_display.setFont(&FreeMonoBold12pt7b);

        for (int rr = 0; rr < nrows; ++rr) {
            int wndx = g_scroll + rr;
            String word = String(slip39_wordlist[g_wordndx[wndx]]);
            Serial.println(String(wndx) + " " + word);

            int yy = yoff + (rr * (height + yborder));
            
            if (wndx != g_ndx) {
                // Regular entry, not being edited
                g_display.setTextColor(GxEPD_BLACK);
                g_display.setCursor(xoff, yy);
                g_display.printf("%2d %s\n", wndx+1, word.c_str());
            } else {
                // Edited entry
                if (unique_match()) {
                    // Unique, highlight entire word.
                    g_display.fillRect(xoff,
                                       yy - height + yborder,
                                       width * (word.length() + 3),
                                       height,
                                       GxEPD_BLACK);
        
                    g_display.setTextColor(GxEPD_WHITE);
                    g_display.setCursor(xoff, yoff);

                    g_display.printf("%2d %s\n", wndx+1, word.c_str());

                } else {
                    // Not unique, highlight cursor.
                    g_display.setTextColor(GxEPD_BLACK);
                    g_display.setCursor(xoff, yy);
            
                    g_display.printf("%2d %s\n", wndx+1, word.c_str());

                    g_display.fillRect(xoff + (g_pos+3)*width,
                                       yy - height + yborder,
                                       width, height,
                                       GxEPD_BLACK);
        
                    g_display.setTextColor(GxEPD_WHITE);
                    g_display.setCursor(xoff + (g_pos+3)*width, yy);
                    g_display.printf("%c", word.c_str()[g_pos]);
                }
            }
        }
        
    }
    while (g_display.nextPage());
}

void compute_scroll() {
    if (g_ndx < 3)
        g_scroll = 0;
    else if (g_ndx > 17)
        g_scroll = 14;
    else
        g_scroll = g_ndx - 3;
}

void make_bip39_key() {
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
        Serial.println("keypad saw " + String(key));

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
        Serial.println("g_rolls: " + g_rolls);
    } else {
        display_wordlist();

        make_slip39_wordlist();
        
        // Wait for a keypress
        char key;
        do {
            key = g_keypad.getKey();
        } while (key == NO_KEY);
        Serial.println("keypad saw " + String(key));

        reset_state();
    }
}

void no_input_or_display() {
    g_rolls = "123456";
    
    generate_key();
            
    for (int ndx = 0; ndx < 12; ndx += 2) {
        int col = 15;
        int row = ndx*10 + 40;
        String word0 = g_bip39.getMnemonic(g_bip39.getWord(ndx));
        String word1 = g_bip39.getMnemonic(g_bip39.getWord(ndx+1));
        Serial.println(word0 + " " + word1);
    }
    
    make_slip39_wordlist();
}

void reset_state() {
    g_rolls = "";
    g_submitted = false;
}

uint8_t g_master_secret[16];

void generate_key() {
    Sha256Class sha256;
    sha256.init();
    for(uint8_t ii=0; ii < g_rolls.length(); ii++) {
        sha256.write(g_rolls[ii]);
    }
    memcpy(g_master_secret, sha256.result(), sizeof(g_master_secret));
    g_bip39.setPayloadBytes(sizeof(g_master_secret));
    g_bip39.setPayload(sizeof(g_master_secret), (uint8_t *)g_master_secret);
    for (int ndx = 0; ndx < 12; ++ndx) {
        uint16_t word = g_bip39.getWord(ndx);
        Serial.println(String(ndx) + " " + String(g_bip39.getMnemonic(word)));
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

void make_slip39_wordlist() {
    int gt = 3;
    int gn = 5;
    
    char *ml[gn];
    
    for (int ii = 0; ii < gn; ++ii)
        ml[ii] = (char *)malloc(MNEMONIC_LIST_LEN);

    Serial.println("calling generate_mnemonics");
    generate_mnemonics(gt, gn,
                       g_master_secret, sizeof(g_master_secret),
                       NULL, 0, 0, ml);

    for (int ii = 0; ii < gn; ++ii)
        Serial.println(String(ii) + " " + String(ml[ii]));

    // Combine 2, 0, 4 to recover the secret.
    char *dml[5];
    dml[0] = ml[2];
    dml[1] = ml[0];
    dml[2] = ml[4];
    
    uint8_t ms[16];
    int msl;
    msl = sizeof(ms);
    combine_mnemonics(gt, dml, NULL, 0, ms, &msl);

    for (int ii = 0; ii < gn; ++ii)
        free(ml[ii]);
    
    if (msl == sizeof(g_master_secret) &&
        memcmp(g_master_secret, ms, msl) == 0) {
        Serial.println("SUCCESS");
    } else {
        Serial.println("FAIL");
    }
}

void verify_slip39_multilevel() {
    int gt = 5;
    char* ml[5] = {
                      "eraser senior decision roster beard "
                      "treat identify grumpy salt index "
                      "fake aviation theater cubic bike "
                      "cause research dragon emphasis counter",

                      "eraser senior ceramic snake clay "
                      "various huge numb argue hesitate "
                      "auction category timber browser greatest "
                      "hanger petition script leaf pickup",
                      
                      "eraser senior ceramic shaft dynamic "
                      "become junior wrist silver peasant "
                      "force math alto coal amazing "
                      "segment yelp velvet image paces",
#if 0
                      // Duplicate of the prior, should fail.
                      "eraser senior ceramic shaft dynamic "
                      "become junior wrist silver peasant "
                      "force math alto coal amazing "
                      "segment yelp velvet image paces",
#else
                      // This one works
                      "eraser senior ceramic round column "
                      "hawk trust auction smug shame "
                      "alive greatest sheriff living perfect "
                      "corner chest sled fumes adequate",
#endif
                      
                      "eraser senior decision smug corner "
                      "ruin rescue cubic angel tackle "
                      "skin skunk program roster trash "
                      "rumor slush angel flea amazing",
    };

    uint8_t ms[16];
    int msl;
    msl = sizeof(ms);
    int rc = combine_mnemonics(gt, (char**) ml, NULL, 0, ms, &msl);

    if (rc == 0) {
        Serial.println("VERIFIED " + String(rc));
    } else {
        Serial.println("FAILED " + String(rc));
    }
}


extern "C" {
void random_buffer(uint8_t *buf, size_t len) {
    printf("random_buffer %d\n", len);
    uint32_t r = 0;
    for (size_t i = 0; i < len; i++) {
        if (i % 4 == 0) {
#if defined(ESP32)
            r = esp_random();
#elif defined(SAMD51)
            r = trngGetRandomNumber();
#endif
        }
        buf[i] = (r >> ((i % 4) * 8)) & 0xFF;
    }
}

void debug_print(char const * str) {
    Serial.println(str);
}
}
