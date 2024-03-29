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
#include "graphic_bitmasks.h"

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
/* One Milli-minute (60 seconds in milliseconds) */
#define MILLI_MINUTE    60000U
/* One Milli-hour (60 minutes in milliminutes) */
#define MILLI_HOUR      3600000U
/* 07:00 in milliseconds */
#define WAKE_TIME       25200000U
/* 23:00 in milliseconds */
#define SLEEP_TIME      82800000U
/* 100 millisecond padding for API calls & Tickers */
#define TIME_PADDING    100U


/*=== P R O T O T Y P E S ===*/

void core0Loop(void *unused);
void core1Loop(void *unused);
void causeTime();
void causeNightTime();
void drawDateAndTimeChars();
void drawHourglass(const uint8_t knLeftX, const uint8_t knTopY, const uint8_t knWidth, const uint8_t knHeight);
void fillHourglass(const uint8_t knFillState);
void printToScreen(const String ksMessage, const uint16_t knColour, const uint8_t knNumChars, const uint8_t knRow, const int knCursorCol, const uint8_t knClearCol);
void inline printToScreen(const String ksMessage, const uint16_t knColour, const uint8_t knNumChars, const uint8_t knRow, const int knCol);
void printRainbowBitmap(const unsigned char bitmap[], const uint16_t nCycles);
long HSBtoRGB(float _hue);
void setDateAndTime();
void blankAndDrawTime();
uint8_t compareStrings(String Str1, String Str2);
String lengthenStrings(String Str1);
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
        "  Touch",  "  Green",  7U,  2
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
uint16_t nGrey = matrix.color444(4U, 4U, 4U);
uint16_t nBrown = matrix.color444(2U, 2U, 0);
uint16_t nTODO   = matrix.color444(0, 5, 15);

/* Setting up the Clock ticker & nighttime ticker */
Ticker oTimeTicker(causeTime, MILLI_SECOND);
Ticker oNightTicker(causeNightTime, 10*MILLI_MINUTE, 1, MILLIS);

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

    matrix.
    /* Draw a pizza (with 6 slices) in the middle-left area of the LED matrix */
//    createPizza(8U, 16U);
//    double nFraction = 1.0 / 6.0, nCircuit = 360.0;
//    for (double i = 0.0; i <= nCircuit; i = i + nFraction) {
//        removeCircularSegment(0U, 8U, 16U, 16U, i);
//    }
//    matrix.fillRect(0U, 8U, 17U, 16U, nBlack);

//    drawHourglass(0U,9U,17U, 15U);
//    fillHourglass(0U);

    /* Draw the essential characters onto the panel */
    drawDateAndTimeChars();

    /* Start the periodic Core 0 Time-tracking Ticker */
    oTimeTicker.start();
    /* Get the time for the nighttime timer from timeapi.io */
    GetAPIRequestJSON(ksTimeRequest);
    /* Start the Core 0 nighttime Ticker */
    oNightTicker.start();
    /* Schedule the next ticker call to at sleep time or now, if it's already nighttime */
    int32_t nTimeTilNight = SLEEP_TIME - ((MILLI_HOUR * doc["hour"].as<int>()) + (MILLI_MINUTE * doc["minute"].as<int>()));
    oNightTicker.interval((nTimeTilNight > 0) ? (nTimeTilNight) : (TIME_PADDING));

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
{/* Empty Loop as each core has an independent loop */}

/**
 * Core 0 main loop - Used to show time & tasks
 * @param unused
 */
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
        delay(1);

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

/**
 * Core 1 main loop - Used for API requests & Carousel
 * @param unused
 */
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

/**
 * Requests the current time & date, shows it on display and schedules next update
 */
void causeTime()
{
    /* Get the time and date from timeapi.io */
    GetAPIRequestJSON(ksTimeRequest);
    /* Schedule the next update for just after the next minute */
    oTimeTicker.interval(MILLI_MINUTE - (MILLI_SECOND * doc["seconds"].as<int>()) + TIME_PADDING);
    /* Set the time and date on the display */
    setDateAndTime();
}

/**
 * Power saving nighttime sequence; blanks screen, deep-sleeps ESP32 & sets wakeup time
 */
