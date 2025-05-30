/*
 * 🎉 E-Paper 7.5" 3-Color Display - COMPLETE REFERENCE 🎉
 * 
 * HARDWARE: 7.5" E-Paper(B) x027/17118-20180424 (640×384, 3-color: Black/White/Red)
 * DRIVER: Waveshare ESP8266 driver board
 * RIBBON: WF0583CZ09
 * 
 * =============================================================================
 * 🔴 RED CHANNEL SECRET - THE BREAKTHROUGH DISCOVERY! 🔴
 * =============================================================================
 * 
 * After extensive reverse engineering, we discovered the magic formula:
 * 
 * BLACK PIXELS:  B/W Channel = 0x00, Red Channel = 0x00 (or any value)
 * WHITE PIXELS:  B/W Channel = 0x33, Red Channel = 0x00 (or any value) 
 * RED PIXELS:    B/W Channel = 0xCC, Red Channel = 0xFF  ⭐ MAGIC COMBO! ⭐
 * 
 * Key Insights:
 * - Red channel is NOT a simple overlay - it requires specific B/W values
 * - 0xCC in B/W channel + 0xFF in Red channel = RED pixels
 * - Other B/W values (0x00, 0x33, 0x55, etc.) do NOT produce red
 * - Full 122,880 bytes (640×384÷2) memory works perfectly with this method
 * - Both channels must be sent: 0x10 (B/W) first, then 0x13 (Red)
 * 
 * =============================================================================
 * 📐 DISPLAY SPECIFICATIONS 📐
 * =============================================================================
 * 
 * Resolution: 640×384 pixels
 * Memory: 122,880 bytes (640×384÷2, because 2 pixels per byte)
 * Memory Layout: Horizontal bands (not row-by-row)
 * 
 * =============================================================================
 * 🔌 PIN CONNECTIONS 🔌
 * =============================================================================
 * 
 * ESP8266 NodeMCU → Display Driver Board
 * GPIO 14 (D5)    → SCK  (Clock)
 * GPIO 13 (D7)    → DIN  (Data In) 
 * GPIO 15 (D8)    → CS   (Chip Select)
 * GPIO 2  (D4)    → RST  (Reset)
 * GPIO 4  (D2)    → DC   (Data/Command)
 * GPIO 5  (D1)    → BUSY (Busy Status - may not work reliably)
 * 3.3V            → VCC
 * GND             → GND
 * 
 * NOTE: BUSY pin often gets stuck HIGH. Use fixed delays instead of busy-wait.
 * 
 * =============================================================================
 * 🎨 HOW TO DRAW PATTERNS 🎨
 * =============================================================================
 * 
 * 1. Initialize the display with EPD_7in5_V1_Init()
 * 
 * 2. Send B/W channel data (0x10):
 *    - Loop through all 122,880 bytes
 *    - For each pixel position, decide color and send corresponding B/W value:
 *      * Black pixel → send 0x00
 *      * White pixel → send 0x33  
 *      * Red pixel   → send 0xCC (prepares for red)
 * 
 * 3. Send Red channel data (0x13):
 *    - Loop through all 122,880 bytes again
 *    - For each pixel position:
 *      * Black/White pixel → send 0x00 (no red)
 *      * Red pixel         → send 0xFF (enable red)
 * 
 * 4. Refresh display with EPD_7IN5_V1_Show()
 * 
 * =============================================================================
 * 🧮 PIXEL COORDINATE CALCULATIONS 🧮
 * =============================================================================
 * 
 * The display memory is organized in horizontal bands, not individual pixels.
 * To calculate positions:
 * 
 * For byte index i (0 to 122,879):
 * - Rough row: y = i / (DISPLAY_WIDTH / 2) = i / 320
 * - Rough col: x = (i % 320) * 2
 * 
 * For sections (dividing screen into 8 vertical strips):
 * - Section = (i * 8) / 122880
 * 
 * For stripes or blocks:
 * - Use modulo operations: i % stripe_width
 * 
 * =============================================================================
 */

