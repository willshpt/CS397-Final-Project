# CS397-Final-Project
 Final project for CS397

 In order to use this code, you must also have the nu-wireless-iot-base, one or more nRF52840s, a TMP36 temperature sensor for each nRF52840, and one or more ESP32 modules.
 
 The Temperature_Network_Project must be uploaded to each nRF52840, making sure to modify THIS_DEVICE_ID for each one so there are no repeats.
 The TMP36s are wired up to each nRF52840 with gnd to gnd, vin to vdd, and vout to AIN7 (or P0.31).
 The CS397_WebServerBLE_Final Arduino sketch must be uploaded to an ESP32. This may require changing the partition scheme to Huge APP in order to fit everything, as the BLE library is too large to fit normally.
