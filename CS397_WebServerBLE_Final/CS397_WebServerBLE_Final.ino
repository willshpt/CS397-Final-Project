/*
 * Will Phillips
 * Code is modified from multiple projects, the links to which can be found
 * below:
 * https://randomnerdtutorials.com/esp32-esp8266-plot-chart-web-server/
 * https://randomnerdtutorials.com/esp32-bluetooth-low-energy-ble-arduino-ide/
 * https://circuits4you.com/2018/11/20/web-server-on-esp32-how-to-update-and-display-sensor-values/
 * Project found at https://github.com/willshpt/CS397-Final-Project
 */


/*
 * ESP32 AJAX Demo
 * Updates and Gets data from webpage without page refresh
 * https://circuits4you.com
 */
/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*********/


// BLE Libraries

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// Import required libraries
  #include <WiFi.h>
  #include <WiFiClient.h>
  #include <WebServer.h>


#include "index.h"  //Web page header file

#define SCAN_DELAY 2000
#define TIMEOUT_TIME 30000
#define DATA_LIST_SET_DATA_SIZE 3

long lastMillis = 0;
// Define struct for packet data
struct packet_data {
    char dev_id[2];
    int16_t temp;
    char counter[2];
    long last_read;
};
struct packet_data data_list[DATA_LIST_SET_DATA_SIZE];
int next_index = 3;

// Finds device IDs from scanned packet data- mostly ready to add any possible ID
int find_device_ids(char high, char low){
    for (int i = 0; i < next_index; i++){
        if (high == data_list[i].dev_id[0] && low == data_list[i].dev_id[1]){
            return i;
        }
        //delay(1);
    }
    if(next_index < DATA_LIST_SET_DATA_SIZE){
        next_index++;
        return next_index - 1;
    }
    return DATA_LIST_SET_DATA_SIZE + 1;
}

// Compares counters in order to only get the newest data
bool compare_counter(char high, char low, int data_id){
    uint16_t stored_counter = (uint16_t)(data_list[data_id].counter[1]) + ((uint16_t)(data_list[data_id].counter[0]) << 8);
    uint16_t new_counter = (uint16_t)low + ((uint16_t)high << 8);
    return (new_counter - stored_counter > 0 || (new_counter < 100 && stored_counter > 65000));
}


//BLE
int scanTime = 5; //In seconds
BLEScan* pBLEScan;

// Scanning
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      //Serial.printf("Advertised Device: %s \n", advertisedDevice.toString().c_str());
    }
};


//Adafruit_BME280 bme(BME_CS); // hardware SPI
//Adafruit_BME280 bme(BME_CS, BME_MOSI, BME_MISO, BME_SCK); // software SPI

// Replace with your network credentials
const char* ssid = "insert_ssid_here";
const char* password = "insert_password_here";

// Create AsyncWebServer object on port 80
WebServer server(80);

// Whenever a get request is sent to root
void handleRoot() {
  if(millis() - lastMillis > SCAN_DELAY){
    ble_scan_procedure();
    lastMillis = millis();
  }
    //delay(1);
 String s = MAIN_page; //Read HTML contents
 server.send(200, "text/html", s); //Send web page
}

// Send temperature data
void handleTemp1() {
 float t = ((float)(data_list[0].temp)/100)*1.8 + 32;
 Serial.printf("%f \n", t);
 String temp1Value = String(t);
 
 server.send(200, "text/plane", temp1Value); //Send ADC value only to client ajax request
}
void handleTemp2() {
 float t = ((float)(data_list[1].temp)/100)*1.8 + 32;
 Serial.printf("%f \n", t);
 String temp2Value = String(t);
 
 server.send(200, "text/plane", temp2Value); //Send ADC value only to client ajax request
}
void handleTemp3() {
 float t = ((float)(data_list[2].temp)/100)*1.8 + 32;
 Serial.printf("%f \n", t);
 String temp3Value = String(t);
 
 server.send(200, "text/plane", temp3Value); //Send ADC value only to client ajax request
}

