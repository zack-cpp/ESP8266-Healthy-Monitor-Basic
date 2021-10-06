#include <Arduino.h>
#include <Wire.h>
#include "MAX30100_PulseOximeter.h"
#include <Blynk.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <MQUnifiedsensor.h>
 
#define REPORTING_PERIOD_MS 1000

//  MQ Sensor Defines
#define board "ESP8266"
#define Voltage_Resolution 3.3
#define pin A0 
#define type "MQ-135" //MQ135
#define ADC_Bit_Resolution 10 // For arduino UNO/MEGA/NANO
#define RatioMQ135CleanAir 3.6//RS / R0 = 3.6 ppm 

PulseOximeter pox;
MQUnifiedsensor MQ135(board, Voltage_Resolution, ADC_Bit_Resolution, pin, type);

uint32_t tsLastReport = 0;

struct State{
  bool readMode = false;
}state;

struct GasParam
{
  String nama[6] = {"CO", "Alkohol", "CO2", "Toluena", "NH4", "Acetone"};
  float setA[6] = {605.18, 77.255, 110.47, 44.947, 102.2, 34.668};
  float setB[6] = {-3.937, -3.18, -2.862, -3.445, -2.473, -3.369};
  float value[6];
  float ambangBatas[6] = {25.0, 100.0, 5000.0, 50.0, 25.0, 750.0};
  int blynkPin[6] = {V0, V1, V2, V3, V4, V5};
}gasParam;

struct Oxymeter{
  uint8_t spo2;
  uint8_t heartRate;
}oxy;
 
void onBeatDetected()
{
    Serial.println("Beat Detected!");
}

char auth[] = "";
char ssid[] = "";
char pass[] = "";

BLYNK_WRITE(V8){
  state.readMode = param.asInt();
  Serial.print("Current Mode: ");
  Serial.println(state.readMode);
  if(!state.readMode){
    
  }
  else{
    oxy.spo2 = 0;
    oxy.heartRate = 0;
    if (!pox.begin()){
      Serial.println("FAILED");
      for(;;);
    }
    else{
      Serial.println("SUCCESS");
    }
  }
}
 
void setup()
{
  Serial.begin(115200);
  
  pinMode(16, OUTPUT);
  Blynk.begin(auth, ssid, pass, "iot.serangkota.go.id", 8080);

  if(WiFi.status() == WL_CONNECTED){
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  }

  Serial.print("Initializing Pulse Oximeter..");

  MQ135.setRegressionMethod(1);
  MQ135.init();
  Serial.print("Calibrating MQ135, please wait.");
  float calcR0 = 0;
  for(int i = 1; i<=10; i ++)
  {
    MQ135.update(); // Update data, the arduino will be read the voltage on the analog pin
    calcR0 += MQ135.calibrate(RatioMQ135CleanAir);
    Serial.print(".");
  }
  MQ135.setR0(calcR0/10);
  Serial.println("  done!.");

  if (!pox.begin()){
    Serial.println("FAILED");
    for(;;);
  }
  else{
    Serial.println("SUCCESS");
    pox.setOnBeatDetectedCallback(onBeatDetected);
  }
}
 
void loop()
{
  if(state.readMode){
    pox.update();
  }
  else{
    MQ135.update();
  }
  Blynk.run();

  oxy.heartRate = pox.getHeartRate();
  oxy.spo2 = pox.getSpO2();
  if (millis() - tsLastReport > REPORTING_PERIOD_MS)
  {
    if(state.readMode){
      Serial.print("Heart rate:");
      Serial.print(oxy.heartRate);
      Serial.print(" bpm / SpO2:");
      Serial.print(oxy.spo2);
      Serial.println(" %");

      Blynk.virtualWrite(V6, oxy.spo2);
      Blynk.virtualWrite(V7, oxy.heartRate);
    }
    else{
      for(byte i = 0; i < 6; i++){
        MQ135.setA(gasParam.setA[i]);
        MQ135.setB(gasParam.setB[i]);
        gasParam.value[i] = MQ135.readSensor();
        if(i == 2){
          gasParam.value[i] += 400;
        }
      }
      Serial.println("");
      for(byte i = 0; i < 6; i++){
        Serial.print(gasParam.nama[i]);
        Serial.print(": ");
        Serial.println(gasParam.value[i]);
        Blynk.virtualWrite(gasParam.blynkPin[i], gasParam.value[i]);
      }
    }
    tsLastReport = millis();
  }
}