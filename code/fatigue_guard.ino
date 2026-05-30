#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define EMG_PIN 34

// Use regular hardware I2C pins for ESP32 (SDA = 21, SCL = 22 default)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

float smoothedValue = 0;
float baselineIntensity = 0;
bool calibrated = false;
int calibrationTimer = 0;

// --- FATIGUE "GAS TANK" VARIABLES ---
float currentFatigue = 0;         
float maxFatigueLimit = 5000;     
float activeThreshold = 0;        

void setup() {
  Serial.begin(115200);
  
  // Initialize standard 12-bit ADC for ESP32 (0-4095 range)
  analogReadResolution(12); 
  
  // Start the OLED (Address 0x3C)
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
}

void loop() {
  // 1. Read and Smooth (90% old, 10% new)
  int rawValue = analogRead(EMG_PIN);
  smoothedValue = (smoothedValue * 0.9) + (rawValue * 0.1); 

  // 2. Calibration Phase 
  if (!calibrated) {
    calibrationTimer++;
    showCalibrating();
    if (calibrationTimer > 50) {
       baselineIntensity = smoothedValue;
       // Requires a 60% increase over resting baseline to count as a deliberate flex
       activeThreshold = baselineIntensity * 1.6; 
       calibrated = true;
       Serial.print("Calibrated! Baseline: "); Serial.print(baselineIntensity);
       Serial.print(" | Threshold: "); Serial.println(activeThreshold);
    }
    delay(50);
    return; 
  }

  // 3. The Fatigue Math
  if (smoothedValue > activeThreshold) {
    float effort = smoothedValue - activeThreshold;
    currentFatigue += (effort * 0.2); 
  } else {
    currentFatigue -= 2.0;            
    if (currentFatigue < 0) currentFatigue = 0; 
  }

  // 4. Trigger Alarm if Tank is Full
  if (currentFatigue >= maxFatigueLimit) {
    triggerAlarm();
    currentFatigue = 0; // Reset tank after alerting
  } else {
    showDualStatus();
  }
  
  delay(50); 
}

// --- DISPLAY & ALARM FUNCTIONS ---

void showCalibrating() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(20, 25);
  display.print("Calibrating...");
  display.display();
}

void triggerAlarm() {
  // Flash screen warning
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(15, 25);
  display.print("REST NOW!");
  display.invertDisplay(true);
  display.display();
  
  delay(2000); // UI freeze acceptable here for an immediate urgent stop flag
  
  display.invertDisplay(false);
}

void showDualStatus() {
  display.clearDisplay();
  
  // Top Bar: Live Effort Speedometer
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Live Effort:");
  display.drawRect(0, 10, 128, 10, SSD1306_WHITE);
  
  // Manual precise float scaling instead of integer map()
  float effortRange = activeThreshold * 2.0; // dynamic window size
  float effortPct = (smoothedValue - activeThreshold) / effortRange;
  int effortBar = constrain(effortPct * 128, 0, 128);
  display.fillRect(0, 10, effortBar, 10, SSD1306_WHITE);

  // Bottom Bar: Total Fatigue Gas Tank
  display.setCursor(0, 32);
  display.print("Total Fatigue:");
  display.drawRect(0, 44, 128, 15, SSD1306_WHITE); 
  
  float fatiguePct = currentFatigue / maxFatigueLimit;
  int fatigueBar = constrain(fatiguePct * 128, 0, 128);
  display.fillRect(0, 44, fatigueBar, 15, SSD1306_WHITE);
  
  display.display();
}