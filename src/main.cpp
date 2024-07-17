#include <Arduino.h>
#include <M5Stack.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <atomic>

#define FACE_JOY_ADDR 0x5e

// WiFi settings
const char* ssid = "ntlab1802";
const char* password = "hoge1802";
const char* udpAddress = "192.168.30.150";  // Broadcast address, adjust if needed
const int udpPort = 4210;

WiFiUDP udp;

// Atomic variables for inter-core communication
std::atomic<uint16_t> atomic_x_data(0);
std::atomic<uint16_t> atomic_y_data(0);
std::atomic<uint8_t> atomic_button_data(0);
std::atomic<bool> atomic_btnA(false);
std::atomic<bool> atomic_btnB(false);
std::atomic<bool> atomic_btnC(false);

std::atomic<uint8_t> frequency(0);
std::atomic<int_fast8_t> roll(0);
std::atomic<int_fast8_t> pitch(0);


int mapJoystick(uint16_t value) {
  return map(value, 0, 4095, -45, 45);
}


void Led(int indexOfLED, int r, int g, int b) {
  Wire.beginTransmission(FACE_JOY_ADDR);
  Wire.write(indexOfLED);
  Wire.write(r);
  Wire.write(g);
  Wire.write(b);
  Wire.endTransmission();
}

void Init() {
  Wire.begin();
  for (int i = 0; i < 256; i++) {
    Wire.beginTransmission(FACE_JOY_ADDR);
    Wire.write(i % 4);
    Wire.write(random(256) * (256 - i) / 256);
    Wire.write(random(256) * (256 - i) / 256);
    Wire.write(random(256) * (256 - i) / 256);
    Wire.endTransmission();
    delay(2);
  }
  for (int i = 0; i < 4; i++) {
    Led(i, 0, 0, 0);
  }
}

void displayTask(void * parameter) {
  while (true) {
    // M5.Lcd.fillRect(0, 60, 320, 180, BLACK);  // Clear previous data
    M5.Lcd.setCursor(10, 60);
    M5.Lcd.printf("Joystick: x:%d y:%d btn:%d", 
      atomic_x_data.load(), atomic_y_data.load(), atomic_button_data.load());

    double frequency_ = frequency.load() * 0.1f;

    pitch.store(
      map(atomic_y_data.load(), 250, 740, 45, -45)
    );

    roll.store(
      map(atomic_x_data.load(), 280, 810, -45, 45)
    );   

    M5.Lcd.setCursor(10, 120);
    M5.Lcd.printf("Frequency: %.1f   ", frequency_);

    M5.Lcd.setCursor(10, 150);
    M5.Lcd.printf("Roll: %d deg    ", roll.load());

    M5.Lcd.setCursor(10, 180);
    M5.Lcd.printf("Pitch: %d deg    ", pitch.load());

    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}

void udpTask(void * parameter) {
  while (true) {
    char buffer[50];
    snprintf(buffer, sizeof(buffer), "x:%d,y:%d,btn:%d,A:%d,B:%d,C:%d", 
      atomic_x_data.load(), atomic_y_data.load(), atomic_button_data.load(),
      atomic_btnA.load(), atomic_btnB.load(), atomic_btnC.load());
    
    udp.beginPacket(udpAddress, udpPort);
    udp.write((uint8_t*)buffer, strlen(buffer));
    udp.endPacket();

    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}

void setup() {
  M5.begin();
  M5.Power.begin();
  M5.Lcd.clear();
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(40, 0);
  M5.Lcd.println("OrnibiBot Controller");
  Init();

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    M5.Lcd.print(".");
  }
  M5.Lcd.println("\nWiFi connected");

  // Start tasks on different cores
  xTaskCreatePinnedToCore(displayTask, "DisplayTask", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(udpTask, "UDPTask", 4096, NULL, 1, NULL, 1);
}

void loop() {
  M5.update();

  Wire.requestFrom(FACE_JOY_ADDR, 5);
  if (Wire.available()) {
    uint8_t y_data_L = Wire.read();
    uint8_t y_data_H = Wire.read();
    uint8_t x_data_L = Wire.read();
    uint8_t x_data_H = Wire.read();
    uint8_t button_data = Wire.read();

    atomic_x_data.store(x_data_H << 8 | x_data_L);
    atomic_y_data.store(y_data_H << 8 | y_data_L);
    atomic_button_data.store(button_data);

    // // LED control based on joystick
    // if (atomic_x_data.load() > 600) {
    //   Led(2, 0, 0, 50);
    //   Led(0, 0, 0, 0);
    // } else if (atomic_x_data.load() < 400) {
    //   Led(0, 0, 0, 50);
    //   Led(2, 0, 0, 0);
    // } else {
    //   Led(0, 0, 0, 0);
    //   Led(2, 0, 0, 0);
    // }

    // if (atomic_y_data.load() > 600) {
    //   Led(3, 0, 0, 50);
    //   Led(1, 0, 0, 0);
    // } else if (atomic_y_data.load() < 400) {
    //   Led(1, 0, 0, 50);
    //   Led(3, 0, 0, 0);
    // } else {
    //   Led(1, 0, 0, 0);
    //   Led(3, 0, 0, 0);
    // }
  }

  if(M5.BtnA.isPressed()){
    frequency = 0; 
    delay(100);  
  } 
  else if(M5.BtnB.isPressed()) {
    if(frequency>=5) frequency -= 5;
    delay(100);
  }
  else if(M5.BtnC.isPressed()){
    if(frequency<50) frequency += 5;
    delay(100);
  }

  delay(50);
}