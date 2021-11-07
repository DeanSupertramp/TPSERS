#include <SoftwareSerial.h>                                                         //Includo libreria SoftwareSerial per la connessione con HC-05
#include <Wire.h>                                                                   //Includo libreria Wire per la connessione i2c con mag
#include <LIS3MDL.h>                                                                //Includo libreria del mag

//definisco pin RX e TX da Arduino verso modulo BT
#define     BT_FROM_PIN         12    //D11
#define     BT_TO_PIN           11    //D12

bool magSimulation = false;
bool simulationState = 1;

LIS3MDL mag;                                                                        //istanzio mag
SoftwareSerial bt(BT_FROM_PIN, BT_TO_PIN);                          //istanzio oggetto SoftwareSerial

char report[80];                                                                    //definisco variabile per i dati ottenuti dal mag
bool state;

void setupMag() {
  Wire.begin();
    //controllo connessione mag
    if (!magSimulation && mag.init()){
      mag.enableDefault();
    }else{
      magSimulation = true;
      Serial.println("Failed to initialize magnetometer. Simulation is enabled.");
    }
}

void setupBT() {
  const unsigned long BT_INIT_TIME = 5000;
  unsigned long startTime = millis();
  bt.begin(9600);
}

void setup() {
  //definisco pin bt

  Serial.begin(9600);                                                                    //inizializzo comunicazione Bluetooth

  setupMag();
  setupBT();

  Serial.println("Program started");
}

void btInputFlush(){
  while(bt.available())
    bt.read();
}

uint8_t* btRead(uint8_t* header, uint8_t* len, uint16_t commTimeout) {
  uint8_t packet[255];
  uint8_t checksum, checksumCheck = 0;
  *header = 0;
  *len = 0;

  unsigned long startTime = millis();

  if (bt.available() == 0)
    return NULL;

  *header = bt.read();
  checksumCheck = *header;
  
  while (bt.available() == 0 && (millis() - startTime < commTimeout));
  
  if (bt.available() == 0)
    return NULL;

  *len = bt.read();
  checksumCheck += *len;

  int i = 0;
  while (i < *len + 1 && (millis() - startTime < commTimeout)) {
    if (bt.available() > 0) {
      if(i < *len){
        packet[i] = (uint8_t) bt.read();
        checksumCheck += packet[i];
      } else {
        checksum = (uint8_t) bt.read();
      }
      i++;
    }
  }
  packet[i-1] = 0;


  snprintf(report, sizeof(report), "checksum, checksumCheck: %d, %d", checksum, checksumCheck);
  Serial.println(report);
  
  if(checksum != checksumCheck){
    btInputFlush();
    return NULL;
  }
  
  snprintf(report, sizeof(report), "BT Header: %d", *header);
  Serial.println(report);
  
  snprintf(report, sizeof(report), "BT Length: %d", *len);
  Serial.println(report);
  
  Serial.print("BT packet: <");
  Serial.print((char*) packet);
  Serial.println(">");
  
  return i == *len+1 ? packet : NULL;
}

void btWrite(uint8_t header, uint8_t len, uint8_t* data) {
  uint8_t checksum = header + len;
  bt.write(header);
  bt.write(len);
  for (int i=0; i < len; i++) {
    bt.write(data[i]);
    checksum += data[i];
  }
  bt.write(checksum);
}



uint16_t magTimeout = 100;
unsigned long magTime = 0;

struct MagData{
  int16_t x, y, z;
} magData;

bool readMag() {
  if ((uint16_t)(millis() - magTime) >= magTimeout) {
    magTime = millis();

    if (magSimulation) {
      if(simulationState){
        magData.y = magData.y + 150;
      }else{
        magData.y = magData.y - 150;
      }
      if(magData.y > 3421){magData.y = magData.y-150; simulationState = 0;}
      if(magData.y < 0) {magData.y = magData.y+150; simulationState = 1;}
      
      magData.x = 3421 - magData.y;
      magData.z = 0;
    }
    else {
      mag.read();

      magData.x = (int16_t) mag.m.x;
      magData.y = (int16_t) mag.m.y;
      magData.z = (int16_t) mag.m.z;
    }

    btWrite(200, 6, (uint8_t * ) &magData);
    
    return true;
  }
  return false;
}

const unsigned long ACK_OUT_TIMEOUT = 2000;
unsigned long lastAckOutTime = 0;

char discovery_command[23] = "ARDUINO_NODE_DISCOVERY";

unsigned long state_timeout = 10000;
unsigned long state_time;

void loop() {
  uint8_t header, len;
  uint8_t* packet;

  bool discoveryVerified;

  packet = btRead(&header, &len, 800);

  switch (state) {

    case 1:

      if(packet != NULL){
        switch (header) {
          case 200:   //timeout & ACK online
            state_time = millis();
            magTimeout = (uint16_t) packet[0] * 10;
            snprintf(report, sizeof(report), "Android is online. Magnetometer timeout: %d", magTimeout);
            Serial.println(report);
            break;
  
          default: btInputFlush();
        }
      }

      // legge i dati dal magnetometro e li invia al bluetooth
      readMag();

      if (millis() - state_time > state_timeout) {
        state = 0;
        Serial.println("Disconnected");
        btInputFlush();
      }
      break;

    case 0:
      //packet = btRead(&header, &len, 5000);
      
      discoveryVerified = false;
      if(packet != NULL){
        if(header == 150){
          discoveryVerified = true;
          for(int i=0; i<22; i++){
            if(packet[i] != (uint8_t)discovery_command[i]){
              discoveryVerified = false;
              break;
            }
          }
        }//header
        
      }//packet
      
      if(discoveryVerified){
        
        Serial.println("Connected");

        state = 1;
        state_time = millis();
      }
      break;

  }
}




