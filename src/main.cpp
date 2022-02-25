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
#define CAROUSEL_DELAY  20U
/* One Milli-minute (60 seconds in milliseconds)*/
#define MILLI_MINUTE    60000U
/* Converter for milliseconds */
#define MILLI           1000U
/* 100 millisecond padding for API calls */
#define API_PADDING     100U


/*=== P R O T O T Y P E S ===*/

void core0Loop(void *unused);
void core1Loop(void *unused);
void causeTime();
void printToScreen(const String ksMessage, const uint16_t knColour, const uint8_t knNumChars, const uint8_t knRow, const int knCursorCol, const uint8_t knClearCol);
void inline printToScreen(const String ksMessage, const uint16_t knColour, const uint8_t knNumChars, const uint8_t knRow, const int knCol);
void setDateAndTime();
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

/*=== D A T A ===*/

/* Create two task objects */
TaskHandle_t Task1, Task2;
/* Declare a Mutex */
SemaphoreHandle_t oMatrixMutex;

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

/* Setting up the Clock timer */
Ticker oTimeTicker(causeTime, 1000U);

/*=== F U N C T I O N S ===*/

void setup() {

    Serial.begin(115200);

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

    /* Begin & blank LED matrix */
    matrix.begin();
    matrix.fillScreen(nBlack);

    /* Draw a pizza (with 6 slices) in the middle-left area of the LED matrix */
//    double nFraction = 1.0 / 6.0, nCircuit = 360.0;
//    createPizza(8U, 16U);
//    for (double i = 0.0; i <= nCircuit; i = i + nFraction) {
//        removeCircularSegment(0U, 8U, 16U, 16U, i);
//    }


    /* Text configuration */
    matrix.setTextSize(1);     // size 1 == 8 pixels high
    matrix.setTextWrap(false); // Don't wrap at end of line - will do ourselves

    /* Draw date '|' character */
    matrix.drawLine(TEXT_WIDTH*2U, 0U, TEXT_WIDTH*2U, TEXT_HEIGHT-1U, nYellow);
    /* Draw time ':' character */
    matrix.setTextColor(nCyan);
    matrix.setCursor(64 - TEXT_WIDTH*3U+2, ROW_0);
    matrix.print(":");

    /* Create the Mutex */
    oMatrixMutex = xSemaphoreCreateMutex();

    /*Syntax for assigning task to a core:
   xTaskCreatePinnedToCore(
                    coreTask,   // Function to implement the task
                    "coreTask", // Name of the task
                    10000,      // Stack size in words
                    NULL,       // Task input parameter
                    0,          // Priority of the task
                    NULL,       // Task handle.
                    taskCore);  // Core where the task should run
   */
    xTaskCreatePinnedToCore(core0Loop, "TimeTask", 5000, NULL, 2, &Task1, CORE_0);
    oTimeTicker.start();
    xTaskCreatePinnedToCore(core1Loop, "AffirmTask", 5000, NULL, 2, &Task2, CORE_1);
}

void loop()
{}

void core0Loop(void *unused)
{
    /* Core 0 loop */
    for(;;)
    {
        oTimeTicker.update();
        delay(1);
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
    oTimeTicker.interval(MILLI_MINUTE - (MILLI * doc["seconds"].as<int>()) + API_PADDING);
    /* Set the time and date on the display */
    setDateAndTime();
}

void printToScreen(const String ksMessage, const uint16_t knColour, const uint8_t knNumChars, const uint8_t knRow, const int knCursorCol, const uint8_t knClearCol)
{
    /* Gain exclusive access to the matrix */
    xSemaphoreTake(oMatrixMutex, portMAX_DELAY);
    /* Set text colour & blank the amount of chars input */
    matrix.setTextColor(knColour);
    matrix.fillRect(knClearCol, knRow, TEXT_WIDTH*knNumChars, TEXT_HEIGHT, nBlack);
    /* Position the cursor at the input position & print message */
    matrix.setCursor(knCursorCol, knRow);
    matrix.print(LengthenStrings(ksMessage));
    /* Relinquish exclusive access to the matrix */
    xSemaphoreGive(oMatrixMutex);
}

void inline printToScreen(const String ksMessage, const uint16_t knColour, const uint8_t knNumChars, const uint8_t knRow, const int knCol)
{
    /* Overloaded function with one column value */
    printToScreen(ksMessage, knColour, knNumChars, knRow, knCol, knCol);
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

    if (compareStrings(sPreviousDay, sNewDay) != 0U)
    {
        /* Blank & set the day zone */
        printToScreen(sNewDay, nYellow, 2U, ROW_0, 0U);
        /* Update the saved value */
        sPreviousDay = sNewDay;
    }
    if (compareStrings(sPreviousMonth, sNewMonth) != 0U)
    {
        /* Blank & set the month zone */
        printToScreen(sNewMonth, nYellow, 2U, ROW_0, TEXT_WIDTH*3U-4U);
        /* Update the saved value */
        sPreviousMonth = sNewMonth;
    }

    if (compareStrings(sPreviousHour, sNewHour) != 0U)
    {
        /* Blank & set the hour zone */
        printToScreen(sNewHour, nCyan, 2U, ROW_0, 64U-TEXT_WIDTH*5U+4);
        /* Update the saved value */
        sPreviousHour = sNewHour;
    }
    if (compareStrings(sPreviousMin, sNewMin) != 0U)
    {
        /* Blank & set the minute zone */
        printToScreen(sNewMin, nCyan, 2U, ROW_0, 64U-TEXT_WIDTH*2U);
        /* Update the saved value */
        sPreviousMin = sNewMin;
    }
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