void causeNightTime()
{
    /* Nighty-Night */
    int32_t nTimeTilWake;
    uint32_t nTimeTilNextCycle;

    /* Blank the screen & print the nighttime message */
    xSemaphoreTake(oLEDMatrixMutex, portMAX_DELAY);
    matrix.fillScreen(nBlack);
    String nNightTimeMessageRow1 = "Night";
    String nNightTimeMessageRow2 = "  :)";
    matrix.setTextSize(2);
    xSemaphoreGive(oLEDMatrixMutex);
    /* Offset messages to give 3D effect */
    printToScreen(nNightTimeMessageRow1, nRed, 0U, ROW_0*TEXT_HEIGHT+1U, 2U);
    printToScreen(nNightTimeMessageRow2, nRed, 0U, ROW_2*TEXT_HEIGHT+1U, 2U);
    printToScreen(nNightTimeMessageRow1, nPurple, 0U, ROW_0*TEXT_HEIGHT, 3U);
    printToScreen(nNightTimeMessageRow2, nPurple, 0U, ROW_2*TEXT_HEIGHT, 3U);
    xSemaphoreTake(oLEDMatrixMutex, portMAX_DELAY);
    delay(5000U);
    xSemaphoreGive(oLEDMatrixMutex);

    /* Get the current time in milliseconds */
    nTimeTilWake = ((MILLI_HOUR * doc["hour"].as<int>()) + (MILLI_MINUTE * doc["minute"].as<int>()));
    /* Get the offset between now & wakeup time : assert if it's the day before or the same day as wakeup time */
    nTimeTilWake = (nTimeTilWake > WAKE_TIME) ? (nTimeTilWake - (24U * MILLI_HOUR)) : (nTimeTilWake);
    /* Schedule the ESP wakeup at WAKE_TIME */
    nTimeTilNextCycle = WAKE_TIME - nTimeTilWake;
    /* Time in microseconds */
    esp_sleep_enable_timer_wakeup(nTimeTilNextCycle*1000U);
    esp_deep_sleep_start();
}

/**
 * Draws accessory datetime characters on matrix
 */
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

/**
 * Prints message to the matrix display
 * @param ksMessage   Message to display
 * @param knColour    Font colour
 * @param knNumChars  Message length
 * @param knRow       Row to print on
 * @param knCursorCol Where in column to print
 * @param knClearCol  Column to clear from.
 */
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

/**
 * (Overload) Prints message to the matrix display
 * @param ksMessage   Message to display
 * @param knColour    Font colour
 * @param knNumChars  Message length
 * @param knRow       Row to print on
 * @param knCursorCol Where in column to print, which is also cleared
 */
void inline printToScreen(const String ksMessage, const uint16_t knColour, const uint8_t knNumChars, const uint8_t knRow, const int knCol)
{
    /* Overloaded function with one column value */
    printToScreen(ksMessage, knColour, knNumChars, knRow, knCol, knCol);
}

/**
 * Prints bitmap in a rainbow fashion
 * @param bitmap  Bitmap to print
 * @param nCycles How many times the display should rainbow cycle
 */
void printRainbowBitmap(const unsigned char bitmap[], const uint16_t nCycles)
{
    /* Gain exclusive access to the matrix */
    xSemaphoreTake(oLEDMatrixMutex, portMAX_DELAY);

    uint16_t nColour;
    long nColourPicked;

    for (uint16_t i = 0U; i < nCycles; i++)
    {
        /* Draw the bitmap while shifting the hue */
        nColourPicked = HSBtoRGB((float(i%64U)/64U)*360.0);
        nColour = matrix.color444((nColourPicked >> 8U) & 0xFU, (nColourPicked >> 4U) & 0xFU, (nColourPicked) & 0xFU);
        matrix.drawBitmap(0U, 0U, bitmap, 64U, 32U, nColour);
        delay(15);
    }

    /* Relinquish exclusive access to the matrix */
    xSemaphoreGive(oLEDMatrixMutex);
}

/**
 * Converts Hue values to Red Green & Blue colour space
 * @param _hue Hue
 * @return     4-bit RGB values
 */
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

/**
 * Sets data and time based on prior API request.
 * Also Displays messages at lunch/ quittin' time(s)
 */
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
        printRainbowBitmap(youre_done_bitmap, 500U);
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
        printRainbowBitmap(lunch_time_bitmap, 500U);
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
            printToScreen(lengthenStrings(sNewDay), nYellow, 2U, ROW_0, 0U);
            /* Update the saved value */
            sPreviousDay = sNewDay;
        }
        if (compareStrings(sPreviousMonth, sNewMonth) != 0U) {
            /* Blank & set the month zone */
            printToScreen(lengthenStrings(sNewMonth), nYellow, 2U, ROW_0, TEXT_WIDTH * 3U - 4U);
            /* Update the saved value */
            sPreviousMonth = sNewMonth;
        }

        if (compareStrings(sPreviousHour, sNewHour) != 0U) {
            /* Blank & set the hour zone */
            printToScreen(lengthenStrings(sNewHour), nCyan, 2U, ROW_0, 64U - TEXT_WIDTH * 5U + 4);
            /* Update the saved value */
            sPreviousHour = sNewHour;
        }
        if (compareStrings(sPreviousMin, sNewMin) != 0U) {
            /* Blank & set the minute zone */
            printToScreen(lengthenStrings(sNewMin), nCyan, 2U, ROW_0, 64U - TEXT_WIDTH * 2U);
            /* Update the saved value */
            sPreviousMin = sNewMin;
        }
    }
}

/**
 * Clears screen and redraws time & date
 */
