#include "DS1302.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "TaskScheduler.h"

#define _TASK_TIMEOUT
#define _TASK_SLEEP_ON_IDLE_RUN

// Pin definitions
#define RTC_RST_PIN 8
#define RTC_IO_PIN 10
#define RTC_SCLK_PIN 9

#define MODE_BUTTON_PIN 3
#define INCREMENT_BUTTON_PIN 4
#define CONFIRM_BUTTON_PIN 5
#define BACK_BUTTON_PIN 6

#define LED1_PIN 7
#define LED2_PIN 11
#define LED3_PIN 12
#define LED4_PIN 13
#define BUZZER_PIN 2

#define LCD_ADDR 0x27
#define LCD_COLS 16
#define LCD_ROWS 2

Scheduler ts;

// RTC and LCD initialization
DS1302 rtc(RTC_RST_PIN, RTC_IO_PIN, RTC_SCLK_PIN);
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// Storage for scheduled times
int ledHours[4] = {6, 17, 18, 22};  // Default times for slots
int ledMinutes[4] = {0, 17, 0, 0};   // Default times for slots

int selectedSlot = 0;       // Currently selected slot (0 to 3)
bool isConfigMode = false;  // Flag for configuration mode
bool isConfigMinutes = false; // Toggle between hour and minute configuration

// Individual Task instances
Task taskSlot0(0, TASK_ONCE, NULL, &ts, false);
Task taskSlot1(0, TASK_ONCE, NULL, &ts, false);
Task taskSlot2(0, TASK_ONCE, NULL, &ts, false);
Task taskSlot3(0, TASK_ONCE, NULL, &ts, false);

// Initialize tasks for buzzer and LEDs
void setupTasks() {
  // Initialize tasks
  taskSlot0.setCallback(handleTimeout0);
  taskSlot1.setCallback(handleTimeout1);
  taskSlot2.setCallback(handleTimeout2);
  taskSlot3.setCallback(handleTimeout3);
}

// Timeout handler for each medication
void handleTimeout(int slot) {
  unsigned long startTime = millis();
  unsigned long duration = 60000; // 1 minute
  bool buzzerState = false;

  while (millis() - startTime < duration) {
    buzzerState = !buzzerState;
    digitalWrite(BUZZER_PIN, buzzerState);
    digitalWrite(slot, HIGH);
    delay(2000); // Toggle every 2 seconds
  }

  // Turn off buzzer and LED after timeout
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(slot, LOW);
}

// Specific timeout handlers for each slot
void handleTimeout0() { handleTimeout(7); }
void handleTimeout1() { handleTimeout(11); }
void handleTimeout2() { handleTimeout(12); }
void handleTimeout3() { handleTimeout(13); }