// Pin definitions
#define PIN_SPI_SCK 14
#define PIN_SPI_DIN 13
#define CS_PIN 15
#define RST_PIN 2
#define DC_PIN 4
#define BUSY_PIN 5

#define GPIO_PIN_SET 1
#define GPIO_PIN_RESET 0

// Display dimensions
#define DISPLAY_WIDTH 640
#define DISPLAY_HEIGHT 384
#define TOTAL_BYTES 122880  // 640*384/2

// Color definitions
#define COLOR_BLACK 0
#define COLOR_WHITE 1  
#define COLOR_RED 2

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("🎉🎉🎉 E-PAPER VICTORY DEMO! 🎉🎉🎉");
  Serial.println("Celebrating the red channel breakthrough!");
  
  // Initialize pins
  pinMode(PIN_SPI_SCK, OUTPUT);
  pinMode(PIN_SPI_DIN, OUTPUT);
  pinMode(CS_PIN, OUTPUT);
  pinMode(RST_PIN, OUTPUT);
  pinMode(DC_PIN, OUTPUT);
  pinMode(BUSY_PIN, INPUT);
  
  digitalWrite(CS_PIN, HIGH);
  digitalWrite(PIN_SPI_SCK, LOW);
  
  runVictoryDemo();
}

void loop() {
  delay(10000);
}

void runVictoryDemo() {
  Serial.println("=== DEMO 1: Victory Banner ===");
  drawVictoryBanner();
  
  Serial.println("\nPress any key for Demo 2...");
  waitForKey();
  
  Serial.println("=== DEMO 2: Color Stripes ===");
  drawColorStripes();
  
  Serial.println("\nPress any key for Demo 3...");
  waitForKey();
  
  Serial.println("=== DEMO 3: Red Border Frame ===");
  drawRedBorderFrame();
  
  Serial.println("\nPress any key for Demo 4...");
  waitForKey();
  
  Serial.println("=== DEMO 4: Checkerboard Pattern ===");
  drawCheckerboard();
  
  Serial.println("\n🎉 VICTORY DEMO COMPLETE! 🎉");
  Serial.println("You've mastered the 3-color e-paper display!");
}

void drawVictoryBanner() {
  Serial.println("Drawing celebration banner...");
  Serial.println("Expected pattern: Red-Red-Black-White-Black-Red-White...");
  
  EPD_7in5_V1_Init();
  
  // Create victory banner with horizontal stripes
  // Pattern explanation:
  // - y < 80:  Red stripe (top)
  // - y < 120: Black stripe  
  // - y < 160: White stripe
  // - y < 200: Black stripe
  // - y < 240: Red stripe
  // - y >= 240: White stripe (bottom)
  drawColorPattern([](long pixelIndex) -> int {
    long y = pixelIndex / (DISPLAY_WIDTH / 2);  // Convert byte index to rough row
    
    if (y < 80) {
      return COLOR_RED;    // Section 0-1: Top red stripe
    } else if (y < 120) {
      return COLOR_BLACK;  // Section 2: Black banner area
    } else if (y < 160) {
      return COLOR_WHITE;  // Section 3: White text area
    } else if (y < 200) {
      return COLOR_BLACK;  // Section 4: Another black stripe
    } else if (y < 240) {
      return COLOR_RED;    // Section 5: Another red stripe
    } else {
      return COLOR_WHITE;  // Section 6+: Bottom white area
    }
  });
  
  Serial.println("Victory banner complete!");
  Serial.println("Should show: Red(top)-Red-Black-White-Black-Red-White(bottom)");
}