void blankAndDrawTime()
{
    /* Blank the screen & reprint the current date & time */
    matrix.fillScreen(nBlack);
    drawDateAndTimeChars();
    printToScreen(lengthenStrings(String(doc["day"].as<int>())), nYellow, 2U, ROW_0, 0U);
    printToScreen(lengthenStrings(String(doc["month"].as<int>())), nYellow, 2U, ROW_0, TEXT_WIDTH * 3U - 4U);
    printToScreen(lengthenStrings(String(doc["hour"].as<int>())), nCyan, 2U, ROW_0, 64U - TEXT_WIDTH * 5U + 4);
    printToScreen(lengthenStrings(String(doc["minute"].as<int>())), nCyan, 2U, ROW_0, 64U - TEXT_WIDTH * 2U);
}

/**
 * Compares 2 strings
 * @param  Str1 String 1
 * @param  Str2 String 2
 * @return  0   : Strings equal
 * @return <0|>0: Strings Unequal
 */
uint8_t compareStrings(String Str1, String Str2)
{
    /* very simple string comparison function */
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

/**
 * Adds leading zeros to single characters
 * @param Str1 Unpadded String
 * @return     Padded String
 */
String lengthenStrings(String Str1)
{
    /* Used to add a leading zero to single character time measurements */
    if (Str1.length() == 1)
    {
        return "0" + Str1;
    }
    return Str1;
}

/**
 * Creates a scrolling carousel displaying a given message/
 * @param ksMessage   What message should be displayed
 * @param knRow       What row to create the carousel on
 * @param knTextDelay Delay between printing each letter (inversely proportional to carousel velocity)
 */
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

/**
 * General get request to JSON
 * @param ksURL target URL
 */
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

/**
 * Draws hourglass on matrix
 * @param knLeftX  Leftmost x pos
 * @param knTopY   Topmost y pos
 * @param knWidth  Hourglass width
 * @param knHeight Hourglass height
 */
void drawHourglass(const uint8_t knLeftX, const uint8_t knTopY, const uint8_t knWidth, const uint8_t knHeight)
{
    /* Gain exclusive access to the matrix */
    xSemaphoreTake(oLEDMatrixMutex, portMAX_DELAY);
    /* Draw the hourglass border */
    matrix.drawFastHLine(knLeftX, knTopY, knWidth, nBrown);
    matrix.drawFastHLine(knLeftX, knTopY+knHeight-1U, knWidth, nBrown);
    /* Draw the glass in fast horizontal lines rather than slower triangles */
    for (uint8_t i = 0; i < ((knHeight-1)/2)-1; i++)
    {
        matrix.drawFastHLine(knLeftX+1U+i, knTopY+1U+i, knWidth-2U*(i+1U), nGrey);
        matrix.drawFastHLine(knLeftX+1U+i, knTopY+knHeight-i-2U, knWidth-2U*(i+1U), nGrey);
    }
    /* Draw centre horizontal line */
    matrix.drawFastHLine(knLeftX+7U, knTopY+7U, 3U, nGrey);
    /* Relinquish exclusive access to the matrix */
    xSemaphoreGive(oLEDMatrixMutex);
}

/**
 * Fills hourglass to a set level.
 * @param knFillState state of hourglass
 */
void fillHourglass(const uint8_t knFillState)
{
    /* Gain exclusive access to the matrix */
    xSemaphoreTake(oLEDMatrixMutex, portMAX_DELAY);

    uint16_t nSand1 = matrix.color444(15U, 8U, 0);
    uint16_t nSand2 = matrix.color444(5U, 1U, 0);

    /* Draw the top glass full */
    matrix.fillTriangle(8U, 16U, 5U, 13U, 11U, 13U, nSand1);
    matrix.drawFastHLine(6U, 12U, 7U, nSand1);
    matrix.drawFastHLine(7U, 11U, 5U, nSand1);
    matrix.drawPixel(9U, 12U, nSand2);
    matrix.drawPixel(10U, 12U, nSand2);
    matrix.drawPixel(7U, 15U, nSand2);
    matrix.drawPixel(9U, 14U, nSand2);

    /* Draw the bottom glass full */
    matrix.fillTriangle(8U, 18U, 12U, 22U, 4U, 22U, nSand1);
    matrix.drawPixel(8U, 17U, nSand2);
    matrix.drawPixel(7U, 19U, nSand2);
    matrix.drawPixel(8U, 20U, nSand2);
    matrix.drawPixel(10U, 22U, nSand2);

    switch (knFillState) {
        case 0U:
        default:
            break;
    }
    /* Relinquish exclusive access to the matrix */
    xSemaphoreGive(oLEDMatrixMutex);
}

/**
 * Creates Pizza graphic
 * @param nXMid Pizza circle x origin
 * @param nYMid Pizza circle y origin
 */
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

/**
 * Complex function to cut pizza
 * @param nTopLeftCol Leftmost column of pizza
 * @param nTopLeftRow Uppermost row of pizza
 * @param nWidth      Pizza Width
 * @param nHeight     Pizza Height
 * @param nFraction   What angle the pizza should be cut at (radians)
 */
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

/**
 * Helper function to calculate coordinates of a point on a circle (pizza)
 * @param x     TDC x
 * @param y     TDC y
 * @param ox    Origin x
 * @param oy    Origin y
 * @param theta Angle of rotation
 */
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