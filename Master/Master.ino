#include <esp_now.h>
#include <WiFi.h>

// Global copy of slave
#define NUMSLAVES 8
esp_now_peer_info_t slavesL[NUMSLAVES] = {};
int SlaveCntL = 0;
esp_now_peer_info_t slavesR[NUMSLAVES] = {};
int SlaveCntR = 0;

#define CHANNEL 0
#define PRINTSCANRESULTS 0

int data = 0;
// variables will change:
int buttonState = 0;         // variable for reading the pushbutton status
const int LaserPin = 18;     // the number of the pushbutton pin
const int LaserPin_1 = 19;     // the number of the pushbutton pin
const int MotionSensorPin = 21;     // the number of the pushbutton pin
const int ChaosPin = 22;     // the number of the chaos pin

long timestamp = 0;
const long ignoreMotionTime = 5*60*1000; // 1000 Anzahl von millisekunden in einer Sekunde, 60 Sekunden in einer Minute, 5 Anzahl an Minuten zu warten
int besucherzahler = 0;
boolean chaosmode = false;
#define kriegslied 5                  // Nummer des kriegslied
#define halloFile 10                  // Datei, die bei Bewegung gespielt werden soll
#define byFiles 4                     // Anzahl der Datein zur verabschiedung (Kommen nach dem Hallo File)
#define normalFiles 10                // Anzahl NormaleDatein, exlusive diese Zahl
#define debounceValue 1000

// Init ESP Now with fallback
void InitESPNow() {
  WiFi.disconnect();
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow Init Success");
  }
  else {
    Serial.println("ESPNow Init Failed");
    ESP.restart();
  }
}

// Scan for slaves in AP mode
void ScanForSlave() {
  int8_t scanResults = WiFi.scanNetworks();
  //reset slaves
  memset(slavesL, 0, sizeof(slavesL));
  SlaveCntL = 0;
  memset(slavesR, 0, sizeof(slavesR));
  SlaveCntR = 0;
  Serial.println("");
  if (scanResults == 0) {
    Serial.println("No WiFi devices in AP Mode found");
  } else {
    Serial.print("Found "); Serial.print(scanResults); Serial.println(" devices ");
    for (int i = 0; i < scanResults; ++i) {
      // Print SSID and RSSI for each device found
      String SSID = WiFi.SSID(i);
      int32_t RSSI = WiFi.RSSI(i);
      String BSSIDstr = WiFi.BSSIDstr(i);

      if (PRINTSCANRESULTS) {
        Serial.print(i + 1); Serial.print(": "); Serial.print(SSID); Serial.print(" ["); Serial.print(BSSIDstr); Serial.print("]"); Serial.print(" ("); Serial.print(RSSI); Serial.print(")"); Serial.println("");
      }
      delay(10);
      // Check if the current device starts with `Slave`
      if (SSID.indexOf("Left") == 0) {
        Serial.print(i + 1); Serial.print(": "); Serial.print(SSID); Serial.print(" ["); Serial.print(BSSIDstr); Serial.print("]"); Serial.print(" ("); Serial.print(RSSI); Serial.print(")"); Serial.println("");
        int mac[6];
        if ( 6 == sscanf(BSSIDstr.c_str(), "%x:%x:%x:%x:%x:%x",  &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5] ) ) {
          for (int ii = 0; ii < 6; ++ii ) {
            slavesL[SlaveCntL].peer_addr[ii] = (uint8_t) mac[ii];
          }
        }
        slavesL[SlaveCntL].channel = CHANNEL; // pick a channel
        slavesL[SlaveCntL].encrypt = 0; // no encryption
        SlaveCntL++;
      } else if (SSID.indexOf("Right") == 0) {
        Serial.print(i + 1); Serial.print(": "); Serial.print(SSID); Serial.print(" ["); Serial.print(BSSIDstr); Serial.print("]"); Serial.print(" ("); Serial.print(RSSI); Serial.print(")"); Serial.println("");
        int mac[6];
        if ( 6 == sscanf(BSSIDstr.c_str(), "%x:%x:%x:%x:%x:%x",  &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5] ) ) {
          for (int ii = 0; ii < 6; ++ii ) {
            slavesR[SlaveCntR].peer_addr[ii] = (uint8_t) mac[ii];
          }
        }
        slavesR[SlaveCntR].channel = CHANNEL; // pick a channel
        slavesR[SlaveCntR].encrypt = 0; // no encryption
        SlaveCntR++;
      }
    }
  }

  if (SlaveCntL > 0 || SlaveCntR > 0 ) {
    Serial.print(SlaveCntL); Serial.println(" Left Slave(s) found, processing..");Serial.print(SlaveCntR); Serial.println(" Right Slave(s) found, processing..");
  } else {
    Serial.println("No Slave Found, trying again.");
  }

  // clean up ram
  WiFi.scanDelete();
}

void printErrorESP(esp_err_t result) {
  Serial.print("Send Status: ");
  if (result == ESP_OK) {
      Serial.println("Success");
    } else if (result == ESP_ERR_ESPNOW_NOT_INIT) {
      // How did we get so far!!
      Serial.println("ESPNOW not Init.");
    } else if (result == ESP_ERR_ESPNOW_ARG) {
      Serial.println("Invalid Argument");
    } else if (result == ESP_ERR_ESPNOW_INTERNAL) {
      Serial.println("Internal Error");
    } else if (result == ESP_ERR_ESPNOW_NO_MEM) {
      Serial.println("ESP_ERR_ESPNOW_NO_MEM");
    } else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
      Serial.println("Peer not found.");
    } else if (result == ESP_ERR_ESPNOW_FULL) {
      Serial.println("Peer list full");
    } else if (result == ESP_ERR_ESPNOW_NO_MEM) {
      Serial.println("Out of memory");
    } else if (result == ESP_ERR_ESPNOW_EXIST) {
    Serial.println("Peer Exists");
    } else {
    Serial.println("Not sure what happened");
    }
}

