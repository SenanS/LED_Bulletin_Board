#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <P3RGB64x32MatrixPanel.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <string.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Ticker.h>
#include <AsyncTCP.h>

/*=== M A C R O S ===*/

/* Maximum amount of characters on the screen */
#define MAX_CHAR_WIDTH  11U
/* Width of each character */
#define TEXT_WIDTH      6U
/* Height of each character */
#define TEXT_HEIGHT     8U
/* Inversely proportional to the speed of the moving text */
#define CAROUSEL_DELAY  15U
/* Converter for milliseconds */
#define MILLI_SECOND    1000U
/* One Milli-minute (60 seconds in milliseconds)*/
#define MILLI_MINUTE    60000U
/* One Milli-hour (60 minutes in milliminutes)*/
#define MILLI_HOUR      3600000U
/* 08:00 in milliseconds */
#define EIGHT_AM        28800000U
/* 23:00 in milliseconds */
#define ELEVEN_PM       82800000U
/* 100 millisecond padding for API calls */
#define API_PADDING     100U


/*=== P R O T O T Y P E S ===*/

void core0Loop(void *unused);
void core1Loop(void *unused);
void causeTime();
void causeNightTime();
void drawDateAndTimeChars();
void printToScreen(const String ksMessage, const uint16_t knColour, const uint8_t knNumChars, const uint8_t knRow, const int knCursorCol, const uint8_t knClearCol);
void inline printToScreen(const String ksMessage, const uint16_t knColour, const uint8_t knNumChars, const uint8_t knRow, const int knCol);
void printRainbowBitmap(const unsigned char bitmap[], const uint16_t nCycles);
long HSBtoRGB(float _hue);
void setDateAndTime();
void blankAndDrawTime();
uint8_t compareStrings(String Str1, String Str2);
String LengthenStrings(String Str1);
void cycleMessage(const String ksMessage, const uint8_t knRow, const uint32_t knTextDelay);
void GetAPIRequestJSON(const String ksURL);
void createPizza(uint8_t nXMid, uint8_t nYMid);
void removeCircularSegment(uint8_t nTopLeftCol, uint8_t nTopLeftRow, uint8_t nWidth, uint8_t nHeight, double nFraction);
void rotateThroughTheta(uint8_t *x, uint8_t *y, uint8_t ox, uint8_t oy, double theta);

/*=== E N U M S ===*/

/* 4 LED text rows (must be multiplied by TEXT_HEIGHT) */
enum LEDRows {
    ROW_0,
    ROW_1,
    ROW_2,
    ROW_3
};

/* 2 ESP 32 cores */
enum ESP32Cores {
    CORE_0,
    CORE_1,
};

/*=== S T R U C T S ===*/

typedef struct TODO_TASKS {
    String      sLine1;             /* Text line 1 of the task */
    String      sLine2;             /* Text line 2 of the task */
    uint32_t    nMinsToComplete;    /* Time it takes to Complete (in minutes) */
    int8_t      nNumRepeats;        /* Number of times this can repeat in a day, (-1 infinite) */
} todo_tasks;

/*=== D A T A ===*/