void drawColorStripes() {
  Serial.println("Drawing vertical color stripes...");
  Serial.println("Expected: Repeating Black-White-Red vertical stripes");
  
  EPD_7in5_V1_Init();
  
  // Vertical color stripes across the screen
  // Each stripe is 80 pixels wide, pattern repeats every 3 stripes
  drawColorPattern([](long pixelIndex) -> int {
    long x = (pixelIndex % (DISPLAY_WIDTH / 2)) * 2;  // Convert to rough column
    
    long stripe = x / 80;  // 80-pixel wide stripes (640/80 = 8 stripes total)
    
    switch(stripe % 3) {
      case 0: return COLOR_BLACK;   // Stripes 0, 3, 6: Black
      case 1: return COLOR_WHITE;   // Stripes 1, 4, 7: White
      case 2: return COLOR_RED;     // Stripes 2, 5: Red
      default: return COLOR_WHITE;
    }
  });
  
  Serial.println("Color stripes complete!");
  Serial.println("Should show 8 vertical stripes: Black-White-Red-Black-White-Red-Black-White");
}

void drawRedBorderFrame() {
  Serial.println("Drawing red border frame...");
  
  EPD_7in5_V1_Init();
  
  // Red border with white center and black inner rectangle
  drawColorPattern([](long pixelIndex) -> int {
    long y = pixelIndex / (DISPLAY_WIDTH / 2);
    long x = (pixelIndex % (DISPLAY_WIDTH / 2)) * 2;
    
    // Red border (outer 40 pixels)
    if (x < 40 || x >= DISPLAY_WIDTH-40 || y < 40 || y >= DISPLAY_HEIGHT-40) {
      return COLOR_RED;
    }
    // Black inner rectangle
    else if (x >= 120 && x < DISPLAY_WIDTH-120 && y >= 120 && y < DISPLAY_HEIGHT-120) {
      return COLOR_BLACK;
    }
    // White middle area
    else {
      return COLOR_WHITE;
    }
  });
  
  Serial.println("Red border frame complete!");
}

void drawCheckerboard() {
  Serial.println("Drawing 3-color checkerboard...");
  
  EPD_7in5_V1_Init();
  
  // 3-color checkerboard pattern
  drawColorPattern([](long pixelIndex) -> int {
    long y = pixelIndex / (DISPLAY_WIDTH / 2);
    long x = (pixelIndex % (DISPLAY_WIDTH / 2)) * 2;
    
    long blockX = x / 60;  // 60-pixel blocks
    long blockY = y / 60;
    
    int pattern = (blockX + blockY) % 3;
    
    switch(pattern) {
      case 0: return COLOR_BLACK;
      case 1: return COLOR_WHITE;
      case 2: return COLOR_RED;
      default: return COLOR_WHITE;
    }
  });
  
  Serial.println("3-color checkerboard complete!");
}

// =============================================================================
// 🎨 GENERIC COLOR PATTERN DRAWING FUNCTION 🎨
// =============================================================================
// This is the core function that implements the red channel magic formula.
// It takes a function that returns COLOR_BLACK, COLOR_WHITE, or COLOR_RED 
// for each pixel position and draws it on the display.