// Scans for devices and finds ones that are in the network, parses their advertisements, and stores their data in the appropriate struct
void ble_scan_procedure(){
  BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
  //Serial.print("Devices found: ");
  //Serial.println(foundDevices.getCount());
  int count = foundDevices.getCount();
  for (int i = 0; i < count; i++){
    delay(1);
    int16_t temp = 0;
    BLEAdvertisedDevice d = foundDevices.getDevice(i);
    if((uint8_t)(*(d.getAddress().getNative()))[0] == 0xc0 && (uint8_t)(*(d.getAddress().getNative()))[1] == 0x98 && (uint8_t)(*(d.getAddress().getNative()))[2] == 0xe5 && (uint8_t)(*(d.getAddress().getNative()))[3] == 0x4f){
      Serial.printf(d.getAddress().toString().c_str());
      Serial.printf("\n");
      if (d.haveManufacturerData()){
        int q = 0;
        std::string md = d.getManufacturerData();
        uint8_t *mdp = (uint8_t *)d.getManufacturerData().data();
        // First one
        q = find_device_ids(mdp[2], mdp[3]);
        if ((q != 4 && compare_counter(mdp[6], mdp[7], q)) || ((uint8_t)(*(d.getAddress().getNative()))[4] == data_list[q].dev_id[0] && (uint8_t)(*(d.getAddress().getNative()))[5] == data_list[q].dev_id[1])){
          data_list[q].dev_id[0] = mdp[2];
          data_list[q].dev_id[1] = mdp[3];
          data_list[q].temp = ((int)mdp[4] << 8) + mdp[5];
          data_list[q].counter[0] = mdp[6];
          data_list[q].counter[1] = mdp[7];
        }
        // Second one
        q = find_device_ids(mdp[8], mdp[9]);
        if ((q != 4 && compare_counter(mdp[12], mdp[13], q)) || ((uint8_t)(*(d.getAddress().getNative()))[4] == data_list[q].dev_id[0] && (uint8_t)(*(d.getAddress().getNative()))[5] == data_list[q].dev_id[1])){
          data_list[q].dev_id[0] = mdp[8];
          data_list[q].dev_id[1] = mdp[9];
          data_list[q].temp = ((int)mdp[10] << 8) + mdp[11];
          data_list[q].counter[0] = mdp[12];
          data_list[q].counter[1] = mdp[13];
        }
        // Third one
        q = find_device_ids(mdp[14], mdp[15]);
        if ((q != 4 && compare_counter(mdp[18], mdp[19], q)) || ((uint8_t)(*(d.getAddress().getNative()))[4] == data_list[q].dev_id[0] && (uint8_t)(*(d.getAddress().getNative()))[5] == data_list[q].dev_id[1])){
          data_list[q].dev_id[0] = mdp[14];
          data_list[q].dev_id[1] = mdp[15];
          data_list[q].temp = ((int)mdp[16] << 8) + mdp[17];
          data_list[q].counter[0] = mdp[18];
          data_list[q].counter[1] = mdp[19];
        }
      }
    }
  }
  Serial.println("Scan done!");
  pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory
}

void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);
  //BLE Setup:
  BLEDevice::init("");

  // put your main code here, to run repeatedly:
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);

  // Currently only checks for these IDs, but code from the nRF52840 program can be
  // swapped in to add new nodes to a list, and the index file can be modified to handle
  // more nodes if need be.
  data_list[0].dev_id[0] = 0xaa;
  data_list[0].dev_id[1] = 0xbb;
  data_list[1].dev_id[0] = 0xaa;
  data_list[1].dev_id[1] = 0xbc;
  data_list[2].dev_id[0] = 0xaa;
  data_list[2].dev_id[1] = 0xbd;

// Uncomment the following and comment out the regular mode in order to use the ESP32
// as its own access point
/*
 //ESP32 As access point
  WiFi.mode(WIFI_AP); //Access Point mode
  WiFi.softAP(ssid, password);
*/
//ESP32 connects to your wifi -----------------------------------
  WiFi.mode(WIFI_STA); //Connectto your wifi
  WiFi.begin(ssid, password);

  Serial.println("Connecting to ");
  Serial.print(ssid);

  //Wait for WiFi to connect
  while(WiFi.waitForConnectResult() != WL_CONNECTED){      
      Serial.print(".");
    }
    
  //If connection successful show IP address in serial monitor
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());  //IP address assigned to your ESP

  // Route for root / web page
  server.on("/", handleRoot);      //This is display page
  server.on("/temperature1", handleTemp1);//To get update of ADC Value only
  server.on("/temperature2", handleTemp2);
  server.on("/temperature3", handleTemp3);

  // Start server
  server.begin();
}
 
void loop(){
  server.handleClient();
  delay(1); // Deal with WDT
  if(millis() - lastMillis > SCAN_DELAY + SCAN_DELAY){ // Wait twice as long if no get- just to have something in there
    ble_scan_procedure();
    lastMillis = millis();
  }
}
