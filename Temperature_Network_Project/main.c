// Temperature Network Project App
//
// Creates a BLE Advertisement Network to share Temperature Data

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// For ADC
#include "nrf.h"
#include "nrfx_saadc.h"
#include "nordic_common.h"
#include "boards.h"
#include "app_error.h"
#include "nrf_delay.h"
#include "app_util_platform.h"

// For timer
#include "nrf_drv_timer.h"
#include "bsp.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "simple_ble.h"

#include "nrf52840dk.h"

#define INPUT_CHANNEL 0

#define TIME_OUT_LIMIT 400 // Limit for timeout
#define DATA_LIST_SET_DATA_SIZE 16 // How many total nodes in the network
#define THIS_DEVICE_ID 0xAABB // Unique device ID- must change for each new device. NOTE: CANNOT BE 0, as that is reserved for node replacement
// Struct for advertised packet data
struct packet_data {
    char dev_id[2];
    char temp[2];
    char counter[2];
    char timeout_counter;
};
struct packet_data data_list[DATA_LIST_SET_DATA_SIZE];

int next_index = 1; // Next index to fill
int timeout_counter = 0; // Counts until timeout
bool new_data = false;

// callback for SAADC events
void saadc_callback (nrfx_saadc_evt_t const * p_event) {
  // don't care about adc callbacks
}

// sample a particular analog channel in blocking mode
nrf_saadc_value_t sample_value (uint8_t channel) {
  nrf_saadc_value_t val;
  ret_code_t error_code = nrfx_saadc_sample_convert(channel, &val);
  APP_ERROR_CHECK(error_code);
  return val;
}


// Intervals for advertising and connections
static simple_ble_config_t ble_config = {
        // c0:98:e5:4f:xx:xx
        .platform_id       = 0x4F,   // used as 4th octect in device BLE address
        .device_id         = THIS_DEVICE_ID, // must be unique on each device you program!
        .adv_name          = "CS397/497", // used in advertisements if there is room
        .adv_interval      = MSEC_TO_UNITS(1000, UNIT_0_625_MS),
        .min_conn_interval = MSEC_TO_UNITS(500, UNIT_1_25_MS),
        .max_conn_interval = MSEC_TO_UNITS(1000, UNIT_1_25_MS),
};

/*******************************************************************************
 *   State for this application
 ******************************************************************************/
// Main application state
simple_ble_app_t* simple_ble_app;




int find_device_ids(char high, char low){
    for (int i = 0; i < next_index; i++){
        if ((high == data_list[i].dev_id[0] && low == data_list[i].dev_id[1]) || (0 == data_list[i].dev_id[0] && 0 == data_list[i].dev_id[1])){
            return i; // Either finds itself or the next open device ID, therefore 
        }
    }
    if(next_index < DATA_LIST_SET_DATA_SIZE){
        next_index++;
        return next_index - 1;
    }
    return 0;
}

bool compare_counter(char high, char low, int data_id){
    uint16_t stored_counter = (uint16_t)(data_list[data_id].counter[1]) + ((uint16_t)(data_list[data_id].counter[0]) << 8);
    uint16_t new_counter = (uint16_t)low + ((uint16_t)high << 8);
    return (new_counter - stored_counter > 0 || (new_counter < 500 && stored_counter > 65000));
}

// Callback handler for advertisement reception
void ble_evt_adv_report(ble_evt_t const* p_ble_evt) {

  // extract the fields we care about
  ble_gap_evt_adv_report_t const* adv_report = &(p_ble_evt->evt.gap_evt.params.adv_report);
  uint8_t const* ble_addr = adv_report->peer_addr.addr; // array of 6 bytes of the address
  uint8_t* adv_buf = adv_report->data.p_data; // array of up to 31 bytes of advertisement payload data
  uint16_t adv_len = adv_report->data.len; // length of advertisement payload data

  // Here is where the node searches through scanned advertisements to find ones in the network,
  // and then parses those nodes to find the data of 
  if(ble_addr[5] == 0xc0 && ble_addr[4] == 0x98 && ble_addr[3] == 0xe5 && ble_addr[2] == 0x4F){
    printf("%x %x %x %x %x %x\n", ble_addr[5], ble_addr[4], ble_addr[3], ble_addr[2], ble_addr[1], ble_addr[0]);
    int i = 0;
    if (adv_len >= 10) {// One packet
      i = find_device_ids(adv_buf[4], adv_buf[5]);
      if ((i != 0 && compare_counter(adv_buf[8], adv_buf[9], i)) || (adv_buf[4] == ble_addr[1] && adv_buf[5] == ble_addr[0])){ // If it is the data from sender, overwrite
        data_list[i].dev_id[0] = adv_buf[4];
        data_list[i].dev_id[1] = adv_buf[5];
        data_list[i].temp[0] = adv_buf[6];
        data_list[i].temp[1] = adv_buf[7];
        data_list[i].counter[0] = adv_buf[8];
        data_list[i].counter[1] = adv_buf[9];
        data_list[i].timeout_counter = 0;
        new_data = true;
      }
    }
    if (adv_len >= 16) { // Two packets
      i = find_device_ids(adv_buf[10], adv_buf[11]);
      if (i != 0 && compare_counter(adv_buf[14], adv_buf[15], i)){
        data_list[i].dev_id[0] = adv_buf[10];
        data_list[i].dev_id[1] = adv_buf[11];
        data_list[i].temp[0] = adv_buf[12];
        data_list[i].temp[1] = adv_buf[13];
        data_list[i].counter[0] = adv_buf[14];
        data_list[i].counter[1] = adv_buf[15];
        data_list[i].timeout_counter = 0;
      }
    }
    if (adv_len >= 16) { // Three packets- this could be changed to a function that figures out indices based on packet length
      i = find_device_ids(adv_buf[16], adv_buf[17]);
      if (i != 0 && compare_counter(adv_buf[20], adv_buf[21], i)){
        data_list[i].dev_id[0] = adv_buf[16];
        data_list[i].dev_id[1] = adv_buf[17];
        data_list[i].temp[0] = adv_buf[18];
        data_list[i].temp[1] = adv_buf[19];
        data_list[i].counter[0] = adv_buf[20];
        data_list[i].counter[1] = adv_buf[21];
        data_list[i].timeout_counter = 0;
      }
    }
  }
}