void drawColorPattern(int (*getPixelColor)(long)) {
  Serial.println("Phase 1: Sending B/W channel data...");
  Serial.println("This prepares the base layer for all pixels");
  
  EPD_SendCommand(0x10);  // Switch to B/W channel (0x10)
  
  for (long i = 0; i < TOTAL_BYTES; i++) {
    int color = getPixelColor(i);  // Get desired color for this pixel
    byte bwValue;
    
    // CRITICAL: B/W channel values determine what the red channel can do
    switch(color) {
      case COLOR_BLACK: 
        bwValue = 0x00;   // Black pixels: B/W = 0x00
        break;
      case COLOR_WHITE: 
        bwValue = 0x33;   // White pixels: B/W = 0x33
        break;
      case COLOR_RED:   
        bwValue = 0xCC;   // Red pixels: B/W = 0xCC (MAGIC VALUE!)
        break;
      default:          
        bwValue = 0x33;   // Default to white
        break;
    }
    
    EPD_SendData(bwValue);
    
    // Progress indicator (every 30k bytes)
    if (i % 30000 == 0) {
      Serial.print("B/W: ");
      Serial.print(i/1000);
      Serial.println("k");
    }
  }
  
  Serial.println("Phase 2: Sending Red channel data...");
  Serial.println("This enables red color where B/W was set to 0xCC");
  
  EPD_SendCommand(0x13);  // Switch to Red channel (0x13)
  
  for (long i = 0; i < TOTAL_BYTES; i++) {
    int color = getPixelColor(i);  // Get desired color for this pixel
    byte redValue;
    
    // Red channel: Only enable red where we want red pixels
    switch(color) {
      case COLOR_BLACK: 
        redValue = 0x00;  // No red on black pixels
        break;
      case COLOR_WHITE: 
        redValue = 0x00;  // No red on white pixels
        break;
      case COLOR_RED:   
        redValue = 0xFF;  // Enable red! (MAGIC VALUE!)
        break;
      default:          
        redValue = 0x00;  // Default: no red
        break;
    }
    
    EPD_SendData(redValue);
    
    // Progress indicator (every 30k bytes)
    if (i % 30000 == 0) {
      Serial.print("Red: ");
      Serial.print(i/1000);
      Serial.println("k");
    }
  }
  
  Serial.println("Phase 3: Refreshing display...");
  EPD_7IN5_V1_Show();
  Serial.println("Pattern drawing complete!");
  Serial.println("");
  Serial.println("🎨 SUMMARY: B/W=0xCC + Red=0xFF = RED PIXELS! 🎨");
}

void waitForKey() {
  while (!Serial.available()) {
    delay(100);
  }
  while (Serial.available()) {
    Serial.read();
  }
}

// =============================================================================
// 🔧 DISPLAY INITIALIZATION & CONTROL FUNCTIONS 🔧
// =============================================================================

// Waveshare 7.5" V1 Display Initialization
// This exact sequence is required for the display to work properly
int EPD_7in5_V1_Init(void) {
  Serial.println("Initializing 7.5\" e-paper display (V1)...");
  
  EPD_Reset();                                  // Hardware reset
  EPD_Send_2(0x01, 0x37, 0x00);               // POWER_SETTING
  EPD_Send_2(0x00, 0xCF, 0x08);               // PANEL_SETTING (critical for V1)
  EPD_Send_3(0x06, 0xC7, 0xCC, 0x28);         // BOOSTER_SOFT_START
  EPD_SendCommand(0x04);                       // POWER_ON
  EPD_WaitUntilIdle();                         // Wait for power up
  EPD_Send_1(0x30, 0x3C);                     // PLL_CONTROL
  EPD_Send_1(0x41, 0x00);                     // TEMPERATURE_CALIBRATION
  EPD_Send_1(0x50, 0x77);                     // VCOM_AND_DATA_INTERVAL_SETTING
  EPD_Send_1(0x60, 0x22);                     // TCON_SETTING
  EPD_Send_4(0x61, 0x02, 0x80, 0x01, 0x80);   // RESOLUTION: 640x384 (0x0280 x 0x0180)
  EPD_Send_1(0x82, 0x1E);                     // VCM_DC_SETTING
  EPD_Send_1(0xE5, 0x03);                     // FLASH_MODE

  EPD_SendCommand(0x10);                       // Prepare for B/W data transmission
  delay(2);
  
  Serial.println("Display initialization complete!");
  return 0;
}

// Display refresh and power management
static void EPD_7IN5_V1_Show(void) {
  Serial.println("Refreshing display...");
  EPD_SendCommand(0x12);                       // DISPLAY_REFRESH command
  delay(100);                                  // Required delay
  delay(3000);                                 // Wait for refresh (BUSY pin unreliable)
  Serial.println("Display refresh complete!");
}