// Check if the slave is already paired with the master.
// If not, pair the slave with master
void manageSlave() {
  if (SlaveCntL > 0) {
    for (int i = 0; i < SlaveCntL; i++) {
      Serial.print("Processing: ");
      for (int ii = 0; ii < 6; ++ii ) {
        Serial.print((uint8_t) slavesL[i].peer_addr[ii], HEX);
        if (ii != 5) Serial.print(":");
      }
      Serial.print(" Status: ");
      // check if the peer exists
      bool exists = esp_now_is_peer_exist(slavesL[i].peer_addr);
      if (exists) {
        // Slave already paired.
        Serial.println("Already Paired");
      } else {
        // Slave not paired, attempt pair
        esp_err_t result = esp_now_add_peer(&slavesL[i]);
        printErrorESP(result);
      }
    }
  }
  if (SlaveCntR > 0) {
    for (int i = 0; i < SlaveCntR; i++) {
      Serial.print("Processing: ");
      for (int ii = 0; ii < 6; ++ii ) {
        Serial.print((uint8_t) slavesR[i].peer_addr[ii], HEX);
        if (ii != 5) Serial.print(":");
      }
      Serial.print(" Status: ");
      // check if the peer exists
      bool exists = esp_now_is_peer_exist(slavesR[i].peer_addr);
      if (exists) {
        // Slave already paired.
        Serial.println("Already Paired");
      } else {
        // Slave not paired, attempt pair
        esp_err_t result = esp_now_add_peer(&slavesR[i]);
        printErrorESP(result);
      }
    }
  } else {
    // No slave found to process
    Serial.println("No Slave found to process");
  }
}


void brodcast(){
  for (int i = 0; i < SlaveCntL; i++) {
    const uint8_t *peer_addr = slavesL[i].peer_addr;
    if (i == 0) { // print only for first slave
      Serial.print("Sending: ");
      Serial.println(data);
    }
    esp_err_t result = esp_now_send(peer_addr, (uint8_t *) &data, sizeof(data));
    printErrorESP(result);
  }
  for (int i = 0; i < SlaveCntR; i++) {
    const uint8_t *peer_addr = slavesR[i].peer_addr;
    if (i == 0) { // print only for first slave
      Serial.print("Sending: ");
      Serial.println(data);
    }
    esp_err_t result = esp_now_send(peer_addr, (uint8_t *) &data, sizeof(data));
    printErrorESP(result);
  }
}

// send data
void sendData() {
  if (data%2){
    int i = random(0,SlaveCntL);
    const uint8_t *peer_addr = slavesL[i].peer_addr;
    Serial.println(slavesL[i].peer_addr[5]);
    Serial.print("Sending: ");
    Serial.println(data);
    esp_err_t result = esp_now_send(peer_addr, (uint8_t *) &data, sizeof(data));
    printErrorESP(result);
  }else {
    int i = random(0,SlaveCntR);
    const uint8_t *peer_addr = slavesR[i].peer_addr;
    Serial.println(slavesR[i].peer_addr[5]);
    Serial.print("Sending: ");
    Serial.println(data);
    esp_err_t result = esp_now_send(peer_addr, (uint8_t *) &data, sizeof(data));
    printErrorESP(result);
  }
}

void setup() {
  pinMode(LaserPin, INPUT_PULLUP);
  pinMode(LaserPin_1, INPUT_PULLUP);
  pinMode(MotionSensorPin, INPUT);
  pinMode(ChaosPin, INPUT_PULLUP);
  
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  Serial.println("ESPNow/Multi-Slave/Master Example");
  Serial.print("STA MAC: "); Serial.println(WiFi.macAddress());
  InitESPNow();
  ScanForSlave();
}

void checkLaser() {
  // Lichtschranke durchbrochen
  buttonState = digitalRead(LaserPin);
  if (buttonState == HIGH){
  } else {
    if (SlaveCntL > 0 || SlaveCntR > 0 ) {
      manageSlave();
      // Stop
      if (!chaosmode){
        data = 0;
        brodcast();
      }
      // Send new Song
      data = random(1, normalFiles);
      if (kriegslied == data) {
        brodcast();
      }else {
        sendData();
      }
      buttonState = digitalRead(LaserPin);
      while (buttonState == LOW) {
        delay(10);
        buttonState = digitalRead(LaserPin);
      }
      // debounce
      delay(debounceValue);
    }
    timestamp = millis();
  }
}

void loop() {
  buttonState = digitalRead(ChaosPin);
  chaosmode = buttonState;
  checkLaser();
  // Check EingangAusgang
  buttonState = digitalRead(LaserPin_1);
  if (buttonState == HIGH){
  } else {
    besucherzahler++;
    if (SlaveCntL > 0 || SlaveCntR > 0 ) {
      manageSlave();
      if (!chaosmode){
        data = 0;
        brodcast();
      }
      if (besucherzahler%2) {
        data = random(halloFile + 1, halloFile + 1 + byFiles);
      } else {
        data = random(1, normalFiles);
      }
      if (kriegslied == data) {
        brodcast();
      }else {
        sendData();
      }
      buttonState = digitalRead(LaserPin_1);
      while (buttonState == LOW) {
        delay(10);
        buttonState = digitalRead(LaserPin_1);
      }
      // debounce
      delay(debounceValue);
    }
    timestamp = millis();
  }
  if(digitalRead(MotionSensorPin)==HIGH && ignoreMotionTime + timestamp < millis()) {
    timestamp = millis();
    Serial.println("Movement detected.");
    data = halloFile;
    sendData();
  }
}
