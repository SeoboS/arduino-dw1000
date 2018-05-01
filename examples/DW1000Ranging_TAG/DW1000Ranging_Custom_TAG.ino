/**
 * 
 * @todo
 *  - move strings to flash (less RAM consumption)
 *  - fix deprecated convertation form string to char* startAsTag
 *  - give example description
 */
#include <SPI.h>
#include "DW1000Ranging.h"
#include <SoftwareSerial.h>
SoftwareSerial BTSerial(7, 8); // RX | TX

// connection pins
const uint8_t PIN_RST = 9; // reset pin
const uint8_t PIN_IRQ = 2; // irq pin
const uint8_t PIN_SS = SS; // spi select pin
const uint8_t BLU_EN = 6  ;
const uint8_t RANGE_INC = 3  ;
const uint8_t SEO_DEBUG = 0;
const uint8_t NUM_ANCHORS = 3;
const uint8_t NEED_ALL_CONNECTIONS = 1;
// 0: keeps old distances of disconnected devices
// 1: only display distances from currently active anchors
const uint8_t MODE = 2;
// 0: default serial mode. 
// 1: bluetooth mode
// 2: display on both mode
// 3: AT Mode

int rangeLedCount = 0;
int updateCount = 0;
String a1 = "4369", a2 = "8738", a3 = "13107"; // placeholder, need to be replaced with chip addresses
int d1 = -1, d2 = -1, d3 = -1;
int d1_prev = -1, d2_prev = -1, d3_prev = -1;
boolean a1connected = 0, a2connected = 0, a3connected = 0;
int s1 = 0, s2 = 0, s3 = 0;

char myChar;


void setup() {
  //Serial.begin(115200);
  Serial.begin(9600);
  if (MODE ==3){
    BTSerial.begin(38400);
  }
  if (MODE ==2){
    BTSerial.begin(9600);
  }
  delay(1000);
  pinMode(BLU_EN,OUTPUT);
  digitalWrite(BLU_EN,HIGH); // bluetooth enable

  pinMode(RANGE_INC,OUTPUT);
  digitalWrite(RANGE_INC,LOW);
  //init the configuration
  DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ); //Reset, CS, IRQ pin
  DW1000Ranging.attachNewRange(newRange);
  DW1000Ranging.attachNewDevice(newDevice);
  DW1000Ranging.attachInactiveDevice(inactiveDevice);
  //Enable the filter to smooth the distance
  DW1000Ranging.useRangeFilter(true);
  
  //we start the module as a tag
  DW1000Ranging.startAsTag("00:00:00:00:00:00:00:00", DW1000.MODE_LONGDATA_RANGE_ACCURACY,false);
}
 
void loop() {
  if (MODE == 3 ){
     while (BTSerial.available()) {
      myChar = BTSerial.read();
      Serial.print(myChar);
    }
  
    while (Serial.available()) {
      myChar = Serial.read();
      Serial.print(myChar); //echo
      BTSerial.print(myChar);
    }
  }
  else{
    digitalWrite(RANGE_INC,LOW);
    if(BTSerial.available()>0){
      while(BTSerial.available()>0){
        byte t = BTSerial.read();
        //Serial.print((char)t);
      }
      Serial.println(" ");
      //msgPacket("Bluetooth connection started");
    }
    DW1000Ranging.loop();
  }
}

//* OVERLOADED DW FUNCTION *//

void newRange() {
  String addr;
  if (rangeLedCount > 10){
    rangeLedCount = 0;
    digitalWrite(RANGE_INC,HIGH);
    //deelay(500);
  }
  rangeLedCount++;
  addr = (String)DW1000Ranging.getDistantDevice()->getShortAddress();
  float dist = DW1000Ranging.getDistantDevice()->getRange(); // float or double
  float rxPower = DW1000Ranging.getDistantDevice()->getRXPower();
  if (dist > 0.0){
    if (addr.equals(a1)){
      d1_prev = d1;
      d1 = (int) (dist*100.0);
      s1 = 1;
    }
    else if (addr.equals(a2)){
      d2_prev = d2;
      d2 = (int) (dist*100.0);
      s2 = 1;
    }
    else if (addr.equals(a3)){
      d3_prev = d3;
      d3 = (int) (dist*100.0);
      s3 = 1;
    }
    //TODO: we should really do the data processng here to minimize bluetooth communication and save memory.
    sendDataWhenUpdated();
    //sendDataInBTPacket();
    //displayDataForMatlab(NUM_ANCHORS);
  }
  if (SEO_DEBUG == 1){ // something about this function really messes up the range received
    printInfo(addr, (int)(dist*100.0),rxPower);
  }
}

void newDevice(DW1000Device* device) {
  char * temp = malloc(sizeof(char)*100);
  sprintf(temp, "ranging init; 1 device added ! ->  short: %#x", device->getShortAddress());
  String stemp = String(temp);
  msgPacket(stemp);
  free(temp);
  if (device->getShortAddress() == a1.toInt()){
    a1connected = true;
  }
  else if (device->getShortAddress() == a2.toInt()){
    a2connected = true;
  }
  else if (device->getShortAddress() == a3.toInt()){
    a3connected = true;
  }
}

void inactiveDevice(DW1000Device* device) {
  char * temp = malloc(sizeof(char)*50);
  sprintf(temp, "delete inactive device: %#x", device->getShortAddress());
  String stemp = String(temp);
  msgPacket(stemp);
  free(temp);
  if (device->getShortAddress() == a1.toInt()){
    a1connected = false;
  }
  else if (device->getShortAddress() == a2.toInt()){
    a2connected = false;
  }
  else if (device->getShortAddress() == a3.toInt()){
    a3connected = false;
  }
}