const unsigned char youre_done_bitmap [] PROGMEM =
{
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xe7, 0xe7, 0x87, 0x9e, 0x71, 0xe0, 0x38, 0x07, 0xe7, 0xe7, 0x03, 0x9e, 0x71, 0xe0, 0x18, 0x07,
    0xe3, 0xc6, 0x79, 0x9e, 0x79, 0xe7, 0x99, 0xff, 0xf3, 0xce, 0x79, 0x9e, 0x79, 0xe7, 0x99, 0xff,
    0xf1, 0x8e, 0x79, 0x9e, 0x7f, 0xe0, 0x38, 0x1f, 0xf8, 0x1e, 0x79, 0x9e, 0x7f, 0xe0, 0x18, 0x1f,
    0xfc, 0x3e, 0x79, 0x9e, 0x7f, 0xe7, 0x99, 0xff, 0xfe, 0x7e, 0x79, 0x9e, 0x7f, 0xe7, 0x99, 0xff,
    0xfe, 0x7f, 0x03, 0xc0, 0xff, 0xe7, 0x98, 0x07, 0xfe, 0x7f, 0x87, 0xe1, 0xff, 0xe7, 0x98, 0x07,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xfc, 0x0f, 0xc3, 0xcf, 0x30, 0x0f, 0xff, 0xff, 0xfc, 0x07, 0x81, 0xc7, 0x30, 0x0f, 0xff,
    0xff, 0xfc, 0xf3, 0x3c, 0xc3, 0x33, 0xff, 0xff, 0xff, 0xfc, 0xf3, 0x3c, 0xc1, 0x33, 0xff, 0xff,
    0xff, 0xfc, 0xf3, 0x3c, 0xc8, 0x30, 0x3f, 0xff, 0xff, 0xfc, 0xf3, 0x3c, 0xcc, 0x30, 0x3f, 0xff,
    0xff, 0xfc, 0xf3, 0x3c, 0xce, 0x33, 0xff, 0xff, 0xff, 0xfc, 0xf3, 0x3c, 0xcf, 0x33, 0xff, 0xff,
    0xff, 0xfc, 0x07, 0x81, 0xcf, 0x30, 0x0f, 0xff, 0xff, 0xfc, 0x0f, 0xc3, 0xcf, 0x30, 0x0f, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

const unsigned char lunch_time_bitmap [] PROGMEM =
{
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x3f, 0xcf, 0x33, 0xcf, 0x0f, 0x3c, 0xff,
    0xff, 0x3f, 0xcf, 0x31, 0xce, 0x07, 0x3c, 0xff, 0xff, 0x3f, 0xcf, 0x30, 0xcc, 0xf3, 0x3c, 0xff,
    0xff, 0x3f, 0xcf, 0x30, 0x4c, 0xf3, 0x3c, 0xff, 0xff, 0x3f, 0xcf, 0x32, 0x0c, 0xff, 0x00, 0xff,
    0xff, 0x3f, 0xcf, 0x33, 0x0c, 0xff, 0x00, 0xff, 0xff, 0x3f, 0xcf, 0x33, 0x8c, 0xf3, 0x3c, 0xff,
    0xff, 0x3f, 0xcf, 0x33, 0xcc, 0xf3, 0x3c, 0xff, 0xff, 0x00, 0xe0, 0x73, 0xce, 0x07, 0x3c, 0xff,
    0xff, 0x00, 0xf0, 0xf3, 0xcf, 0x0f, 0x3c, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x03, 0x03, 0x3f, 0x30, 0x0f, 0xff,
    0xff, 0xf0, 0x03, 0x03, 0x1e, 0x30, 0x0f, 0xff, 0xff, 0xff, 0x3f, 0xcf, 0x0c, 0x33, 0xff, 0xff,
    0xff, 0xff, 0x3f, 0xcf, 0x00, 0x33, 0xff, 0xff, 0xff, 0xff, 0x3f, 0xcf, 0x21, 0x30, 0x3f, 0xff,
    0xff, 0xff, 0x3f, 0xcf, 0x33, 0x30, 0x3f, 0xff, 0xff, 0xff, 0x3f, 0xcf, 0x3f, 0x33, 0xff, 0xff,
    0xff, 0xff, 0x3f, 0xcf, 0x3f, 0x33, 0xff, 0xff, 0xff, 0xff, 0x3f, 0x03, 0x3f, 0x30, 0x0f, 0xff,
    0xff, 0xff, 0x3f, 0x03, 0x3f, 0x30, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};
// Array of all bitmaps for convenience. (Total bytes used to store images in PROGMEM = 272*2)
const int all_bitmaps_array_LEN = 2;
const unsigned char* all_bitmaps_array[2] =
{
    youre_done_bitmap,
    lunch_time_bitmap
};

const todo_tasks kaoTasksArray[13U] =
{
    {
        " Smile",   "  :D",     3U,  -1
    }, {
        "10 Deep",  "Breaths",  3U,  -1
    }, {
        " Relax",   "Muscles",  3U,  -1
    }, {
        "Stretch",  "   :)",    3U,  -1
    }, {
        " Sit Up",  "Straight", 3U,  -1
    }, {
        "  Drink",  "  Water",  5U,  -1
    }, {
        "Moistur-", "ise  :@",  5U,  2
    }, {
        "   10",    "Push-Ups", 7U,  2
    }, {
        "  Touch",  "  Grass",  7U,  2
    }, {
        "   Eat",   "  Fruit",  7U,  2
    }, {
        " Oil Up",  "That Bio", 7U,  1
    }, {
        "  Brew",   "  Tea",    10U, 1
    }, {
        "  Short",  "  Walk",   15U, 1
    }
};

/* Create two task objects */
TaskHandle_t Task1, Task2;
/* Declare a Mutex */
SemaphoreHandle_t oLEDMatrixMutex;

/* WiFiManager, Local intialization. Once its business is done, there is no need to keep it around */
WiFiManager wm;

/* String for storing server response */
String response = "";
/* JSON document */
DynamicJsonDocument doc(2048);

/* Affirmations website */
const String ksAffirmRequest = "https://www.affirmations.dev/";
/* Time & date website */
const String ksTimeRequest = "https://www.timeapi.io/api/Time/current/zone?timeZone=Europe/Dublin";

/* Default pin wiring constructor */
P3RGB64x32MatrixPanel matrix;
/* Custom pin wiring constructor */
/* P3RGB64x32MatrixPanel matrix(25, 26, 27, 21, 22, 23, 15, 32, 33, 12, 16, 17, 18); */

/* Colour Declarations */
uint16_t nBlack  = matrix.color444(0, 0, 0);
uint16_t nBlue   = matrix.color444(0, 0, 15);
uint16_t nGreen  = matrix.color444(0, 15, 0);
uint16_t nRed    = matrix.color444(15, 0, 0);
uint16_t nYellow = matrix.color444(15, 15, 0);
uint16_t nCyan   = matrix.color444(0, 15, 15);
uint16_t nPurple = matrix.color444(15, 0, 15);
uint16_t nWhite  = matrix.color444(15, 15, 15);
uint16_t nTODO   = matrix.color444(0, 5, 15);

/* Setting up the Clock ticker & nighttime ticker */
Ticker oTimeTicker(causeTime, MILLI_SECOND);
Ticker oNightTicker(causeNightTime, 10*MILLI_MINUTE, 0, MILLIS);

/*=== F U N C T I O N S ===*/

void setup()
{
    /* Set up serial comms */
    Serial.begin(115200);

    /* Create the Mutex */
    oLEDMatrixMutex = xSemaphoreCreateMutex();

    /*Initiate wifi with saved wifi credentials */
    bool bWifiStatus;
    bWifiStatus = wm.autoConnect(); /* Autoconnect to stored network */

    if(!bWifiStatus) {
        Serial.println("Connection Failed");
        /* ESP.restart(); */
    }
    else {
        Serial.println("Connection Successful");
    }

    /* Begin LED matrix */
    matrix.begin();

    /* Blanking & Text configuration */
    matrix.fillScreen(nBlack);
    matrix.setTextSize(1);     // size 1 == 8 pixels high
    matrix.setTextWrap(false); // Don't wrap at end of line - will do ourselves

    /* Draw a pizza (with 6 slices) in the middle-left area of the LED matrix */
//    createPizza(8U, 16U);
//    double nFraction = 1.0 / 6.0, nCircuit = 360.0;
//    for (double i = 0.0; i <= nCircuit; i = i + nFraction) {
//        removeCircularSegment(0U, 8U, 16U, 16U, i);
//    }
//    matrix.fillRect(0U, 8U, 17U, 16U, nBlack);
    uint16_t nGrey = matrix.color444(4U, 4U, 4U);
    uint16_t nBrown = matrix.color444(2U, 2U, 0);
    matrix.drawLine(0U, 8U, 16U, 8U, nBrown);
    matrix.drawLine(0U, 23U, 16U, 23U, nBrown);
    matrix.fillTriangle(0U, 9U, 16U, 9U, 8U, 16U, nGrey);
    matrix.fillTriangle(0U, 22U, 16U, 22U, 8U, 15U, nGrey);

    /* Draw the essential characters onto the panel */
    drawDateAndTimeChars();

    /* Start the Core 0 Time-tracking Ticker */
    oTimeTicker.start();
    /* Get the time for the nighttime timer from timeapi.io */
    GetAPIRequestJSON(ksTimeRequest);
    /* Start the Core 0 nighttime Ticker */
    oNightTicker.start();
    /* Schedule the next ticker call to 23:00 or now, if it's already nighttime */
    int32_t nTimeTilNight = ELEVEN_PM - ((MILLI_HOUR * doc["hour"].as<int>()) + (MILLI_MINUTE * doc["minute"].as<int>()));
    oNightTicker.interval((nTimeTilNight > 0) ? (nTimeTilNight) : (API_PADDING));

    /* Assigning tasks to each core */
    xTaskCreatePinnedToCore(core0Loop,  /* Function to implement the task */
                            "TimeTask", /* Name of the task */
                            5000,       /* Stack size in words */
                            NULL,       /* Task input parameter */
                            2,          /* Priority of the task */
                            &Task1,     /* Task handle. */
                            CORE_0);    /* Core where the task should run */
    xTaskCreatePinnedToCore(core1Loop, "AffirmTask", 5000, NULL, 2, &Task2, CORE_1);
}

void loop()
{}

void core0Loop(void *unused)
{
    uint8_t i = 0;
    /* Core 0 loop */
    for(;;)
    {
        /* Update the time til the next minute */
        oTimeTicker.update();
        /* Update the night timer during the day */
        oNightTicker.update();

//        if (i == 13U)
//        {
//            i = 0U;
//        }
//        printToScreen(kaoTasksArray[i].sLine1, nTODO, 8U, ROW_1*TEXT_HEIGHT, 17U);
//        printToScreen(kaoTasksArray[i].sLine2, nTODO, 8U, ROW_2*TEXT_HEIGHT, 17U);
//        i++;
//        delay(2000U);
    }
}

void core1Loop(void *unused)
{
    /* Core 1 loop */
    for(;;) {
        GetAPIRequestJSON(ksAffirmRequest);
        /* Message converted to string */
        String sMessage = doc["affirmation"].as<char*>();
        cycleMessage(sMessage, ROW_3, CAROUSEL_DELAY);
        delay(1);
    }
}

void causeTime()
{
    /* Get the time and date from timeapi.io */
    GetAPIRequestJSON(ksTimeRequest);
    /* Schedule the next update for just after the next minute */
    oTimeTicker.interval(MILLI_MINUTE - (MILLI_SECOND * doc["seconds"].as<int>()) + API_PADDING);
    /* Set the time and date on the display */
    setDateAndTime();
}

void causeNightTime()
{
    /* Nighty-Night */
    int32_t nTimeTilWake;
    uint32_t nTimeTilNextCycle;

    /* Blank the screen & print the nighttime message */
    matrix.fillScreen(nBlack);
    String nNightTimeMessageRow1 = "Night";
    String nNightTimeMessageRow2 = "  :)";
    matrix.setTextSize(2);
    /* Offset messages to give 3D effect */
    printToScreen(nNightTimeMessageRow1, nRed, 0U, ROW_0*TEXT_HEIGHT+1U, 2U);
    printToScreen(nNightTimeMessageRow2, nRed, 0U, ROW_2*TEXT_HEIGHT+1U, 2U);
    printToScreen(nNightTimeMessageRow1, nPurple, 0U, ROW_0*TEXT_HEIGHT, 3U);
    printToScreen(nNightTimeMessageRow2, nPurple, 0U, ROW_2*TEXT_HEIGHT, 3U);
    delay(5000U);

    /* Schedule the next ticker call to at 8:00 */
    nTimeTilWake = ((MILLI_HOUR * doc["hour"].as<int>()) + (MILLI_MINUTE * doc["minute"].as<int>()));
    /* Check if it's the same day as the sleep time */
    nTimeTilWake = (nTimeTilWake >= ELEVEN_PM) ? (nTimeTilWake - (24 * MILLI_HOUR)) : (nTimeTilWake);
    nTimeTilNextCycle = EIGHT_AM - nTimeTilWake;
    /* Time in microseconds */
    esp_sleep_enable_timer_wakeup(nTimeTilNextCycle*1000U);
    esp_deep_sleep_start();

}

void drawDateAndTimeChars()
{
    /* Gain exclusive access to the matrix */
    xSemaphoreTake(oLEDMatrixMutex, portMAX_DELAY);
    /* Draw date '|' character */
    matrix.drawLine(TEXT_WIDTH*2U, 0U, TEXT_WIDTH*2U, TEXT_HEIGHT-1U, nYellow);
    /* Draw time ':' character */
    matrix.setTextColor(nCyan);
    matrix.setCursor(64 - TEXT_WIDTH*3U+2, ROW_0);
    matrix.print(":");
    /* Relinquish exclusive access to the matrix */
    xSemaphoreGive(oLEDMatrixMutex);
}

void printToScreen(const String ksMessage, const uint16_t knColour, const uint8_t knNumChars, const uint8_t knRow, const int knCursorCol, const uint8_t knClearCol)
{
    /* Gain exclusive access to the matrix */
    xSemaphoreTake(oLEDMatrixMutex, portMAX_DELAY);
    /* Set text colour & blank the amount of chars input */
    matrix.setTextColor(knColour);
    matrix.fillRect(knClearCol, knRow, TEXT_WIDTH*knNumChars, TEXT_HEIGHT, nBlack);
    /* Position the cursor at the input position & print message */
    matrix.setCursor(knCursorCol, knRow);
    matrix.print(ksMessage);
    /* Relinquish exclusive access to the matrix */
    xSemaphoreGive(oLEDMatrixMutex);
}

void inline printToScreen(const String ksMessage, const uint16_t knColour, const uint8_t knNumChars, const uint8_t knRow, const int knCol)
{
    /* Overloaded function with one column value */
    printToScreen(ksMessage, knColour, knNumChars, knRow, knCol, knCol);
}

void printRainbowBitmap(const unsigned char bitmap[], const uint16_t nCycles)
{
    /* Gain exclusive access to the matrix */
    xSemaphoreTake(oLEDMatrixMutex, portMAX_DELAY);

    uint16_t nColour;
    long nColourPicked;

    for (uint16_t i = 0U; i < nCycles; i++)
    {
        nColourPicked = HSBtoRGB((float(i%64U)/64U)*360.0);
        nColour = matrix.color444((nColourPicked >> 8U) & 0xFU, (nColourPicked >> 4U) & 0xFU, (nColourPicked) & 0xFU);
        matrix.drawBitmap(0U, 0U, bitmap, 64U, 32U, nColour);
        delay(15);
    }

    /* Relinquish exclusive access to the matrix */
    xSemaphoreGive(oLEDMatrixMutex);
}

long HSBtoRGB(float _hue) {
    float red = 0.0;
    float green = 0.0;
    float blue = 0.0;

    int slice = _hue / 60.0;
    float hue_frac = (_hue / 60.0) - slice;

    float aa = (1.0);
    float bb = (1.0 - 1 * hue_frac);
    float cc = (1.0 - 1 * (1.0 - hue_frac));

    switch(slice) {
        case 0:
            red = 1;
            green = cc;
            blue = aa;
            break;
        case 1:
            red = bb;
            green = 1;
            blue = aa;
            break;
        case 2:
            red = aa;
            green = 1;
            blue = cc;
            break;
        case 3:
            red = aa;
            green = bb;
            blue = 1;
            break;
        case 4:
            red = cc;
            green = aa;
            blue = 1;
            break;
        case 5:
            red = 1;
            green = aa;
            blue = bb;
            break;
        default:
            red = 0.0;
            green = 0.0;
            blue = 0.0;
            break;
    }

    long  ired = red * 16.0;
    long  igreen = green * 16.0;
    long  iblue = blue * 16.0;

    return long((ired << 8) | (igreen << 4) | (iblue));
}

void setDateAndTime()
{
    static String sPreviousDay="", sPreviousMonth="", sPreviousHour="", sPreviousMin="", sPreviousDoW="";
    String sNewDay, sNewMonth, sNewHour, sNewMin, sNewDoW;

    sNewDay   = doc["day"].as<int>();
    sNewMonth = doc["month"].as<int>();
    sNewHour  = doc["hour"].as<int>();
    sNewMin   = doc["minute"].as<int>();
    sNewDoW   = doc["dayOfWeek"].as<char*>();

    /* Work's Done! */
    if ((sNewHour == "17") && (sNewMin == "30") && (sNewDoW[0] != 'S'))
    {
        /* Blank the screen & print the celebration */
        matrix.fillScreen(nBlack);
        printRainbowBitmap(youre_done_bitmap, 300U);
        blankAndDrawTime();
        /* Update stored variables */
        sPreviousDay = sNewDay;
        sPreviousMonth = sNewMonth;
        sPreviousHour = sNewHour;
        sPreviousMin = sNewMin;
    }
    /* Lunch time! */
    else if ((sNewHour == "13") && (sNewMin == "0"))
    {
        /* Blank the screen & print the celebration */
        matrix.fillScreen(nBlack);
        printRainbowBitmap(lunch_time_bitmap, 300U);
        blankAndDrawTime();
        /* Update stored variables */
        sPreviousDay = sNewDay;
        sPreviousMonth = sNewMonth;
        sPreviousHour = sNewHour;
        sPreviousMin = sNewMin;
    }
    else {
        /* Regular update of date & time */
        if (compareStrings(sPreviousDay, sNewDay) != 0U) {
            /* Blank & set the day zone */
            printToScreen(LengthenStrings(sNewDay), nYellow, 2U, ROW_0, 0U);
            /* Update the saved value */
            sPreviousDay = sNewDay;
        }
        if (compareStrings(sPreviousMonth, sNewMonth) != 0U) {
            /* Blank & set the month zone */
            printToScreen(LengthenStrings(sNewMonth), nYellow, 2U, ROW_0, TEXT_WIDTH * 3U - 4U);
            /* Update the saved value */
            sPreviousMonth = sNewMonth;
        }

        if (compareStrings(sPreviousHour, sNewHour) != 0U) {
            /* Blank & set the hour zone */
            printToScreen(LengthenStrings(sNewHour), nCyan, 2U, ROW_0, 64U - TEXT_WIDTH * 5U + 4);
            /* Update the saved value */
            sPreviousHour = sNewHour;
        }
        if (compareStrings(sPreviousMin, sNewMin) != 0U) {
            /* Blank & set the minute zone */
            printToScreen(LengthenStrings(sNewMin), nCyan, 2U, ROW_0, 64U - TEXT_WIDTH * 2U);
            /* Update the saved value */
            sPreviousMin = sNewMin;
        }
    }
}

void blankAndDrawTime()
{
    /* Blank the screen & reprint the current date & time */
    matrix.fillScreen(nBlack);
    drawDateAndTimeChars();
    printToScreen(LengthenStrings(String(doc["day"].as<int>())), nYellow, 2U, ROW_0, 0U);
    printToScreen(LengthenStrings(String(doc["month"].as<int>())), nYellow, 2U, ROW_0, TEXT_WIDTH*3U-4U);
    printToScreen(LengthenStrings(String(doc["hour"].as<int>())), nCyan, 2U, ROW_0, 64U-TEXT_WIDTH*5U+4);
    printToScreen(LengthenStrings(String(doc["minute"].as<int>())), nCyan, 2U, ROW_0, 64U-TEXT_WIDTH*2U);
}

uint8_t compareStrings(String Str1, String Str2)
{
    /* very simple string comparison funciton */
    const uint8_t knMinSize = (Str1.length() > Str1.length()) ? (Str2.length()) : (Str1.length());
    if (knMinSize == 0)
    {
        return -1;
    }
    uint8_t i = 0;
    while((Str1[i] == Str2[i]) && (i < knMinSize))
    {
        i++;
    }
    return Str1[i] - Str2[i];
}

String LengthenStrings(String Str1)
{
    /* Used to add a leading zero to single character time measurements */
    if (Str1.length() == 1)
    {
        return "0" + Str1;
    }
    return Str1;
}

void cycleMessage(const String ksMessage, const uint8_t knRow, const uint32_t knTextDelay)
{
    /* The length of the message in pixels/columns */
    const int knPixelLength   = ((int)ksMessage.length()*TEXT_WIDTH);
    /* Start at the last character position (just off screen) */
    const int knStartPosition = ((int)(MAX_CHAR_WIDTH*TEXT_WIDTH));

    for (int nPos = knStartPosition; nPos >= -knPixelLength; nPos--)
    {
        /* Print the message */
        printToScreen(ksMessage, nPurple, MAX_CHAR_WIDTH, knRow*TEXT_HEIGHT, nPos, 0U);
        /* How long to wait between printing each column */
        delay(knTextDelay);
    }
}

void GetAPIRequestJSON(const String ksURL)
{
    //Initiate HTTP client
    HTTPClient http;
    //Start the request
    http.begin(ksURL);
    //Use HTTP GET request
    http.GET();
    //Response from server
    response = http.getString();
    //Parse JSON, read error if any
    DeserializationError error = deserializeJson(doc, response);
    if(error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }
    //Close connection
    http.end();
}

void createPizza(uint8_t nXMid, uint8_t nYMid)
{
    /* Pizza "Colours" */
    uint16_t nCrust  = matrix.color444(1, 1, 0);
    uint16_t nCheese = matrix.color444(15, 14, 1);
    uint16_t nPepp   = matrix.color444(11, 1, 0);
    /* Pizza Code */
    matrix.fillCircle(nXMid,    nYMid,    7U, nCheese);
    matrix.drawCircle(nXMid,    nYMid,    8U, nCrust);
    matrix.fillCircle(nXMid-2U, nYMid-7U, 1U, nPepp);
    matrix.fillCircle(nXMid+3U, nYMid+3U, 1U, nPepp);
    matrix.fillCircle(nXMid-1U, nYMid+2U, 1U, nPepp);
    matrix.fillCircle(nXMid+4U, nYMid-4U, 1U, nPepp);
    matrix.fillCircle(nXMid+1U, nYMid-3U, 1U, nPepp);
    matrix.fillCircle(nXMid-6U, nYMid,    1U, nPepp);
    matrix.fillCircle(nXMid-4U, nYMid-3U, 1U, nPepp);
    matrix.fillCircle(nXMid,    nYMid+7U, 1U, nPepp);
    matrix.drawPixel(nXMid-3U,  nYMid+4U, nGreen);
    matrix.drawPixel(nXMid-1U,  nYMid-4U, nGreen);
    matrix.drawPixel(nXMid+4U,  nYMid-2U, nGreen);
    matrix.drawPixel(nXMid+1U,  nYMid+1U, nGreen);
}

void removeCircularSegment(uint8_t nTopLeftCol, uint8_t nTopLeftRow, uint8_t nWidth, uint8_t nHeight, double nFraction)
{
    double pi = 3.14159;
    uint8_t xPos, yPos, xOrigin, yOrigin;

    /* Centre of the circle */
    xOrigin = (nWidth/2) + nTopLeftCol;
    yOrigin = (nHeight/2) + nTopLeftRow;

    /* Start from the top centre */
    xPos = xOrigin;
    yPos = nTopLeftRow;

    rotateThroughTheta(&xPos, &yPos, xOrigin, yOrigin, (2*pi*nFraction));
    matrix.drawLine(xOrigin, yOrigin, xPos, yPos, nBlack);
}

void rotateThroughTheta(uint8_t *x, uint8_t *y, uint8_t ox, uint8_t oy, double theta)
{
    float px, py;
    /* Rotation applied to x & y through an angle of theta, around origin points ox & oy */
    px = cos(theta) * (*x-ox) - sin(theta) * (*y-oy) + ox;
    py = sin(theta) * (*x-ox) + cos(theta) * (*y-oy) + oy;

    /* Round results to nearest integer */
    *x = (int)(px < 0) ? (px - 0.5) : (px + 0.5);
    *y = (int)(py < 0) ? (py - 0.5) : (py + 0.5);
}