void check_for_timeout(){ // Check if timed out
    for (int i = 1; i < next_index; i++){
        if(data_list[i].timeout_counter >= TIME_OUT_LIMIT * 4){ // Wait until 4x the timeout limit before erasing- give it a chance to reconnect
            // Clear the node
            data_list[i].timeout_counter = 0;
            data_list[i].counter[0] = 0;
            data_list[i].counter[1] = 0;
            data_list[i].dev_id[0] = 0;
            data_list[i].dev_id[1] = 0;
            // Doesn't actually decrease the size,
        }
        else{
            data_list[i].timeout_counter++;
        }
    }
}

int main(void) {
  ret_code_t error_code = NRF_SUCCESS;

  // Setup for ADC
  // initialize analog to digital converter
  int16_t temp_val = 0;
  uint16_t counter_val = 0;
  nrfx_saadc_config_t saadc_config = NRFX_SAADC_DEFAULT_CONFIG;
  saadc_config.resolution = NRF_SAADC_RESOLUTION_12BIT;
  error_code = nrfx_saadc_init(&saadc_config, saadc_callback);
  APP_ERROR_CHECK(error_code);

  // initialize analog inputs
  // configure with 0 as input pin for now
  nrf_saadc_channel_config_t channel_config = NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(0);
  channel_config.gain = NRF_SAADC_GAIN1_6; // input gain of 1/6 Volts/Volt, multiply incoming signal by (1/6)
  channel_config.reference = NRF_SAADC_REFERENCE_INTERNAL; // 0.6 Volt reference, input after gain can be 0 to 0.6 Volts

  // specify input pin and initialize that ADC channel
  channel_config.pin_p = NRF_SAADC_INPUT_AIN7; // Pin P0.31
  error_code = nrfx_saadc_channel_init(INPUT_CHANNEL, &channel_config);
  APP_ERROR_CHECK(error_code);

  // Setup for packets
  data_list[0].dev_id[1] = THIS_DEVICE_ID;
  data_list[0].dev_id[0] = THIS_DEVICE_ID >> 8;
  data_list[0].counter[1] = counter_val;
  data_list[0].counter[0] = counter_val >> 8;


  printf("Board started. Initializing BLE: \n");

  // Setup BLE
  // Note: simple BLE is our own library. You can find it in `nrf5x-base/lib/simple_ble/`
  simple_ble_app = simple_ble_init(&ble_config);


  // Start scanning
  scanning_start();


  while(1) {
    // sample analog inputs
    nrf_saadc_value_t analog_val = sample_value(INPUT_CHANNEL);
    //3400=3V, so that's how the value is converted
    float temp_temp_val = (((float)(analog_val)*3000/3400)-500)/10; // Gets the value as a float with 1 degrees celsius accuracy
    temp_val = ((int16_t)temp_temp_val) * 100; // Converts to int with 0.01 degree accuracy
    if(abs(temp_val - ((data_list[0].temp[0] << 8) | data_list[0].temp[1])) > 5 || timeout_counter >= TIME_OUT_LIMIT || new_data){
        //Update data
        data_list[0].counter[1] = counter_val;
        data_list[0].counter[0] = counter_val >> 8;
        data_list[0].temp[1] = temp_val;
        data_list[0].temp[0] = temp_val >> 8;
        //reset things
        new_data = false;
        timeout_counter = 0;
        // This is where a more optimized setup would deal with
        // rotating through advertisements, selecting which advertisements
        // need to be sent (by adding a send_next bool to the struct)
        uint8_t ble_data[BLE_GAP_ADV_SET_DATA_SIZE_MAX] = {0x15, 0xFF, 0xFF, 0xFF, data_list[0].dev_id[0], data_list[0].dev_id[1], data_list[0].temp[0], data_list[0].temp[1], data_list[0].counter[0], data_list[0].counter[1], data_list[1].dev_id[0], data_list[1].dev_id[1], data_list[1].temp[0], data_list[1].temp[1], data_list[1].counter[0], data_list[1].counter[1], data_list[2].dev_id[0], data_list[2].dev_id[1], data_list[2].temp[0], data_list[2].temp[1], data_list[2].counter[0], data_list[2].counter[1]};
        printf("Advertising Stop to Change Values!!\n");
        printf("Temp Value: %d °C\n", temp_val);
        // Here is where advertising stops to change values
        // Once again, this could easily be modified to implement
        // a more complicated/robust energy efficient method of
        // sending and recieving data, such as some form of TDMA
        // rather than ALOHA
        advertising_stop(); // Stop in order to change the values
        simple_ble_adv_raw(ble_data, 22); // Start advertising with new values
        nrf_delay_ms(100); // Delay .1 second after starting advertisements
        counter_val++; // Increase counter for this device
    }
    // Uncomment the following to display results
    //printf("value: %d (millivolts)\n", analog_val*3000/3400);
    //printf("value: %f (°C)\n", temp_val);
      
    nrf_delay_ms(10);
    timeout_counter++;
    check_for_timeout();
  }
}