// BUSY pin monitoring (often unreliable - use fixed delays instead)
void EPD_WaitUntilIdle() {
  Serial.print("Checking BUSY pin...");
  int timeout = 0;
  while(digitalRead(BUSY_PIN) == HIGH) {
    delay(20);
    timeout++;
    if (timeout > 500) {  // 10 second timeout
      Serial.println(" TIMEOUT (BUSY stuck HIGH - normal behavior)");
      return;
    }
  }
  Serial.println(" Ready");
  delay(20);
}

// Hardware reset sequence
void EPD_Reset() {
  Serial.println("Performing hardware reset...");
  digitalWrite(RST_PIN, HIGH);
  delay(50);
  digitalWrite(RST_PIN, LOW);                  // Pull reset low
  delay(5);
  digitalWrite(RST_PIN, HIGH);                 // Release reset
  delay(50);                                   // Wait for reset to complete
}

// =============================================================================
// 🔌 LOW-LEVEL SPI COMMUNICATION 🔌
// =============================================================================

// Send command byte to display
void EPD_SendCommand(byte command) {
  digitalWrite(DC_PIN, LOW);                   // DC low = command mode
  EpdSpiTransferCallback(command);
}

// Send data byte to display  
void EPD_SendData(byte data) {
  digitalWrite(DC_PIN, HIGH);                  // DC high = data mode
  EpdSpiTransferCallback(data);
}

// Bit-banged SPI transfer (more reliable than hardware SPI)
void EpdSpiTransferCallback(byte data) {
  digitalWrite(CS_PIN, GPIO_PIN_RESET);        // Select display
  
  // Send 8 bits, MSB first
  for (int i = 0; i < 8; i++) {
    if ((data & 0x80) == 0) 
      digitalWrite(PIN_SPI_DIN, GPIO_PIN_RESET); 
    else                    
      digitalWrite(PIN_SPI_DIN, GPIO_PIN_SET);
      
    data <<= 1;                                // Shift to next bit
    digitalWrite(PIN_SPI_SCK, GPIO_PIN_SET);   // Clock high
    digitalWrite(PIN_SPI_SCK, GPIO_PIN_RESET); // Clock low
  }
  
  digitalWrite(CS_PIN, GPIO_PIN_SET);          // Deselect display
}

// Helper functions for multi-byte commands
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
// 🎯 USAGE EXAMPLES & QUICK REFERENCE 🎯
// =============================================================================
/*
 * TO CREATE YOUR OWN PATTERNS:
 * 
 * 1. Define a function that returns COLOR_BLACK, COLOR_WHITE, or COLOR_RED:
 *    int myPattern(long pixelIndex) {
 *      // Your logic here based on pixelIndex (0 to 122,879)
 *      return COLOR_RED;  // or COLOR_BLACK, COLOR_WHITE
 *    }
 * 
 * 2. Draw it:
 *    EPD_7in5_V1_Init();
 *    drawColorPattern(myPattern);
 * 
 * COORDINATE CALCULATIONS:
 * - Row (approx): y = pixelIndex / 320
 * - Column (approx): x = (pixelIndex % 320) * 2
 * - Section (0-7): section = (pixelIndex * 8) / 122880
 * 
 * MEMORY LAYOUT:
 * - Total bytes: 122,880 (640×384÷2)
 * - Each byte represents 2 pixels horizontally
 * - Memory flows left-to-right, top-to-bottom in bands
 * 
 * TROUBLESHOOTING:
 * - BUSY pin stuck HIGH: Normal, use fixed delays
 * - No red showing: Check B/W=0xCC + Red=0xFF combination
 * - Partial display update: Check wiring and power
 * - Display not responding: Verify initialization sequence
 * 
 * TESTED WORKING VALUES:
 * ✅ BLACK: B/W=0x00, Red=any (typically 0x00)
 * ✅ WHITE: B/W=0x33, Red=any (typically 0x00)  
 * ✅ RED:   B/W=0xCC, Red=0xFF (ONLY working combination!)
 * 
 * Happy coding! 🎨
 */
