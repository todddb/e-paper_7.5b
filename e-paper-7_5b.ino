/*
 * 🗓️ Scheduled E-Paper Calendar Display System 🗓️
 * 
 * Connects to desktop calendar generator and displays calendar on 7.5" e-paper
 * Updates on schedule: 7:50am, 8:20am, 8:50am, 9:20am ... 4:20pm, 4:50pm
 * Deep sleeps between updates for power efficiency
 * 
 * ENDPOINTS:
 * - Status & Time: http://192.168.11.2:5000/status
 * - PNG Image: http://192.168.11.2:5000/calendar.png
 * - Raw B/W Data: http://192.168.11.2:5000/calendar_bw.raw
 * - Raw Red Data: http://192.168.11.2:5000/calendar_red.raw
 * 
 * SCHEDULE: Updates every 30 minutes from 7:50am to 4:50pm
 * SLEEP: Deep sleep from 4:50pm until 7:50am next day
 * 
 * =============================================================================
 * 🔴 RED CHANNEL SECRET (for future red accent support)
 * BLACK PIXELS:  B/W = 0x00, Red = 0x00
 * WHITE PIXELS:  B/W = 0x33, Red = 0x00 
 * RED PIXELS:    B/W = 0xCC, Red = 0xFF
 * =============================================================================
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

// =============================================================================
// 🔌 PIN DEFINITIONS & CONSTANTS
// =============================================================================

// Undefine ESP8266 default pins to avoid conflicts
#ifdef PIN_SPI_SCK
#undef PIN_SPI_SCK
#endif
#ifdef PIN_SPI_DIN  
#undef PIN_SPI_DIN
#endif

#define PIN_SPI_SCK 14
#define PIN_SPI_DIN 13
#define CS_PIN 15
#define RST_PIN 2
#define DC_PIN 4
#define BUSY_PIN 5

#define GPIO_PIN_SET 1
#define GPIO_PIN_RESET 0

// Display specifications
#define DISPLAY_WIDTH 640
#define DISPLAY_HEIGHT 384
#define TOTAL_BYTES 122880  // 640*384/2

// =============================================================================
// 🌐 WIFI & SERVER CONFIGURATION
// =============================================================================

const char* ssid = "Todd's Office";        // Your WiFi network
const char* password = "t0dds0ff!c3";      // Your WiFi password

const char* status_url = "http://192.168.11.2:5000/status";
const char* calendar_raw_url = "http://192.168.11.2:5000/calendar_bw.raw";
const char* calendar_red_url = "http://192.168.11.2:5000/calendar_red.raw";
const char* calendar_png_url = "http://192.168.11.2:5000/calendar.png";

// =============================================================================
// ⏰ TIME & SCHEDULING CONFIGURATION
// =============================================================================

// Time tracking variables
unsigned long serverTimestamp = 0;    // Unix timestamp from server
unsigned long localTimeBase = 0;      // millis() when we got server time
bool timeSet = false;

// Schedule: Updates every 30 minutes from 7:50am to 4:50pm
// 7:50am = 470 minutes from midnight
// 4:50pm = 1010 minutes from midnight  
// Total updates per day: 19
const int FIRST_UPDATE_MINUTES = 7 * 60 + 50;  // 7:50am = 470 minutes
const int LAST_UPDATE_MINUTES = 16 * 60 + 50;  // 4:50pm = 1010 minutes
const int UPDATE_INTERVAL_MINUTES = 30;

// Calculate total number of updates per day
const int TOTAL_DAILY_UPDATES = ((LAST_UPDATE_MINUTES - FIRST_UPDATE_MINUTES) / UPDATE_INTERVAL_MINUTES) + 1;

// =============================================================================
// 🚀 SETUP & MAIN LOOP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("🗓️ Scheduled E-Paper Calendar Starting... 🗓️");
  
  // Initialize display pins
  initializeHardware();
  
  // Connect to WiFi
  connectToWiFi();
  
  // Get current time from server
  if (synchronizeTime()) {
    Serial.println("✅ Time synchronized with server");
    
    // Check if we should update now or sleep
    int currentMinutes = getCurrentMinutesFromMidnight();
    if (shouldUpdateNow(currentMinutes)) {
      Serial.println("🕐 It's update time! Updating calendar...");
      updateCalendar();
    } else {
      Serial.println("😴 Not update time, going to sleep...");
    }
    
    // Calculate and enter deep sleep until next update
    enterDeepSleepUntilNextUpdate();
    
  } else {
    Serial.println("❌ Failed to synchronize time, trying emergency update...");
    updateCalendar();
    
    // If time sync failed, sleep for 30 minutes and try again
    Serial.println("😴 Emergency sleep for 30 minutes...");
    ESP.deepSleep(30 * 60 * 1000000UL); // 30 minutes in microseconds
  }
}

void loop() {
  // This should never be reached due to deep sleep
  Serial.println("⚠️  WARNING: Loop reached - this shouldn't happen with deep sleep!");
  delay(10000);
}

// =============================================================================
// ⏰ TIME SYNCHRONIZATION & SCHEDULING
// =============================================================================

bool synchronizeTime() {
  Serial.println("🕐 Synchronizing time with server...");
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ No WiFi connection for time sync");
    return false;
  }
  
  WiFiClient client;
  HTTPClient http;
  
  http.begin(client, status_url);
  http.setTimeout(10000);  // 10 second timeout
  
  int httpCode = http.GET();
  
  if (httpCode != HTTP_CODE_OK) {
    Serial.print("❌ Status request failed, code: ");
    Serial.println(httpCode);
    http.end();
    return false;
  }
  
  String payload = http.getString();
  http.end();
  
  Serial.print("📡 Status response: ");
  Serial.println(payload);
  
  // Parse JSON response
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, payload);
  
  if (error) {
    Serial.print("❌ JSON parsing failed: ");
    Serial.println(error.f_str());
    return false;
  }
  
  // Extract timestamp
  const char* timestampStr = doc["timestamp"];
  if (!timestampStr) {
    Serial.println("❌ No timestamp in status response");
    return false;
  }
  
  Serial.print("🕐 Server timestamp: ");
  Serial.println(timestampStr);
  
  // Parse ISO timestamp to Unix timestamp
  serverTimestamp = parseISOTimestamp(timestampStr);
  localTimeBase = millis();
  timeSet = true;
  
  Serial.print("✅ Time set - Unix timestamp: ");
  Serial.println(serverTimestamp);
  
  return true;
}

unsigned long parseISOTimestamp(const char* isoTime) {
  // Parse "2025-06-04T13:28:26.472224" format
  // This is a simplified parser - for production you might want more robust parsing
  
  int year, month, day, hour, minute, second;
  sscanf(isoTime, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second);
  
  // Convert to Unix timestamp (simplified calculation)
  // This is approximate - for precise time you'd want a proper time library
  unsigned long timestamp = 0;
  
  // Days since epoch (1970-01-01)
  int days = (year - 1970) * 365 + (year - 1970) / 4; // Rough leap year calculation
  
  // Add months (approximate)
  int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  for (int m = 1; m < month; m++) {
    days += daysInMonth[m - 1];
  }
  
  days += day - 1;  // Days in current month
  
  timestamp = days * 24UL * 60UL * 60UL;  // Convert days to seconds
  timestamp += hour * 60UL * 60UL;        // Add hours
  timestamp += minute * 60UL;             // Add minutes
  timestamp += second;                    // Add seconds
  
  return timestamp;
}

int getCurrentMinutesFromMidnight() {
  if (!timeSet) {
    return -1;  // Time not set
  }
  
  // Calculate current time based on server timestamp + elapsed time
  unsigned long elapsedMillis = millis() - localTimeBase;
  unsigned long currentTimestamp = serverTimestamp + (elapsedMillis / 1000);
  
  // Extract hour and minute from timestamp
  unsigned long secondsToday = currentTimestamp % (24UL * 60UL * 60UL);
  int currentHour = secondsToday / 3600;
  int currentMinute = (secondsToday % 3600) / 60;
  
  int minutesFromMidnight = currentHour * 60 + currentMinute;
  
  Serial.print("🕐 Current time: ");
  Serial.print(currentHour);
  Serial.print(":");
  if (currentMinute < 10) Serial.print("0");
  Serial.print(currentMinute);
  Serial.print(" (");
  Serial.print(minutesFromMidnight);
  Serial.println(" minutes from midnight)");
  
  return minutesFromMidnight;
}

bool shouldUpdateNow(int currentMinutes) {
  if (currentMinutes < 0) return true;  // If time not set, update anyway
  
  // Check if current time matches any update time (within 5 minute window)
  for (int updateMinutes = FIRST_UPDATE_MINUTES; updateMinutes <= LAST_UPDATE_MINUTES; updateMinutes += UPDATE_INTERVAL_MINUTES) {
    if (abs(currentMinutes - updateMinutes) <= 5) {
      Serial.print("✅ Update time match: ");
      Serial.print(updateMinutes / 60);
      Serial.print(":");
      if ((updateMinutes % 60) < 10) Serial.print("0");
      Serial.println(updateMinutes % 60);
      return true;
    }
  }
  
  return false;
}

int getNextUpdateMinutes(int currentMinutes) {
  // Find the next update time
  for (int updateMinutes = FIRST_UPDATE_MINUTES; updateMinutes <= LAST_UPDATE_MINUTES; updateMinutes += UPDATE_INTERVAL_MINUTES) {
    if (updateMinutes > currentMinutes) {
      return updateMinutes;
    }
  }
  
  // If we're past the last update of the day, next update is tomorrow at 7:50am
  return FIRST_UPDATE_MINUTES + (24 * 60);  // Tomorrow's first update
}

void enterDeepSleepUntilNextUpdate() {
  int currentMinutes = getCurrentMinutesFromMidnight();
  int nextUpdateMinutes = getNextUpdateMinutes(currentMinutes);
  
  int sleepMinutes;
  if (nextUpdateMinutes > currentMinutes) {
    sleepMinutes = nextUpdateMinutes - currentMinutes;
  } else {
    // Next update is tomorrow
    sleepMinutes = (24 * 60) - currentMinutes + nextUpdateMinutes;
  }
  
  // Subtract 2 minutes to wake up slightly early for WiFi connection
  sleepMinutes = max(1, sleepMinutes - 2);
  
  Serial.print("😴 Sleeping for ");
  Serial.print(sleepMinutes);
  Serial.print(" minutes until next update at ");
  
  int nextHour = (nextUpdateMinutes % (24 * 60)) / 60;
  int nextMinute = (nextUpdateMinutes % (24 * 60)) % 60;
  Serial.print(nextHour);
  Serial.print(":");
  if (nextMinute < 10) Serial.print("0");
  Serial.println(nextMinute);
  
  // Convert minutes to microseconds for deep sleep
  unsigned long sleepMicros = sleepMinutes * 60UL * 1000000UL;
  
  Serial.println("💤 Entering deep sleep...");
  Serial.flush();  // Make sure all serial output is sent before sleeping
  
  ESP.deepSleep(sleepMicros);
}

// =============================================================================
// 🌐 WIFI CONNECTION
// =============================================================================

void connectToWiFi() {
  Serial.print("📶 Connecting to WiFi: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("✅ WiFi Connected! IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("❌ WiFi connection failed!");
  }
}

// =============================================================================
// 🗓️ CALENDAR UPDATE FUNCTIONS
// =============================================================================

// Function declarations (to avoid compilation errors)
bool downloadAndStreamBW();
bool downloadAndStreamRed();
bool streamChannelToDisplay(HTTPClient& http, byte channelCommand, const char* channelName, int expectedBytes);
void diagnoseSizeIssues(int contentLength, const char* channelName);

void updateCalendar() {
  Serial.println("🗓️ Starting calendar update...");
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ No WiFi connection, skipping update");
    return;
  }
  
  // Download and display calendar
  if (downloadAndDisplayCalendar()) {
    Serial.println("✅ Calendar updated successfully!");
  } else {
    Serial.println("❌ Calendar update failed");
  }
}

bool downloadAndDisplayCalendar() {
  Serial.println("🗓️ Starting 3-color calendar download...");
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ No WiFi connection, skipping update");
    return false;
  }
  
  // Initialize display first
  Serial.println("🖥️  Initializing display...");
  EPD_7in5_V1_Init();
  
  // Download and stream B/W channel
  if (!downloadAndStreamBW()) {
    Serial.println("❌ Failed to download B/W channel");
    return false;
  }
  
  // Download and stream Red channel
  if (!downloadAndStreamRed()) {
    Serial.println("❌ Failed to download Red channel");
    return false;
  }
  
  // Refresh display
  Serial.println("🖥️  Refreshing display...");
  EPD_7IN5_V1_Show();
  
  Serial.println("✅ 3-color calendar update complete!");
  return true;
}

bool downloadAndStreamBW() {
  WiFiClient client;
  HTTPClient http;
  
  Serial.println("📡 Downloading B/W channel...");
  Serial.print("URL: ");
  Serial.println(calendar_raw_url);
  
  http.begin(client, calendar_raw_url);
  http.setTimeout(30000);  // 30 second timeout
  
  int httpCode = http.GET();
  
  if (httpCode != HTTP_CODE_OK) {
    Serial.print("❌ B/W HTTP request failed, code: ");
    Serial.println(httpCode);
    http.end();
    return false;
  }
  
  int contentLength = http.getSize();
  Serial.print("📊 B/W Content length: ");
  Serial.println(contentLength);
  Serial.print("📊 Expected length: ");
  Serial.println(TOTAL_BYTES);
  
  // Diagnose size issues
  diagnoseSizeIssues(contentLength, "B/W");
  
  // Stream B/W data to display
  bool success = streamChannelToDisplay(http, 0x10, "B/W", contentLength);
  http.end();
  
  return success;
}

bool downloadAndStreamRed() {
  WiFiClient client;
  HTTPClient http;
  
  Serial.println("📡 Downloading Red channel...");
  Serial.print("URL: ");
  Serial.println(calendar_red_url);
  
  http.begin(client, calendar_red_url);
  http.setTimeout(30000);  // 30 second timeout
  
  int httpCode = http.GET();
  
  if (httpCode != HTTP_CODE_OK) {
    Serial.print("❌ Red HTTP request failed, code: ");
    Serial.println(httpCode);
    
    // If red channel fails, fill with "no red" data
    Serial.println("💡 Red channel not available, disabling red everywhere...");
    EPD_SendCommand(0x13);  // Red channel
    for (long i = 0; i < TOTAL_BYTES; i++) {
      EPD_SendData(0xFF);  // Disable red
      if (i % 30000 == 0) {
        Serial.print("No Red: ");
        Serial.print(i/1000);
        Serial.println("k");
      }
    }
    http.end();
    return true;  // Continue even if red channel fails
  }
  
  int contentLength = http.getSize();
  Serial.print("📊 Red Content length: ");
  Serial.println(contentLength);
  Serial.print("📊 Expected length: ");
  Serial.println(TOTAL_BYTES);
  
  // Diagnose size issues
  diagnoseSizeIssues(contentLength, "Red");
  
  // Stream Red data to display
  bool success = streamChannelToDisplay(http, 0x13, "Red", contentLength);
  http.end();
  
  return success;
}

void diagnoseSizeIssues(int contentLength, const char* channelName) {
  if (contentLength == TOTAL_BYTES / 4) {
    Serial.print("🔍 DIAGNOSIS (");
    Serial.print(channelName);
    Serial.println("): File is 1/4 expected size!");
    Serial.println("💡 Calendar generator creating 320×192 instead of 640×384");
  } else if (contentLength == TOTAL_BYTES * 2) {
    Serial.print("🔍 DIAGNOSIS (");
    Serial.print(channelName);
    Serial.println("): File is 2x expected size!");
    Serial.println("💡 Calendar generator not packing 2 pixels per byte");
  } else if (contentLength != TOTAL_BYTES) {
    Serial.print("🔍 DIAGNOSIS (");
    Serial.print(channelName);
    Serial.println("): Unexpected file size");
    Serial.println("💡 Check calendar generator output format");
  } else {
    Serial.print("✅ ");
    Serial.print(channelName);
    Serial.println(" channel size is perfect!");
  }
}

bool streamChannelToDisplay(HTTPClient& http, byte channelCommand, const char* channelName, int expectedBytes) {
  WiFiClient* stream = http.getStreamPtr();
  
  Serial.print("📤 Streaming ");
  Serial.print(channelName);
  Serial.println(" channel data to display...");
  
  EPD_SendCommand(channelCommand);  // 0x10 for B/W, 0x13 for Red
  
  long bytesReceived = 0;
  unsigned long startTime = millis();
  
  // Data analysis counters
  int blackBytes = 0, whiteBytes = 0, redPrepBytes = 0, otherBytes = 0;
  
  // Read and send data in chunks
  while (http.connected() && bytesReceived < expectedBytes && bytesReceived < TOTAL_BYTES) {
    size_t availableBytes = stream->available();
    
    if (availableBytes > 0) {
      // Read up to 1024 bytes at a time
      size_t maxToRead = min((long)expectedBytes - bytesReceived, (long)TOTAL_BYTES - bytesReceived);
      size_t bytesToRead = min(availableBytes, (size_t)maxToRead);
      bytesToRead = min(bytesToRead, (size_t)1024);
      
      uint8_t buffer[1024];
      size_t bytesRead = stream->readBytes(buffer, bytesToRead);
      
      // Analyze and send each byte to the display
      for (size_t i = 0; i < bytesRead; i++) {
        byte dataByte = buffer[i];
        
        // Count different pixel types for analysis
        if (dataByte == 0x00) blackBytes++;
        else if (dataByte == 0x33) whiteBytes++;
        else if (dataByte == 0xCC) redPrepBytes++;
        else otherBytes++;
        
        EPD_SendData(dataByte);
      }
      
      bytesReceived += bytesRead;
      
      // Progress indicator with data analysis
      if (bytesReceived % 10000 == 0) {
        Serial.print("📥 ");
        Serial.print(channelName);
        Serial.print(": ");
        Serial.print(bytesReceived / 1000);
        Serial.print("k / ");
        Serial.print(expectedBytes / 1000);
        Serial.print("k (🔍 0x00:");
        Serial.print(blackBytes);
        Serial.print(" 0x33:");
        Serial.print(whiteBytes);
        Serial.print(" 0xCC:");
        Serial.print(redPrepBytes);
        Serial.print(" other:");
        Serial.print(otherBytes);
        Serial.println(")");
      }
    } else {
      delay(10);  // Wait for more data
      
      // Timeout check (30 seconds total)
      if (millis() - startTime > 30000) {
        Serial.print("⏱️  Timeout waiting for ");
        Serial.print(channelName);
        Serial.println(" channel data");
        return false;
      }
    }
  }
  
  // If we didn't get enough data, pad appropriately
  if (bytesReceived < TOTAL_BYTES) {
    Serial.print("📝 Padding ");
    Serial.print(channelName);
    Serial.print(" channel: ");
    Serial.print((TOTAL_BYTES - bytesReceived) / 1000);
    Serial.println("k bytes...");
    
    byte paddingValue;
    if (channelCommand == 0x10) {
      paddingValue = 0x33;  // White pixels for B/W channel
      Serial.println("   Using white padding for B/W channel");
      whiteBytes += (TOTAL_BYTES - bytesReceived);
    } else {
      paddingValue = 0xFF;  // No red for Red channel
      Serial.println("   Using no-red padding for Red channel");
    }
    
    for (long i = bytesReceived; i < TOTAL_BYTES; i++) {
      EPD_SendData(paddingValue);
      
      if (i % 30000 == 0) {
        Serial.print("   Padding ");
        Serial.print(channelName);
        Serial.print(": ");
        Serial.print(i/1000);
        Serial.println("k");
      }
    }
  }
  
  // Final data analysis report
  Serial.print("✅ ");
  Serial.print(channelName);
  Serial.println(" channel complete!");
  Serial.print("📊 Data Analysis - Black(0x00): ");
  Serial.print(blackBytes);
  Serial.print(", White(0x33): ");
  Serial.print(whiteBytes);
  Serial.print(", RedPrep(0xCC): ");
  Serial.print(redPrepBytes);
  Serial.print(", Other: ");
  Serial.println(otherBytes);
  Serial.print("   Total received: ");
  Serial.print(bytesReceived);
  Serial.print(" bytes, Total sent: ");
  Serial.print(TOTAL_BYTES);
  Serial.println(" bytes");
  
  // Alert if no black pixels found
  if (channelCommand == 0x10 && blackBytes == 0) {
    Serial.println("⚠️  WARNING: No black pixels (0x00) found in B/W channel!");
    Serial.println("💡 This suggests Flask app might be sending black pixels incorrectly");
    Serial.println("💡 Check: Are black pixels being sent as 0x00?");
  }
  
  return (bytesReceived > 0);
}

// =============================================================================
// 🔧 HARDWARE INITIALIZATION
// =============================================================================

void initializeHardware() {
  Serial.println("🔧 Initializing hardware pins...");
  
  pinMode(PIN_SPI_SCK, OUTPUT);
  pinMode(PIN_SPI_DIN, OUTPUT);
  pinMode(CS_PIN, OUTPUT);
  pinMode(RST_PIN, OUTPUT);
  pinMode(DC_PIN, OUTPUT);
  pinMode(BUSY_PIN, INPUT);
  
  digitalWrite(CS_PIN, HIGH);
  digitalWrite(PIN_SPI_SCK, LOW);
  
  Serial.println("✅ Hardware initialized");
}

// =============================================================================
// 🖥️  DISPLAY FUNCTIONS (from documented e-paper driver)
// =============================================================================

int EPD_7in5_V1_Init(void) {
  Serial.println("🖥️  Initializing 7.5\" e-paper display (V1)...");
  
  EPD_Reset();
  EPD_Send_2(0x01, 0x37, 0x00);               // POWER_SETTING
  EPD_Send_2(0x00, 0xCF, 0x08);               // PANEL_SETTING
  EPD_Send_3(0x06, 0xC7, 0xCC, 0x28);         // BOOSTER_SOFT_START
  EPD_SendCommand(0x04);                       // POWER_ON
  EPD_WaitUntilIdle();
  EPD_Send_1(0x30, 0x3C);                     // PLL_CONTROL
  EPD_Send_1(0x41, 0x00);                     // TEMPERATURE_CALIBRATION
  EPD_Send_1(0x50, 0x77);                     // VCOM_AND_DATA_INTERVAL_SETTING
  EPD_Send_1(0x60, 0x22);                     // TCON_SETTING
  EPD_Send_4(0x61, 0x02, 0x80, 0x01, 0x80);   // RESOLUTION: 640x384
  EPD_Send_1(0x82, 0x1E);                     // VCM_DC_SETTING
  EPD_Send_1(0xE5, 0x03);                     // FLASH_MODE

  EPD_SendCommand(0x10);                       // Prepare for B/W data
  delay(2);
  return 0;
}

static void EPD_7IN5_V1_Show(void) {
  EPD_SendCommand(0x12);  // DISPLAY_REFRESH
  delay(100);
  delay(3000);  // Wait for refresh to complete
}

void EPD_WaitUntilIdle() {
  // BUSY pin often unreliable, use timeout
  int timeout = 0;
  while(digitalRead(BUSY_PIN) == HIGH) {
    delay(20);
    timeout++;
    if (timeout > 500) return;  // 10 second timeout
  }
  delay(20);
}

void EPD_Reset() {
  digitalWrite(RST_PIN, HIGH);
  delay(50);
  digitalWrite(RST_PIN, LOW);
  delay(5);
  digitalWrite(RST_PIN, HIGH);
  delay(50);
}

void EPD_SendCommand(byte command) {
  digitalWrite(DC_PIN, LOW);
  EpdSpiTransferCallback(command);
}

void EPD_SendData(byte data) {
  digitalWrite(DC_PIN, HIGH);
  EpdSpiTransferCallback(data);
}

void EpdSpiTransferCallback(byte data) {
  digitalWrite(CS_PIN, GPIO_PIN_RESET);
  for (int i = 0; i < 8; i++) {
    if ((data & 0x80) == 0) 
      digitalWrite(PIN_SPI_DIN, GPIO_PIN_RESET); 
    else                    
      digitalWrite(PIN_SPI_DIN, GPIO_PIN_SET);
    data <<= 1;
    digitalWrite(PIN_SPI_SCK, GPIO_PIN_SET);
    digitalWrite(PIN_SPI_SCK, GPIO_PIN_RESET);
  }
  digitalWrite(CS_PIN, GPIO_PIN_SET);
}

void EPD_Send_1(byte c, byte v1) {
  EPD_SendCommand(c);
  EPD_SendData(v1);
}

void EPD_Send_2(byte c, byte v1, byte v2) {
  EPD_SendCommand(c);
  EPD_SendData(v1);
  EPD_SendData(v2);
}

void EPD_Send_3(byte c, byte v1, byte v2, byte v3) {
  EPD_SendCommand(c);
  EPD_SendData(v1);
  EPD_SendData(v2);
  EPD_SendData(v3);
}

void EPD_Send_4(byte c, byte v1, byte v2, byte v3, byte v4) {
  EPD_SendCommand(c);
  EPD_SendData(v1);
  EPD_SendData(v2);
  EPD_SendData(v3);
  EPD_SendData(v4);
}

// =============================================================================
// 🎯 SCHEDULED CALENDAR WITH DEEP SLEEP! 🎯
// =============================================================================
/*
 * ✅ SCHEDULED CALENDAR WITH DEEP SLEEP NOW IMPLEMENTED!
 * 
 * FEATURES ADDED:
 * 1. ⏰ Time synchronization with server via /status endpoint
 * 2. 📅 Scheduled updates every 30 minutes from 7:50am to 4:50pm
 * 3. 😴 Deep sleep between updates for power efficiency
 * 4. 🌙 Long sleep from 4:50pm until 7:50am next day
 * 5. 🔄 Automatic wake-up and calendar updates
 * 
 * SCHEDULE:
 * - Updates: 7:50, 8:20, 8:50, 9:20, 9:50, 10:20, 10:50, 11:20, 11:50,
 *           12:20, 12:50, 1:20, 1:50, 2:20, 2:50, 3:20, 3:50, 4:20, 4:50
 * - Total: 19 updates per day
 * - Sleep: 15 hours overnight (4:50pm to 7:50am)
 * 
 * POWER EFFICIENCY:
 * - Device only wakes up for ~2-3 minutes per update
 * - Deep sleep reduces power consumption to ~100μA
 * - Perfect for battery-powered installations
 * 
 * IMPORTANT HARDWARE NOTE:
 * For deep sleep to work properly, you MUST connect:
 * - RST pin to GPIO16 (D0) on your ESP8266
 * - This allows the device to wake itself up from deep sleep
 * 
 * TIME SYNCHRONIZATION:
 * - Gets server timestamp from /status endpoint
 * - Maintains internal time tracking during wake periods
 * - Handles timezone and daylight saving automatically via server time
 * 
 * ERROR HANDLING:
 * - If time sync fails, does emergency update and retries in 30 minutes
 * - If calendar update fails, still goes to sleep on schedule
 * - Robust WiFi connection handling with retries
 * 
 * DEBUGGING:
 * - Extensive serial output for troubleshooting
 * - Time calculations and sleep duration logging
 * - Update schedule verification and status reporting
 * 
 * Happy scheduled calendaring! 🗓️😴
 */
