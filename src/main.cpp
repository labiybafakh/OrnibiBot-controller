#include <Arduino.h>
#include <M5Stack.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <atomic>

#define FACE_JOY_ADDR 0x5e

// WiFi settings
const char* ssid = "ornibibot";
const char* password = "ornibibot5208";
const char* udpAddress = "192.168.3.15";  // Broadcast address, adjust if needed
const int udpPort = 4210;

WiFiUDP udp;


struct ornibibot_param{
  std::atomic<uint8_t> frequency;
  std::atomic<int8_t> roll;
  std::atomic<int8_t> pitch;
};

// Atomic variables for inter-core communication
std::atomic<uint16_t> atomic_x_data(0);
std::atomic<uint16_t> atomic_y_data(0);
std::atomic<uint8_t> atomic_button_data(0);
std::atomic<bool> atomic_btnA(false);
std::atomic<bool> atomic_btnB(false);
std::atomic<bool> atomic_btnC(false);

ornibibot_param ornibibot_parameter;

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

void drawWifiStrength(int x, int y, int width, int height) {
  int strength = WiFi.RSSI();
  int bars = 0;
  
  if (strength > -55) bars = 4;
  else if (strength > -65) bars = 3;
  else if (strength > -75) bars = 2;
  else if (strength > -85) bars = 1;
  
  M5.Lcd.drawRect(x, y, width, height, TFT_WHITE);
  
  int barWidth = width / 4;
  for (int i = 0; i < 4; i++) {
    if (i < bars) {
      M5.Lcd.fillRect(x + i * barWidth, y + height - (i + 1) * height / 4, 
                      barWidth - 1, (i + 1) * height / 4, TFT_GREEN);
    } else {
      M5.Lcd.drawRect(x + i * barWidth, y + height - (i + 1) * height / 4, 
                      barWidth - 1, (i + 1) * height / 4, TFT_WHITE);
    }
  }
}

void displayTask(void * parameter) {
  while (true) {

    if(WiFi.status() == WL_CONNECTED){
      M5.Lcd.setCursor(0, 30);
      M5.Lcd.println("WiFi connected    ");
      drawWifiStrength(250, 30, 60, 20);
  }
    else{
        M5.Lcd.setCursor(0, 30);
        M5.Lcd.print("WiFi connecting    ");
        M5.Lcd.fillRect(250, 30, 60, 20, TFT_BLACK);
    }


    M5.Lcd.setCursor(10, 60);

    double frequency_ = ornibibot_parameter.frequency.load() * 0.1f;



    M5.Lcd.setCursor(10, 120);
    M5.Lcd.printf("Frequency: %.1f   ", frequency_);

    M5.Lcd.setCursor(10, 150);
    M5.Lcd.printf("Roll: %d deg    ", ornibibot_parameter.roll.load());

    M5.Lcd.setCursor(10, 180);
    M5.Lcd.printf("Pitch: %d deg    ", ornibibot_parameter.pitch.load());

    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}

void udpTask(void * parameter) {
  while (true) {

    uint8_t buffer[3];

    buffer[0] = (uint8_t)ornibibot_parameter.frequency;
    buffer[1] = (uint8_t)ornibibot_parameter.roll;
    buffer[2] = (uint8_t)ornibibot_parameter.pitch;
    
    udp.beginPacket(udpAddress, udpPort);
    udp.write(buffer, sizeof(buffer));
    udp.endPacket();

    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}

void setup() {
  M5.begin();
  M5.Speaker.mute();  // Issue of noisy sound
  M5.Power.begin();
  M5.Lcd.clear();
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(40, 0);
  M5.Lcd.println("OrnibiBot Controller");
  Init();

  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  M5.Lcd.setCursor(0, 30);
  M5.Lcd.print("WiFi connecting");

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }
    M5.Lcd.setCursor(0, 30);
  M5.Lcd.println("WiFi connected    ");
  udp.begin(udpPort);

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
  }

  ornibibot_parameter.pitch = map(atomic_y_data.load(), 250, 740, 45, -45);
  ornibibot_parameter.roll = map(atomic_x_data.load(), 280, 810, -45, 45);


  if(M5.BtnA.isPressed()){
    ornibibot_parameter.frequency = 0; 
    delay(100);  
  } 
  else if(M5.BtnB.isPressed()) {
    if(ornibibot_parameter.frequency.load()>=5) ornibibot_parameter.frequency -= 5;
    delay(100);
  }
  else if(M5.BtnC.isPressed()){
    if(ornibibot_parameter.frequency.load()<50) ornibibot_parameter.frequency += 5;
    delay(100);
  }

  delay(50);
}