//* HELPER FUNCTION *//

void printInfo(String addr, int distance, float rxPower){
  char * temp = malloc(sizeof(char)*130);
  sprintf(temp, "[ from: %s, Range: %d m, RX power: ", addr.c_str(),distance);
  msg(temp);
  msg(rxPower);
  msg(" dBm]\n");
  free(temp);
}

void sendDataWhenUpdated(){
  if ( (d1_prev != d1) || (d2_prev != d2) || (d3_prev != d3) ){
   msg("[");
   // theoretically only one range should update at a time so this should work
    if (a1connected && (d1_prev != d1) && (d1 > 0) && (d1_prev > 0) ){  
      ++updateCount;
      if (abs(d1-d1_prev) > 1000 ){// has a habit of overestimating distance  // but could get stuck when travelling large distances quickly
        d1 = min(d1,d1_prev);
      }
      char * temp = malloc(sizeof(char)*40);
      sprintf(temp, "%s: %d, ", a1.c_str(),d1);
      //msg(d1_prev);
      //msg(", ");
      msg(String(temp));
      free(temp);
    }
    else if (a2connected && (d2_prev != d2) && (d2 > 0) && (d2_prev > 0)){
      ++updateCount;
      if (abs(d2-d2_prev) > 1000 ){
        d2 = min(d2,d2_prev);
      }
      char * temp = malloc(sizeof(char)*40);
      sprintf(temp, "%s: %d, ", a2.c_str(),d2);
      //msg(d2_prev);
      //msg(", ");
      msg(String(temp));
      free(temp);
    }
    else if (a3connected && (d3_prev != d3) && (d3 > 0) && (d3_prev > 0)){
      ++updateCount;
      if (abs(d3-d3_prev) > 1000 ){
        d3 = min(d3,d3_prev);
      }
      char * temp = malloc(sizeof(char)*40);
      sprintf(temp, "%s: %d, ", a3.c_str(),d3);
      //msg(d3_prev);
      //msg(", ");
      msg(String(temp));
      free(temp);
    }
    else{
      
    }
    msg("]\n");
    //flushBuffer();
  }
  if (updateCount > 50){
    updateCount = 0;
    sendDataInBTPacket();
  }
}

// This format is particular to how the data is received on the end of the android app
void sendDataInBTPacket(){
  if (NEED_ALL_CONNECTIONS == 0){ // idt this part works
    msg("[");
    if (d1 >-1){
      char * temp = malloc(sizeof(char)*40);
      sprintf(temp, "%s: %d, ", a1.c_str(),d1);
      msg(String(temp));
      free(temp);
      //Serial.print(a1); Serial.print(": "); Serial.print(d1); Serial.print(", ");
    }
    if (d2 >-1){
      char * temp = malloc(sizeof(char)*40);
      sprintf(temp, "%s: %d, ", a2.c_str() ,d2);
      msg(String(temp));
      free(temp);
    }
    if (d3 >-1){      
      char * temp = malloc(sizeof(char)*40);
      sprintf(temp, "%s: %d", a3.c_str(),d3);
      msg(String(temp));
      free(temp);
    }
    msg("]\n");
  }
  else{
    msg("[");
    if (a1connected){
      char * temp = malloc(sizeof(char)*40);
      sprintf(temp, "%s: %d, ", a1.c_str(),d1);
      msg(String(temp));
      free(temp);
      //Serial.print(a1); Serial.print(": "); Serial.print(d1); Serial.print(", ");
    }
    if (a2connected){
      char * temp = malloc(sizeof(char)*40);
      sprintf(temp, "%s: %d, ", a2.c_str(),d2);
      msg(String(temp));
      free(temp);
    }
    if (a3connected){
      char * temp = malloc(sizeof(char)*40);
      sprintf(temp, "%s: %d", a3.c_str(),d3);
      msg(String(temp));
      free(temp);
    }
    msg("]\n");
  }
}
void displayDataForMatlab(int numAnchors){
    if (s1 == 1 && numAnchors>0){
      msg(d1); s1 = 0;
      numAnchors -= 1;
      if (numAnchors > 0){
         msg(',');
      }
    }
    else if (s2 == 1 && numAnchors>0 ){
      msg(d2); s2 = 0;
      numAnchors -= 1;
      if (numAnchors > 0){
         msg(',');
      }
    }
    else if (s3 == 1 && numAnchors>0){
      msg(d3); s3 = 0;
      numAnchors -= 1;
    }
  msg('\n');
}

void msgPacket(String str){
    Serial.print("[");
    Serial.print(str);
    Serial.println("]");
  if (MODE ==2){
    BTSerial.print("[");
    BTSerial.print(str);
    BTSerial.println("]");
  }
}
void msg(String str){
    Serial.print(str);
  if (MODE ==2){
    BTSerial.print(str);
  }  
}
void msg(char str){
    Serial.print(str);
  if (MODE ==2){
    BTSerial.print(str);
  }    
}
void msg(int str){
  Serial.print(str);
  if (MODE ==2){
    BTSerial.print(str);
  }    
}
void msg(float str){
  Serial.print(str);
  if (MODE ==2){
    BTSerial.print(str);
  }
}
void flushBuffer(){
  Serial.flush();
  if (MODE==2){
    BTSerial.flush();
  }
}