void setup() {
  // Initialize Serial Monitor
  Serial.begin(9600);

  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");
  delay(2000);

  // Initialize buttons, LEDs, and buzzer
  pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(INCREMENT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(CONFIRM_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BACK_BUTTON_PIN, INPUT_PULLUP);

  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(LED3_PIN, OUTPUT);
  pinMode(LED4_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  lcd.clear();
  lcd.print("Ready");
  delay(2000);

  // Initialize tasks
  setupTasks();
}

void loop() {
  ts.execute();

  // Regular operations
  if (!isConfigMode) {
    displayNextSlotAndCurrentTime();
    checkMedications();

    if (digitalRead(MODE_BUTTON_PIN) == LOW) {
      enterConfigMode();
    }

    if (Serial.available() > 0) {
      char input = Serial.read();
      draw_menu(input);
    }
  } else {
    handleConfigMode();
  }
}

// Check and schedule medication tasks
void checkMedications() {
  Time now = rtc.getTime(); // Use appropriate method to get current time

  if (now.hour == ledHours[0] && now.min == ledMinutes[0] && !taskSlot0.isEnabled()) {
    taskSlot0.enable();
  }
  if (now.hour == ledHours[1] && now.min == ledMinutes[1] && !taskSlot1.isEnabled()) {
    taskSlot1.enable();
  }
  if (now.hour == ledHours[2] && now.min == ledMinutes[2] && !taskSlot2.isEnabled()) {
    taskSlot2.enable();
  }
  if (now.hour == ledHours[3] && now.min == ledMinutes[3] && !taskSlot3.isEnabled()) {
    taskSlot3.enable();
  }
}


// Display the next slot based on current time
void displayNextSlotAndCurrentTime() {
  Time now = rtc.getTime();
  int nextSlot = getNextSlot(now.hour, now.min);

  // Display next slot time
  lcd.setCursor(0, 0);
  lcd.print("Next: ");
  lcd.print(nextSlot + 1);
  lcd.print(" ");
  lcd.print(ledHours[nextSlot] < 10 ? "0" : "");
  lcd.print(ledHours[nextSlot]);
  lcd.print(":");
  lcd.print(ledMinutes[nextSlot] < 10 ? "0" : "");
  lcd.print(ledMinutes[nextSlot]);
  lcd.print("  ");

  // Display current time below
  lcd.setCursor(0, 1);
  lcd.print("Now: ");
  lcd.print(now.hour < 10 ? "0" : "");
  lcd.print(now.hour);
  lcd.print(":");
  lcd.print(now.min < 10 ? "0" : "");
  lcd.print(now.min);
  lcd.print(":");
  lcd.print(now.sec < 10 ? "0" : "");
  lcd.print(now.sec);

  delay(500); // Refresh every half second
}

// Get the next slot index based on current time
int getNextSlot(int currentHour, int currentMinute) {
  for (int i = 0; i < 4; i++) {
    if ((ledHours[i] > currentHour) || 
        (ledHours[i] == currentHour && ledMinutes[i] > currentMinute)) {
      return i;
    }
  }
  return 0; // Return the first slot if all times have passed
}

// Enter configuration mode via button
void enterConfigMode() {
  isConfigMode = true;
  isConfigMinutes = false; // Start with hour configuration
  lcd.clear();
  lcd.print("Set Slot ");
  lcd.print(selectedSlot + 1);
  delay(1000);
}

// Handle configuration mode
void handleConfigMode() {
  static int hour = ledHours[selectedSlot];
  static int minute = ledMinutes[selectedSlot];

  lcd.setCursor(0, 0);
  lcd.print("Slot ");
  lcd.print(selectedSlot + 1); // Display current slot (1-based index)
  lcd.setCursor(0, 1);

  if (!isConfigMinutes) {
    lcd.print("Hour: ");
    lcd.print(hour < 10 ? "0" : "");
    lcd.print(hour);
    lcd.print("   ");
  } else {
    lcd.print("Minute: ");
    lcd.print(minute < 10 ? "0" : "");
    lcd.print(minute);
    lcd.print("   ");
  }

  // Mode button logic to switch slots
  if (digitalRead(MODE_BUTTON_PIN) == LOW) {
    selectedSlot = (selectedSlot + 1) % 4; // Cycle through slots 0 to 3
    hour = ledHours[selectedSlot];        // Load the new slot's hour
    minute = ledMinutes[selectedSlot];    // Load the new slot's minute
    lcd.clear();
    lcd.print("Slot ");
    lcd.print(selectedSlot + 1);
    delay(1000); // Debounce delay
  }

  // Increment button logic
  if (digitalRead(INCREMENT_BUTTON_PIN) == LOW) {
    if (!isConfigMinutes) {
      hour = (hour + 1) % 24;
    } else {
      minute = (minute + 1) % 60;
    }
    delay(200); // Debounce
  }

  // Confirm button logic
  if (digitalRead(CONFIRM_BUTTON_PIN) == LOW) {
    if (!isConfigMinutes) {
      isConfigMinutes = true; // Move to minute configuration
      delay(200); // Debounce
    } else {
      // Save configured time
      ledHours[selectedSlot] = hour;
      ledMinutes[selectedSlot] = minute;

      lcd.clear();
      lcd.print("Saved Slot ");
      lcd.print(selectedSlot + 1);
      delay(2000);
      isConfigMode = false; // Exit configuration mode
    }
  }

  // Back button logic
  if (digitalRead(BACK_BUTTON_PIN) == LOW) {
    lcd.clear();
    lcd.print("Exiting Config");
    delay(1000);
    isConfigMode = false; // Exit configuration mode
  }
}

// Draw the menu and handle input via Serial Monitor
void draw_menu(char input) {
  switch (input) {
    case '1': // Set Date/Time
      set_time();
      break;
    case '2': // Schedule Drug Times
      schedule_drugs();
      break;
    default:
      Serial.println("Invalid option.");
      break;
  }
}

void set_time() {
  Serial.println("Enter Hour (0-23): ");
  while (Serial.available() == 0);
  int hour = Serial.parseInt();

  Serial.println("Enter Minute (0-59): ");
  while (Serial.available() == 0);
  int minute = Serial.parseInt();

  Serial.println("Enter Second (0-59): ");
  while (Serial.available() == 0);
  int second = Serial.parseInt();

  rtc.setTime(second, minute, hour);

  Serial.println("Time updated!");
}

void schedule_drugs() {
  Serial.println("Enter slot number (1-4): ");
  while (Serial.available() == 0);
  int slot = Serial.parseInt() - 1;  // Convert to 0-based index
  if (slot < 0 || slot > 3) {
    Serial.println("Invalid slot.");
    return;
  }

  Serial.println("Enter Hour (0-23): ");
  while (Serial.available() == 0);
  int hour = Serial.parseInt();
  ledHours[slot] = hour;

  Serial.println("Enter Minute (0-59): ");
  while (Serial.available() == 0);
  int minute = Serial.parseInt();
  ledMinutes[slot] = minute;

  Serial.print("Slot ");
  Serial.print(slot + 1);
  Serial.print(" scheduled at ");
  Serial.print(hour);
  Serial.print(":");
  Serial.println(minute);
}
