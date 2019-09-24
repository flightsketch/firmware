/**
 * Copyright (c) 2014 - 2018, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/** @file
 *
 * @defgroup ble_sdk_uart_over_ble_main main.c
 * @{
 * @ingroup  ble_sdk_app_nus_eval
 * @brief    UART over BLE application main file.
 *
 * This file contains the source code for a sample application that uses the Nordic UART service.
 * This application uses the @ref srvlib_conn_params module.
 */


#include <stdint.h>
#include <string.h>
#include <math.h>
#include "nordic_common.h"
#include "nrf.h"
#include "ble_hci.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "app_timer.h"
#include "ble_nus.h"
#include "app_uart.h"
#include "app_util_platform.h"
#include "bsp_btn_ble.h"
#include "nrf_pwr_mgmt.h"

#include "nrf_drv_spi.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"

#include "nrf_drv_saadc.h"

#include "bmp280_driver/bmp280.h"
#include "bmp280_config.h"

#include "bmp388_driver/bmp3.h"



#if defined (UART_PRESENT)
#include "nrf_uart.h"
#endif
#if defined (UARTE_PRESENT)
#include "nrf_uarte.h"
#endif

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#define APP_BLE_CONN_CFG_TAG            1                                           /**< A tag identifying the SoftDevice BLE configuration. */

//#define DEVICE_NAME                     "FlightSketch"                               /**< Name of device. Will be included in the advertising data. */
#define NUS_SERVICE_UUID_TYPE           BLE_UUID_TYPE_VENDOR_BEGIN                  /**< UUID type for the Nordic UART Service (vendor specific). */

#define APP_BLE_OBSERVER_PRIO           3                                           /**< Application's BLE observer priority. You shouldn't need to modify this value. */

#define APP_ADV_INTERVAL                320                                          /**< The advertising interval (in units of 0.625 ms. This value corresponds to 40 ms). */

#define APP_ADV_DURATION                0                                       /**< The advertising duration (180 seconds) in units of 10 milliseconds. */

#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(20, UNIT_1_25_MS)             /**< Minimum acceptable connection interval (20 ms), Connection interval uses 1.25 ms units. */
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(75, UNIT_1_25_MS)             /**< Maximum acceptable connection interval (75 ms), Connection interval uses 1.25 ms units. */
#define SLAVE_LATENCY                   0                                           /**< Slave latency. */
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(4000, UNIT_10_MS)             /**< Connection supervisory timeout (4 seconds), Supervision Timeout uses 10 ms units. */
#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000)                       /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000)                      /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3                                           /**< Number of attempts before giving up the connection parameter negotiation. */

#define DEAD_BEEF                       0xDEADBEEF                                  /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

#define UART_TX_BUF_SIZE                256                                         /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE                256                                         /**< UART RX buffer size. */

#define MAIN_LOOP_PERIOD                20

#define MAIN_LOOP_INTERVAL         APP_TIMER_TICKS(MAIN_LOOP_PERIOD)                /**< Main loop interval (ticks). */



float dt = MAIN_LOOP_PERIOD/1000.0;

BLE_NUS_DEF(m_nus, NRF_SDH_BLE_TOTAL_LINK_COUNT);                                   /**< BLE NUS service instance. */
NRF_BLE_GATT_DEF(m_gatt);                                                           /**< GATT module instance. */
NRF_BLE_QWR_DEF(m_qwr);                                                             /**< Context for the Queued Write module.*/
BLE_ADVERTISING_DEF(m_advertising);                                                 /**< Advertising module instance. */

APP_TIMER_DEF(main_loop_id);

#define SPI_INSTANCE  0 /**< SPI instance index. */
static const nrf_drv_spi_t spi = NRF_DRV_SPI_INSTANCE(SPI_INSTANCE);  /**< SPI instance. */
static volatile bool spi_xfer_done;  /**< Flag used to indicate that SPI instance completed the transfer. */

#define TEST_STRING "FlightSketch!!!"
static uint8_t       m_tx_buf[] = TEST_STRING;           /**< TX buffer. */
static uint8_t       m_rx_buf[sizeof(TEST_STRING) + 1];    /**< RX buffer. */
static const uint8_t m_length = sizeof(m_tx_buf)-1;        /**< Transfer length. */

#define	SPI_READ	0x80
#define SPI_WRITE	0x7F

static uint16_t   m_conn_handle          = BLE_CONN_HANDLE_INVALID;                 /**< Handle of the current connection. */
static uint16_t   m_ble_nus_max_data_len = BLE_GATT_ATT_MTU_DEFAULT - 3;            /**< Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART service module. */
static ble_uuid_t m_adv_uuids[]          =                                          /**< Universally unique service identifier. */
{
    {BLE_UUID_NUS_SERVICE, NUS_SERVICE_UUID_TYPE}
};

char DEVICE_NAME[24];

bool main_loop_update = false;
int send_update = 0;
int send_update_int = 5;
int save_update = 0;
int save_update_int = 0;
int batt_update = 0;
int batt_update_int = 250;
float log_dt = 0.0;

float E = 0.0;

float K1_boost = 0.197584269677409;
float K2_boost = 1.086251556999431;
float K3_boost = 2.985922024587661;

float K1_coast = 0.077894859154768;
float K2_coast = 0.157902454768786;
float K3_coast = 0.160043843024796;

float K1 = 0.0;
float K2 = 0.0;
float K3 = 0.0;



float t1;
float t2;

float araw_last = 0.0;
float vinst = 0.0;
float vinst_last = 0.0;
float ainst = 0.0;

bool isIdle = false;

bool arm_request = false;
bool armedForLaunch = false;
bool launchDetect = false;
bool boost = false;

float timeToBurnout = 0.0;

bool armedForLanding = false;
float minLandingTime = 30.0;
bool landed = false;

bool record_data = false;
bool download_request = false;
unsigned int file_length = 0;
float data_time = 0;

bool baro_error = false;

void parsePacket_typeF1(void);

void parsePacket_typeF2(void);

void parsePacket_typeF3(void);

void parsePacket_typeF4(void);

static void main_loop_timeout_handler(void * p_context)
{
    UNUSED_PARAMETER(p_context);
    main_loop_update = true;
    
}

union data_address {
    unsigned char address_string[4];
    unsigned int  address_int;
};

union float_bytes {
    unsigned char float_string[4];
    float data;
};

struct state {
  float ref_pressure;
  float pressure;
  float altitude;
  float ref_altitude;
  float raw_altitude;
  float max_altitude;
  float velocity;
  float velocity_filt;
  float max_velocity;
  float acceleration;
  float temp;
};

struct status {
  bool isArmedForLaunch: 1;
  bool isRecording: 1;
};

struct dataPt {
  float pressure;
  float altitude;
  float velocity;
};

struct dataPt buffer[64];
int bufferStart = 0;


union status_bytes {

  struct status status;
  unsigned char   bytes[1];

};



struct state vehicle_state;

/**@brief Function for assert macro callback.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyse
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num    Line number of the failing ASSERT call.
 * @param[in] p_file_name File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

/**@brief Function for initializing the timer module.
 */
static void timers_init(void)
{
    ret_code_t err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);

    // Create timers.
    err_code = app_timer_create(&main_loop_id,
                                APP_TIMER_MODE_REPEATED,
                                main_loop_timeout_handler);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for the GAP initialization.
 *
 * @details This function will set up all the necessary GAP (Generic Access Profile) parameters of
 *          the device. It also sets the permissions and appearance.
 */
static void gap_params_init(void)
{
    uint32_t                err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *) DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling Queued Write Module errors.
 *
 * @details A pointer to this function will be passed to each service which may need to inform the
 *          application about an error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void nrf_qwr_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for handling the data from the Nordic UART Service.
 *
 * @details This function will process the data received from the Nordic UART BLE Service and send
 *          it to the UART module.
 *
 * @param[in] p_evt       Nordic UART Service event.
 */
/**@snippet [Handling the data received over BLE] */
static void nus_data_handler(ble_nus_evt_t * p_evt)
{

    if (p_evt->type == BLE_NUS_EVT_RX_DATA)
    {
          NRF_LOG_DEBUG("Rx Data");
          if (p_evt->params.rx_data.length == 4 && p_evt->params.rx_data.p_data[2] == 0){  //check if valid command packet
              //check checksum
              unsigned char chk = 0;
              chk = p_evt->params.rx_data.p_data[0] + p_evt->params.rx_data.p_data[1];
              if (chk == p_evt->params.rx_data.p_data[3]){
                  //valid command packet
                  unsigned char packet_type = p_evt->params.rx_data.p_data[1];
                  switch (packet_type){
                    case 0xF1: parsePacket_typeF1();
                    break;
                    case 0xF2: parsePacket_typeF2();
                    break;
                    case 0xF3: parsePacket_typeF3();
                    break;
                    case 0xF4: download_request = true;
                    break;
                    }
              }
          }
        

//        uint32_t err_code;
//
//        NRF_LOG_DEBUG("Received data from BLE NUS. Writing data on UART.");
//        NRF_LOG_HEXDUMP_DEBUG(p_evt->params.rx_data.p_data, p_evt->params.rx_data.length);
//
//        for (uint32_t i = 0; i < p_evt->params.rx_data.length; i++)
//        {
//            do
//            {
//                err_code = app_uart_put(p_evt->params.rx_data.p_data[i]);
//                if ((err_code != NRF_SUCCESS) && (err_code != NRF_ERROR_BUSY))
//                {
//                    NRF_LOG_ERROR("Failed receiving NUS message. Error 0x%x. ", err_code);
//                    APP_ERROR_CHECK(err_code);
//                }
//            } while (err_code == NRF_ERROR_BUSY);
//        }
//        if (p_evt->params.rx_data.p_data[p_evt->params.rx_data.length - 1] == '\r')
//        {
//            while (app_uart_put('\n') == NRF_ERROR_BUSY);
//        }
    }

}
/**@snippet [Handling the data received over BLE] */


/**@brief Function for initializing services that will be used by the application.
 */
static void services_init(void)
{
    uint32_t           err_code;
    ble_nus_init_t     nus_init;
    nrf_ble_qwr_init_t qwr_init = {0};

    // Initialize Queued Write Module.
    qwr_init.error_handler = nrf_qwr_error_handler;

    err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
    APP_ERROR_CHECK(err_code);

    // Initialize NUS.
    memset(&nus_init, 0, sizeof(nus_init));

    nus_init.data_handler = nus_data_handler;

    err_code = ble_nus_init(&m_nus, &nus_init);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling an event from the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module
 *          which are passed to the application.
 *
 * @note All this function does is to disconnect. This could have been done by simply setting
 *       the disconnect_on_fail config parameter, but instead we use the event handler
 *       mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    uint32_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
}


/**@brief Function for handling errors from the Connection Parameters module.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    uint32_t               err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for putting the chip into sleep mode.
 *
 * @note This function will not return.
 */
static void sleep_mode_enter(void)
{
    uint32_t err_code = bsp_indication_set(BSP_INDICATE_IDLE);
    APP_ERROR_CHECK(err_code);

    // Prepare wakeup buttons.
    err_code = bsp_btn_ble_sleep_mode_prepare();
    APP_ERROR_CHECK(err_code);

    // Go to system-off mode (this function will not return; wakeup will cause a reset).
    err_code = sd_power_system_off();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    uint32_t err_code;

    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
            err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
            APP_ERROR_CHECK(err_code);
            break;
        case BLE_ADV_EVT_IDLE:
            sleep_mode_enter();
            break;
        default:
            break;
    }
}


/**@brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    uint32_t err_code;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            NRF_LOG_INFO("Connected");
            err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
            APP_ERROR_CHECK(err_code);
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            NRF_LOG_INFO("Disconnected");
            // LED indication will be changed when advertising starts.
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
            break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
            NRF_LOG_DEBUG("PHY update request.");
            ble_gap_phys_t const phys =
            {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
            err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            APP_ERROR_CHECK(err_code);
        } break;

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            // Pairing not supported
            err_code = sd_ble_gap_sec_params_reply(m_conn_handle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            // No system attributes have been stored.
            err_code = sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTC_EVT_TIMEOUT:
            // Disconnect on GATT Client timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        default:
            // No implementation needed.
            break;
    }
}


/**@brief Function for the SoftDevice initialization.
 *
 * @details This function initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    ret_code_t err_code;

    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);

    // Configure the BLE stack using the default settings.
    // Fetch the start address of the application RAM.
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    APP_ERROR_CHECK(err_code);

    // Enable BLE stack.
    err_code = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err_code);

    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
}


/**@brief Function for handling events from the GATT library. */
void gatt_evt_handler(nrf_ble_gatt_t * p_gatt, nrf_ble_gatt_evt_t const * p_evt)
{
    if ((m_conn_handle == p_evt->conn_handle) && (p_evt->evt_id == NRF_BLE_GATT_EVT_ATT_MTU_UPDATED))
    {
        m_ble_nus_max_data_len = p_evt->params.att_mtu_effective - OPCODE_LENGTH - HANDLE_LENGTH;
        NRF_LOG_INFO("Data len is set to 0x%X(%d)", m_ble_nus_max_data_len, m_ble_nus_max_data_len);
    }
    NRF_LOG_DEBUG("ATT MTU exchange completed. central 0x%x peripheral 0x%x",
                  p_gatt->att_mtu_desired_central,
                  p_gatt->att_mtu_desired_periph);
}


/**@brief Function for initializing the GATT library. */
void gatt_init(void)
{
    ret_code_t err_code;

    err_code = nrf_ble_gatt_init(&m_gatt, gatt_evt_handler);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_ble_gatt_att_mtu_periph_set(&m_gatt, NRF_SDH_BLE_GATT_MAX_MTU_SIZE);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling events from the BSP module.
 *
 * @param[in]   event   Event generated by button press.
 */
void bsp_event_handler(bsp_event_t event)
{
    uint32_t err_code;
    switch (event)
    {
        case BSP_EVENT_SLEEP:
            sleep_mode_enter();
            break;

        case BSP_EVENT_DISCONNECT:
            err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            if (err_code != NRF_ERROR_INVALID_STATE)
            {
                APP_ERROR_CHECK(err_code);
            }
            break;

        case BSP_EVENT_WHITELIST_OFF:
            if (m_conn_handle == BLE_CONN_HANDLE_INVALID)
            {
                err_code = ble_advertising_restart_without_whitelist(&m_advertising);
                if (err_code != NRF_ERROR_INVALID_STATE)
                {
                    APP_ERROR_CHECK(err_code);
                }
            }
            break;

        default:
            break;
    }
}


/**@brief   Function for handling app_uart events.
 *
 * @details This function will receive a single character from the app_uart module and append it to
 *          a string. The string will be be sent over BLE when the last character received was a
 *          'new line' '\n' (hex 0x0A) or if the string has reached the maximum data length.
 */
/**@snippet [Handling the data received over UART] */
void uart_event_handle(app_uart_evt_t * p_event)
{
    static uint8_t data_array[BLE_NUS_MAX_DATA_LEN];
    static uint8_t index = 0;
    uint32_t       err_code;

    switch (p_event->evt_type)
    {
        case APP_UART_DATA_READY:
            UNUSED_VARIABLE(app_uart_get(&data_array[index]));
            index++;

            if ((data_array[index - 1] == '\n') ||
                (data_array[index - 1] == '\r') ||
                (index >= m_ble_nus_max_data_len))
            {
                if (index > 1)
                {
                    NRF_LOG_DEBUG("Ready to send data over BLE NUS");
                    NRF_LOG_HEXDUMP_DEBUG(data_array, index);

                    do
                    {
                        uint16_t length = (uint16_t)index;
                        err_code = ble_nus_data_send(&m_nus, data_array, &length, m_conn_handle);
                        if ((err_code != NRF_ERROR_INVALID_STATE) &&
                            (err_code != NRF_ERROR_RESOURCES) &&
                            (err_code != NRF_ERROR_NOT_FOUND))
                        {
                            APP_ERROR_CHECK(err_code);
                        }
                    } while (err_code == NRF_ERROR_RESOURCES);
                }

                index = 0;
            }
            break;

        case APP_UART_COMMUNICATION_ERROR:
            APP_ERROR_HANDLER(p_event->data.error_communication);
            break;

        case APP_UART_FIFO_ERROR:
            APP_ERROR_HANDLER(p_event->data.error_code);
            break;

        default:
            break;
    }
}
/**@snippet [Handling the data received over UART] */


/**@brief  Function for initializing the UART module.
 */
/**@snippet [UART Initialization] */
static void uart_init(void)
{
    uint32_t                     err_code;
    app_uart_comm_params_t const comm_params =
    {
        .rx_pin_no    = RX_PIN_NUMBER,
        .tx_pin_no    = TX_PIN_NUMBER,
        .rts_pin_no   = RTS_PIN_NUMBER,
        .cts_pin_no   = CTS_PIN_NUMBER,
        .flow_control = APP_UART_FLOW_CONTROL_DISABLED,
        .use_parity   = false,
#if defined (UART_PRESENT)
        .baud_rate    = NRF_UART_BAUDRATE_115200
#else
        .baud_rate    = NRF_UARTE_BAUDRATE_115200
#endif
    };

    APP_UART_FIFO_INIT(&comm_params,
                       UART_RX_BUF_SIZE,
                       UART_TX_BUF_SIZE,
                       uart_event_handle,
                       APP_IRQ_PRIORITY_LOWEST,
                       err_code);
    APP_ERROR_CHECK(err_code);
}
/**@snippet [UART Initialization] */

static void application_timers_start(void)
{
    ret_code_t err_code;

    // Start application timers.
    err_code = app_timer_start(main_loop_id, MAIN_LOOP_INTERVAL, NULL);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the Advertising functionality.
 */
static void advertising_init(void)
{
    uint32_t               err_code;
    ble_advertising_init_t init;

    memset(&init, 0, sizeof(init));

    init.advdata.name_type          = BLE_ADVDATA_FULL_NAME;
    init.advdata.include_appearance = false;
    init.advdata.flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;

    init.srdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    init.srdata.uuids_complete.p_uuids  = m_adv_uuids;

    init.config.ble_adv_fast_enabled  = true;
    init.config.ble_adv_fast_interval = APP_ADV_INTERVAL;
    init.config.ble_adv_fast_timeout  = APP_ADV_DURATION;
    init.evt_handler = on_adv_evt;

    err_code = ble_advertising_init(&m_advertising, &init);
    APP_ERROR_CHECK(err_code);

    ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
}


/**@brief Function for initializing buttons and leds.
 *
 * @param[out] p_erase_bonds  Will be true if the clear bonding button was pressed to wake the application up.
 */
static void buttons_leds_init(bool * p_erase_bonds)
{
    bsp_event_t startup_event;

    uint32_t err_code = bsp_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS, bsp_event_handler);
    APP_ERROR_CHECK(err_code);

    err_code = bsp_btn_ble_init(NULL, &startup_event);
    APP_ERROR_CHECK(err_code);

    *p_erase_bonds = (startup_event == BSP_EVENT_CLEAR_BONDING_DATA);
}


/**@brief Function for initializing the nrf log module.
 */
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}


/**@brief Function for initializing power management.
 */
static void power_management_init(void)
{
    ret_code_t err_code;
    err_code = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling the idle state (main loop).
 *
 * @details If there is no pending log operation, then sleep until next the next event occurs.
 */
static void idle_state_handle(void)
{
    UNUSED_RETURN_VALUE(NRF_LOG_PROCESS());
    nrf_pwr_mgmt_run();
}


/**@brief Function for starting advertising.
 */
static void advertising_start(void)
{
    uint32_t err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
    APP_ERROR_CHECK(err_code);
}

// Input range of internal Vdd measurement = (0.6 V)/(1/6) = 3.6 V
// 3.0 volts -> 14486 ADC counts with 14-bit sampling: 4828.8 counts per volt
#define ADC12_COUNTS_PER_VOLT 4551

/**
 * @brief Function for 14-bit adc init in polled mode
 */
void Adc12bitPolledInitialise(void)
{
    uint32_t timeout = 10;
    nrf_saadc_channel_config_t myConfig =
    {
        .resistor_p = NRF_SAADC_RESISTOR_DISABLED,
        .resistor_n = NRF_SAADC_RESISTOR_DISABLED,
        .gain       = NRF_SAADC_GAIN1_6,
        .reference  = NRF_SAADC_REFERENCE_INTERNAL,
        .acq_time   = NRF_SAADC_ACQTIME_40US,
        .mode       = NRF_SAADC_MODE_SINGLE_ENDED,
        .burst      = NRF_SAADC_BURST_ENABLED,
        .pin_p      = NRF_SAADC_INPUT_VDD,
        .pin_n      = NRF_SAADC_INPUT_DISABLED
    };

    nrf_saadc_resolution_set((nrf_saadc_resolution_t) 3);   // 3 is 14-bit
    nrf_saadc_oversample_set((nrf_saadc_oversample_t) 2);   // 2 is 4x, about 150uSecs total
    nrf_saadc_int_disable(NRF_SAADC_INT_ALL);
    nrf_saadc_event_clear(NRF_SAADC_EVENT_END);
    nrf_saadc_event_clear(NRF_SAADC_EVENT_STARTED);
    nrf_saadc_enable();

    NRF_SAADC->CH[1].CONFIG =
              ((myConfig.resistor_p << SAADC_CH_CONFIG_RESP_Pos)   & SAADC_CH_CONFIG_RESP_Msk)
            | ((myConfig.resistor_n << SAADC_CH_CONFIG_RESN_Pos)   & SAADC_CH_CONFIG_RESN_Msk)
            | ((myConfig.gain       << SAADC_CH_CONFIG_GAIN_Pos)   & SAADC_CH_CONFIG_GAIN_Msk)
            | ((myConfig.reference  << SAADC_CH_CONFIG_REFSEL_Pos) & SAADC_CH_CONFIG_REFSEL_Msk)
            | ((myConfig.acq_time   << SAADC_CH_CONFIG_TACQ_Pos)   & SAADC_CH_CONFIG_TACQ_Msk)
            | ((myConfig.mode       << SAADC_CH_CONFIG_MODE_Pos)   & SAADC_CH_CONFIG_MODE_Msk)
            | ((myConfig.burst      << SAADC_CH_CONFIG_BURST_Pos)  & SAADC_CH_CONFIG_BURST_Msk);

    NRF_SAADC->CH[1].PSELN = myConfig.pin_n;
    NRF_SAADC->CH[1].PSELP = myConfig.pin_p;
}

/**
 * @brief Function for 14-bit adc battery voltage by direct blocking reading
 */
uint16_t GetBatteryVoltage1(void)
{
    uint16_t result = 9999;         // Some recognisable dummy value
    uint32_t timeout = 10000;       // Trial and error
    volatile int16_t buffer[8];
    // Enable command
    nrf_saadc_enable();
    NRF_SAADC->RESULT.PTR = (uint32_t)buffer;
    NRF_SAADC->RESULT.MAXCNT = 1;
    nrf_saadc_event_clear(NRF_SAADC_EVENT_END);
    nrf_saadc_task_trigger(NRF_SAADC_TASK_START);
    nrf_saadc_task_trigger(NRF_SAADC_TASK_SAMPLE);

    while (0 == nrf_saadc_event_check(NRF_SAADC_EVENT_END) && timeout > 0)
    {
        timeout--;
    }
    nrf_saadc_task_trigger(NRF_SAADC_TASK_STOP);
    nrf_saadc_event_clear(NRF_SAADC_EVENT_STARTED);
    nrf_saadc_event_clear(NRF_SAADC_EVENT_END);
    // Disable command to reduce power consumption
    nrf_saadc_disable();
    if (timeout != 0)
    {
        result = ((buffer[0] * 1000L)+(ADC12_COUNTS_PER_VOLT/2)) / ADC12_COUNTS_PER_VOLT;
    }
    return result;
}




void spi_event_handler(nrf_drv_spi_evt_t const * p_event,
                       void *                    p_context)
{
    spi_xfer_done = true;
    //NRF_LOG_INFO("Transfer completed.");
    if (m_rx_buf[0] != 0)
    {
        NRF_LOG_INFO(" Received:");
        NRF_LOG_HEXDUMP_INFO(m_rx_buf, strlen((const char *)m_rx_buf));
    }
}

void spi_init(void){
    nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_config.ss_pin   = SPI_SS_PIN;
    spi_config.miso_pin = SPI_MISO_PIN;
    spi_config.mosi_pin = SPI_MOSI_PIN;
    spi_config.sck_pin  = SPI_SCK_PIN;
    spi_config.frequency = NRF_DRV_SPI_FREQ_4M;
    APP_ERROR_CHECK(nrf_drv_spi_init(&spi, &spi_config, spi_event_handler, NULL));
    NRF_LOG_INFO("SPI example started.");
}

int8_t BMP_280_read(uint8_t dev_id, uint8_t reg_addr, uint8_t *data, uint16_t len){
    //NRF_LOG_INFO("read");
    spi_xfer_done = false;
    uint8_t tx_buffer[30] = {0};
    uint8_t rx_buffer[30] = {0};
    uint8_t rx_buffer_out[30];
    tx_buffer[0] = reg_addr|SPI_READ;
    uint8_t i;
    
    nrf_gpio_pin_clear(29);
    nrf_drv_spi_transfer(&spi, tx_buffer, len+1, rx_buffer, len+1);
    
    while (!spi_xfer_done){
        __WFE();
    }
    nrf_gpio_pin_set(29);
    for(i=0; i<len; i++){
        *(data+i) = rx_buffer[i+1];
    }

    return (int8_t)BMP280_OK;
}

int8_t BMP_280_write(uint8_t dev_id, uint8_t reg_addr, uint8_t *data, uint16_t len){
    //NRF_LOG_INFO("write");
    spi_xfer_done = false;
    uint8_t tx_buffer[30] = {0};
    tx_buffer[0] = reg_addr;
    uint8_t i;

    for(i=0; i<len; i++){
        tx_buffer[i+1] = *(data+i);
    }
    nrf_gpio_pin_clear(29);
    nrf_drv_spi_transfer(&spi, tx_buffer, len+1, NULL, len+1);
    
    while (!spi_xfer_done){
        __WFE();
    }
    nrf_gpio_pin_set(29);
    return BMP280_OK;
}


void store_recording_started(void){

    spi_xfer_done = false;
    uint8_t tx_buffer[20] = {0}; //write cmd, 3 byte address, 16 bytes data
    uint8_t rx_buffer[20] = {0};
    tx_buffer[0] = 0x06;

    uint8_t i;
    
    nrf_gpio_pin_clear(9);
    nrf_drv_spi_transfer(&spi, tx_buffer, 1, rx_buffer, 1);
    
    while (!spi_xfer_done){
        __WFE();
    }
    nrf_gpio_pin_set(9);

    union data_address address_bytes;
    address_bytes.address_int = 0x008000;

    tx_buffer[0] = 0x02;

    tx_buffer[1] = address_bytes.address_string[2];
    tx_buffer[2] = address_bytes.address_string[1];
    tx_buffer[3] = address_bytes.address_string[0];

    tx_buffer[4] = 0x01;


    spi_xfer_done = false;
    nrf_gpio_pin_clear(9);
    nrf_drv_spi_transfer(&spi, tx_buffer, 5, rx_buffer, 5);
    
    while (!spi_xfer_done){
        __WFE();
    }
    nrf_gpio_pin_set(9);
}

void store_recording_finished(void){

    spi_xfer_done = false;
    uint8_t tx_buffer[20] = {0}; //write cmd, 3 byte address, 16 bytes data
    uint8_t rx_buffer[20] = {0};
    tx_buffer[0] = 0x06;

    uint8_t i;
    
    nrf_gpio_pin_clear(9);
    nrf_drv_spi_transfer(&spi, tx_buffer, 1, rx_buffer, 1);
    
    while (!spi_xfer_done){
        __WFE();
    }
    nrf_gpio_pin_set(9);

    union data_address address_bytes;
    address_bytes.address_int = 0x008001;

    tx_buffer[0] = 0x02;

    tx_buffer[1] = address_bytes.address_string[2];
    tx_buffer[2] = address_bytes.address_string[1];
    tx_buffer[3] = address_bytes.address_string[0];

    tx_buffer[4] = 0x01;


    spi_xfer_done = false;
    nrf_gpio_pin_clear(9);
    nrf_drv_spi_transfer(&spi, tx_buffer, 5, rx_buffer, 5);
    
    while (!spi_xfer_done){
        __WFE();
    }
    nrf_gpio_pin_set(9);
}

void store_file_length(void){

    spi_xfer_done = false;
    uint8_t tx_buffer[20] = {0}; //write cmd, 3 byte address, 16 bytes data
    uint8_t rx_buffer[20] = {0};
    tx_buffer[0] = 0x06;

    uint8_t i;
    
    nrf_gpio_pin_clear(9);
    nrf_drv_spi_transfer(&spi, tx_buffer, 1, rx_buffer, 1);
    
    while (!spi_xfer_done){
        __WFE();
    }
    nrf_gpio_pin_set(9);

    union data_address address_bytes;
    address_bytes.address_int = 0x008003;

    union data_address file_length_write;
    file_length_write.address_int = file_length;



    tx_buffer[0] = 0x02;

    tx_buffer[1] = address_bytes.address_string[2];
    tx_buffer[2] = address_bytes.address_string[1];
    tx_buffer[3] = address_bytes.address_string[0];

    tx_buffer[4] = file_length_write.address_string[2];
    tx_buffer[5] = file_length_write.address_string[1];
    tx_buffer[6] = file_length_write.address_string[0];

    spi_xfer_done = false;
    nrf_gpio_pin_clear(9);
    nrf_drv_spi_transfer(&spi, tx_buffer, 7, rx_buffer, 7);
    
    while (!spi_xfer_done){
        __WFE();
    }
    nrf_gpio_pin_set(9);


}


void write_data(unsigned int address){

    spi_xfer_done = false;
    uint8_t tx_buffer[20] = {0}; //write cmd, 3 byte address, 16 bytes data
    uint8_t rx_buffer[20] = {0};
    tx_buffer[0] = 0x06;

    uint8_t i;
    
    nrf_gpio_pin_clear(9);
    nrf_drv_spi_transfer(&spi, tx_buffer, 1, rx_buffer, 1);
    
    while (!spi_xfer_done){
        __WFE();
    }
    nrf_gpio_pin_set(9);

    union data_address address_bytes;
    address_bytes.address_int = address + 0x008100;

    union float_bytes time_bytes;
    time_bytes.data = data_time;
    union float_bytes press_bytes;
    press_bytes.data = vehicle_state.pressure;
    union float_bytes alt_bytes;
    alt_bytes.data = vehicle_state.altitude;
    union float_bytes vel_bytes;
    vel_bytes.data = vehicle_state.velocity;

    tx_buffer[0] = 0x02;

    tx_buffer[1] = address_bytes.address_string[2];
    tx_buffer[2] = address_bytes.address_string[1];
    tx_buffer[3] = address_bytes.address_string[0];

    for (i=0; i<4; i++){
        tx_buffer[4+i] = time_bytes.float_string[i];
    }

    for (i=0; i<4; i++){
        tx_buffer[8+i] = press_bytes.float_string[i];
    }

    for (i=0; i<4; i++){
        tx_buffer[12+i] = alt_bytes.float_string[i];
    }

    for (i=0; i<4; i++){
        tx_buffer[16+i] = vel_bytes.float_string[i];
    }

    spi_xfer_done = false;
    nrf_gpio_pin_clear(9);
    nrf_drv_spi_transfer(&spi, tx_buffer, 20, rx_buffer, 20);
    
    while (!spi_xfer_done){
        __WFE();
    }
    nrf_gpio_pin_set(9);


    file_length = file_length + 16;
    data_time = data_time + log_dt;

}

void write_buffer(void){

    int j = 0;
    int k = 0;
    int buffer_position;

    
    uint8_t tx_buffer[132] = {0}; //write cmd, 3 byte address, 16 bytes data
    uint8_t rx_buffer[132] = {0};
    

    int i;
    
    

    union float_bytes time_bytes;
    union float_bytes press_bytes;
    union float_bytes alt_bytes;
    union float_bytes vel_bytes;
    nrf_delay_us(700);
    for (k=0; k<8; k++){
      spi_xfer_done = false;
      tx_buffer[0] = 0x06;
      nrf_gpio_pin_clear(9);
      nrf_drv_spi_transfer(&spi, tx_buffer, 1, rx_buffer, 1);
    
      while (!spi_xfer_done){
          __WFE();
      }
      nrf_gpio_pin_set(9);

      tx_buffer[0] = 0x02;
      union data_address address_bytes;
      address_bytes.address_int = file_length + 0x008100;
      tx_buffer[1] = address_bytes.address_string[2];
      tx_buffer[2] = address_bytes.address_string[1];
      tx_buffer[3] = address_bytes.address_string[0];


      
      spi_xfer_done = false;
      nrf_gpio_pin_clear(9);

      for (j=0; j<8; j++){
        
        buffer_position = bufferStart + 8*k+j;
        if (buffer_position >= 64){
          buffer_position = buffer_position - 64;
        }
        time_bytes.data = data_time;
        press_bytes.data = buffer[buffer_position].pressure;
        alt_bytes.data = buffer[buffer_position].altitude;
        vel_bytes.data = buffer[buffer_position].velocity;

        for (i=0; i<4; i++){
            tx_buffer[j*16+4+i] = time_bytes.float_string[i];
        }

        for (i=0; i<4; i++){
            tx_buffer[j*16+8+i] = press_bytes.float_string[i];
        }

        for (i=0; i<4; i++){
            tx_buffer[j*16+12+i] = alt_bytes.float_string[i];
        }

        for (i=0; i<4; i++){
            tx_buffer[j*16+16+i] = vel_bytes.float_string[i];
        }
      
        data_time = data_time + log_dt;
      }

      nrf_drv_spi_transfer(&spi, tx_buffer, 132, rx_buffer, 132);
    
      while (!spi_xfer_done){
          __WFE();
      }
      nrf_gpio_pin_set(9);

      file_length = file_length + 128;
      nrf_delay_us(700);
    }

}

float read_float(unsigned int address){

    spi_xfer_done = false;
    uint8_t tx_buffer[8] = {0}; //read cmd, 3 byte address, 4 bytes data
    uint8_t rx_buffer[8] = {0};
    tx_buffer[0] = 0x03;

    union data_address address_bytes;
    address_bytes.address_int = address + 0x008100;

    union float_bytes read_bytes;

    tx_buffer[1] = address_bytes.address_string[2];
    tx_buffer[2] = address_bytes.address_string[1];
    tx_buffer[3] = address_bytes.address_string[0];

    nrf_gpio_pin_clear(9);
    nrf_drv_spi_transfer(&spi, tx_buffer, 8, rx_buffer, 8);
    
    while (!spi_xfer_done){
        __WFE();
    }
    nrf_gpio_pin_set(9);

    read_bytes.float_string[0] = rx_buffer[4];
    read_bytes.float_string[1] = rx_buffer[5];
    read_bytes.float_string[2] = rx_buffer[6];
    read_bytes.float_string[3] = rx_buffer[7];

    return read_bytes.data;

}

void read_file_length(void){

    spi_xfer_done = false;
    uint8_t tx_buffer[8] = {0}; //read cmd, 3 byte address, 4 bytes data
    uint8_t rx_buffer[8] = {0};
    tx_buffer[0] = 0x03;

    union data_address address_bytes;
    address_bytes.address_int = 0x008003;

    union data_address read_file_length;
    

    tx_buffer[1] = address_bytes.address_string[2];
    tx_buffer[2] = address_bytes.address_string[1];
    tx_buffer[3] = address_bytes.address_string[0];

    nrf_gpio_pin_clear(9);
    nrf_drv_spi_transfer(&spi, tx_buffer, 7, rx_buffer, 7);
    
    while (!spi_xfer_done){
        __WFE();
    }
    nrf_gpio_pin_set(9);

    read_file_length.address_string[2] = rx_buffer[4];
    read_file_length.address_string[1] = rx_buffer[5];
    read_file_length.address_string[0] = rx_buffer[6];

    if (read_file_length.address_int < 0xFFFFFF){
        file_length = read_file_length.address_int;
    }
    else {
        file_length = 0;
    }

}

    // read rec start
uint8_t read_rec_start(void){

    spi_xfer_done = false;
    uint8_t tx_buffer[8] = {0}; //read cmd, 3 byte address, 4 bytes data
    uint8_t rx_buffer[8] = {0};
    tx_buffer[0] = 0x03;

    union data_address address_bytes;
    address_bytes.address_int = 0x008000;

    tx_buffer[1] = address_bytes.address_string[2];
    tx_buffer[2] = address_bytes.address_string[1];
    tx_buffer[3] = address_bytes.address_string[0];

    nrf_gpio_pin_clear(9);
    nrf_drv_spi_transfer(&spi, tx_buffer, 5, rx_buffer, 5);
    
    while (!spi_xfer_done){
        __WFE();
    }
    nrf_gpio_pin_set(9);

    return rx_buffer[4];

}
    // read rec finished
uint8_t read_rec_finished(void){

    spi_xfer_done = false;
    uint8_t tx_buffer[8] = {0}; //read cmd, 3 byte address, 4 bytes data
    uint8_t rx_buffer[8] = {0};
    tx_buffer[0] = 0x03;

    union data_address address_bytes;
    address_bytes.address_int = 0x008001;

    tx_buffer[1] = address_bytes.address_string[2];
    tx_buffer[2] = address_bytes.address_string[1];
    tx_buffer[3] = address_bytes.address_string[0];

    nrf_gpio_pin_clear(9);
    nrf_drv_spi_transfer(&spi, tx_buffer, 5, rx_buffer, 5);
    
    while (!spi_xfer_done){
        __WFE();
    }
    nrf_gpio_pin_set(9);

    return rx_buffer[4];

}

void scan_for_eof(void){

    uint8_t tx_buffer[8] = {0}; //read cmd, 3 byte address, 4 bytes data
    uint8_t rx_buffer[8] = {0};
    tx_buffer[0] = 0x03;

    union data_address address_bytes;

    bool in_file = true;

    while (in_file){
        address_bytes.address_int = file_length + 0x008100;

        tx_buffer[1] = address_bytes.address_string[2];
        tx_buffer[2] = address_bytes.address_string[1];
        tx_buffer[3] = address_bytes.address_string[0];

        nrf_gpio_pin_clear(9);

        spi_xfer_done = false;
        nrf_drv_spi_transfer(&spi, tx_buffer, 8, rx_buffer, 8);
    
        while (!spi_xfer_done){
            __WFE();
        }
        nrf_gpio_pin_set(9);

        if (rx_buffer[4] == 0xFF && rx_buffer[5] == 0xFF && rx_buffer[6] == 0xFF && rx_buffer[7] == 0xFF){
          in_file = false;
        }
        else{
          file_length = file_length + 16;
        }
    }
}

unsigned char check_flash(void){

    spi_xfer_done = false;
    uint8_t tx_buffer[6] = {0}; //read cmd, 3 byte address, 4 bytes data
    uint8_t rx_buffer[6] = {0};
    tx_buffer[0] = 0x90;


    nrf_gpio_pin_clear(9);
    nrf_drv_spi_transfer(&spi, tx_buffer, 6, rx_buffer, 6);
    
    while (!spi_xfer_done){
        __WFE();
    }
    nrf_gpio_pin_set(9);

    return rx_buffer[4];



}

void erase_data(void){

    spi_xfer_done = false;
    uint8_t tx_buffer[1] = {0};
    uint8_t rx_buffer[1] = {0};
    tx_buffer[0] = 0x06;

    nrf_gpio_pin_clear(9);
    nrf_drv_spi_transfer(&spi, tx_buffer, 1, rx_buffer, 1);
    
    while (!spi_xfer_done){
        __WFE();
    }
    nrf_gpio_pin_set(9);

    tx_buffer[0] = 0xc7;


    spi_xfer_done = false;
    nrf_gpio_pin_clear(9);
    nrf_drv_spi_transfer(&spi, tx_buffer, 1, rx_buffer, 1);
    
    while (!spi_xfer_done){
        __WFE();
    }
    nrf_gpio_pin_set(9);

}


int8_t rslt;
struct bmp280_dev bmp;

struct bmp3_dev bmp388;

void bmp280_config(void){

        /* Sensor interface over SPI with native chip select line */
    bmp.dev_id  =  0;
    bmp.intf = BMP280_SPI_INTF;
    bmp.read = BMP_280_read;
    bmp.write = BMP_280_write;
    bmp.delay_ms = nrf_delay_ms;

    rslt = bmp280_init(&bmp);
    NRF_LOG_INFO("Device found with chip id 0x%x", bmp.chip_id);
    if (rslt == BMP280_OK) {
      /* Sensor chip ID will be printed if initialization was successful */
      NRF_LOG_INFO("Device found with chip id 0x%x", bmp.chip_id);
    }

    struct bmp280_config conf;

    /* Always read the current settings before writing, especially when
     * all the configuration is not modified 
     */
    rslt = bmp280_get_config(&conf, &bmp);
    /* Check if rslt == BMP280_OK, if not, then handle accordingly */

    /* Overwrite the desired settings */
    conf.filter = BMP280_FILTER_OFF;
    conf.os_pres = BMP280_OS_8X;
    conf.os_temp = BMP280_OS_1X;
    conf.odr = BMP280_ODR_0_5_MS;

    rslt = bmp280_set_config(&conf, &bmp);
    /* Check if rslt == BMP280_OK, if not, then handle accordingly */

    /* Always set the power mode after setting the configuration */
    rslt = bmp280_set_power_mode(BMP280_FORCED_MODE, &bmp);
    /* Check if rslt == BMP280_OK, if not, then handle accordingly */

    rslt = bmp280_get_config(&conf, &bmp);
    /* Check if rslt == BMP280_OK, if not, then handle accordingly */


    struct bmp280_uncomp_data ucomp_data;
    uint8_t meas_dur = bmp280_compute_meas_time(&bmp);

    NRF_LOG_INFO("Measurement duration: %dms\r\n", meas_dur);
    
    /* Check if rslt == BMP280_OK, if not, then handle accordingly */


}

struct bmp280_data {    
 float pressure;
 float temp;
};

struct bmp388_data {    
 float pressure;
 float temp;
};

struct bmp280_data bmp280_read(){

    struct bmp280_data data;
    struct bmp280_uncomp_data ucomp_data;

    spi_xfer_done = false;
    uint8_t tx_buffer[2] = {0};
    uint8_t rx_buffer[2] = {0};
    tx_buffer[0] = 0xD0;
    tx_buffer[1] = 0xF5;
    
    nrf_gpio_pin_clear(29);
    nrf_drv_spi_transfer(&spi, tx_buffer, 2, rx_buffer, 2);
    while (!spi_xfer_done){
        __WFE();
    }
    nrf_gpio_pin_set(29);

    if (rx_buffer[1] == 0x58){

        rslt = bmp280_get_uncomp_data(&ucomp_data, &bmp);
        
        double temp = bmp280_comp_temp_double(ucomp_data.uncomp_temp, &bmp);
        double pres = bmp280_comp_pres_double(ucomp_data.uncomp_press, &bmp);

        data.temp = (float) temp;
        data.pressure = (float) pres/1000.0;
    }
    else {
        spi_xfer_done = false;
        tx_buffer[0] = 0x60;
        tx_buffer[1] = 0xB6;

        nrf_gpio_pin_clear(29);
        nrf_drv_spi_transfer(&spi, tx_buffer, 2, rx_buffer, 2);
        while (!spi_xfer_done){
            __WFE();
        }
        nrf_gpio_pin_set(29);

        baro_error = true;
        data.temp = -999.999;
        data.pressure = -999.999;
    }

    rslt = bmp280_set_power_mode(BMP280_FORCED_MODE, &bmp);
    /* Check if rslt == BMP280_OK, if not, then handle accordingly */

    return data;


}


struct bmp388_data bmp388_read()
{
    struct bmp388_data data;

    int8_t rslt;
    /* Variable used to select the sensor component */
    uint8_t sensor_comp;
    /* Variable used to store the compensated data */
    struct bmp3_data data_read;

    /* Sensor component selection */
    sensor_comp = BMP3_PRESS | BMP3_TEMP;
    /* Temperature and Pressure data are read and stored in the bmp3_data instance */
    rslt = bmp3_get_sensor_data(sensor_comp, &data_read, &bmp388);

    /* Print the temperature and pressure data */
    //NRF_LOG_INFO("Temperature\t Pressure\t\n");
    //NRF_LOG_INFO("%0.2f\t\t %0.2f\t\t\n",data.temperature, data.pressure);

    data.pressure = (float) data_read.pressure/1000.0;
    data.temp = (float) data_read.temperature;

    rslt = bmp3_set_op_mode(&bmp388);

    return data;
}

void read_baro(void){

  //struct bmp280_data data;
  //data = bmp280_read();

  struct bmp388_data data;
  data = bmp388_read();

  vehicle_state.pressure = data.pressure;
  vehicle_state.temp = data.temp;

  float raw_alt = 0.0;

  raw_alt = data.pressure/101.325;
  raw_alt = pow(raw_alt,0.190284);
  raw_alt = 1.0 - raw_alt;
  raw_alt = raw_alt * 145366.45;

  vehicle_state.raw_altitude = raw_alt - vehicle_state.ref_altitude;
}

void update_state(void){

    float accelLimit = 5.0*32.2;

    vehicle_state.altitude = vehicle_state.altitude + t1*vehicle_state.velocity + t2*vehicle_state.acceleration;
    vehicle_state.velocity = vehicle_state.velocity + t1*vehicle_state.acceleration;

    vinst = (vehicle_state.raw_altitude - araw_last) / dt;
    ainst = (vinst - vinst_last) / dt;

    vinst_last = vinst;
    araw_last = vehicle_state.raw_altitude;
    
    if (!baro_error && (abs(ainst) < 32000.0)){
        E = vehicle_state.raw_altitude - vehicle_state.altitude;
    
        vehicle_state.altitude = vehicle_state.altitude + K1 * E;
        vehicle_state.velocity = vehicle_state.velocity + K2 * E;
        vehicle_state.acceleration = vehicle_state.acceleration + K3 * E;
    
    
        if (vehicle_state.altitude > vehicle_state.max_altitude){
            vehicle_state.max_altitude = vehicle_state.altitude;
        }
    }

//    if (vehicle_state.acceleration > accelLimit){
//      vehicle_state.acceleration = accelLimit;
//    }
//
//    if (vehicle_state.acceleration < -accelLimit){
//      vehicle_state.acceleration = -accelLimit;
//    }


    if (launchDetect && boost) {
      if (vehicle_state.velocity > 50.0 && vehicle_state.acceleration < -32.2){
        boost = false;
        timeToBurnout = data_time;
        K1 = K1_coast;
        K2 = K2_coast;
        K3 = K3_coast;
      }
    }

    vehicle_state.velocity_filt = (vehicle_state.velocity_filt * (20.0 - 1.0) + vehicle_state.velocity) / 20.0;

    if (!armedForLanding && (vehicle_state.velocity_filt < -2.0)) {
        armedForLanding = true;
        minLandingTime = data_time + 15.0;
    }

    if (armedForLanding && (vehicle_state.velocity_filt > -2.0) && (data_time > minLandingTime) && (vehicle_state.altitude < 50.0)) {
        landed = true;
    }
        

}

void vehicle_init(void){
    vehicle_state.pressure = 101.325;
    vehicle_state.ref_pressure = 101.325;
    vehicle_state.altitude = 0.0;
    vehicle_state.ref_altitude = 0.0;
    vehicle_state.max_altitude = 0.0;
    vehicle_state.raw_altitude = 0.0;
    vehicle_state.velocity = 0.0;
    vehicle_state.velocity_filt = 0.0;
    vehicle_state.max_velocity = 0.0;
    vehicle_state.acceleration = 0.0;
    vehicle_state.temp = 59.0;
}

void vehicle_reset(void){
    vehicle_state.altitude = 0.0;
    vehicle_state.max_altitude = 0.0;
    vehicle_state.raw_altitude = 0.0;
    vehicle_state.velocity = 0.0;
    vehicle_state.velocity_filt = 0.0;
    vehicle_state.max_velocity = 0.0;
    vehicle_state.acceleration = 0.0;
}

void send_file_header(void){

    
    unsigned char packet[9] = {0};
    unsigned char chk = 0;

    packet[0] = 0xf5;
    packet[1] = 0x03;
    packet[2] = 0x04;
    packet[3] = 0xfc;

    union data_address file_length_tx;
    file_length_tx.address_int = file_length;

    packet[4] = file_length_tx.address_string[0];
    chk = packet[4];
    packet[5] = file_length_tx.address_string[1];
    chk = chk + packet[5];
    packet[6] = file_length_tx.address_string[2];
    chk = chk + packet[6];
    packet[7] = file_length_tx.address_string[3];
    chk = chk + packet[7];

    packet[8] = chk;


    int err_code = 0;
    uint16_t length = 9;
    err_code = ble_nus_data_send(&m_nus, &packet[0], &length, m_conn_handle);

}

void send_file_eof(void){

    
    unsigned char packet[4] = {0};
    unsigned char chk = 0;

    packet[0] = 0xf5;
    packet[1] = 0x05;
    packet[2] = 0x00;
    packet[3] = 0xfa;

    int err_code = 0;
    uint16_t length = 4;
    err_code = ble_nus_data_send(&m_nus, &packet[0], &length, m_conn_handle);

    if (err_code != NRF_SUCCESS){
        while (err_code != NRF_SUCCESS){
            nrf_delay_ms(5);
            err_code = ble_nus_data_send(&m_nus, &packet[0], &length, m_conn_handle);
            //APP_ERROR_CHECK(err_code);

        }

    }

}

union data_packet1 {
    unsigned char data_string[17];
    struct data_struct {
        unsigned char start;
        unsigned char type;
        unsigned char data_length;
        unsigned char head_chk;
        float temp;
        float alt;
        float maxAlt;
        unsigned char data_chk;
    }data;
};

void send_update_packet(void){

    union data_packet1 data_packet;

    data_packet.data.start =        0xf5;
    data_packet.data.type =         0x01;
    data_packet.data.data_length =  0x0c;
    data_packet.data.head_chk =     0x02;

    data_packet.data.alt = vehicle_state.altitude;
    data_packet.data.maxAlt = vehicle_state.max_altitude;
    data_packet.data.temp = vehicle_state.temp;

    data_packet.data.data_chk = 0;

    int i;

    for (i=4; i<16; i++){
    
        data_packet.data.data_chk = data_packet.data.data_chk + data_packet.data_string[i];

    }

    int err_code = 0;
    uint16_t length = 17;
    err_code = ble_nus_data_send(&m_nus, &data_packet.data_string[0], &length, m_conn_handle);
}

void parsePacket_typeF1(void){ // set zero alt
    arm_request = true;

    
    
}


void start_data_recording(void){

    record_data = true;
    file_length = 0;
    data_time = 0.0;
    store_recording_started();

}


void parsePacket_typeF2(void){ // start data record
    start_data_recording();
    armedForLaunch = false;
}



void parsePacket_typeF3(void){

}

void parsePacket_typeF4(void){ // download data

    send_file_header();

    int num_packets = file_length / 4;
    unsigned int i = 0;
    unsigned char packet[9] = {0};
    unsigned char chk = 0;

    packet[0] = 0xf5;
    packet[1] = 0x04;
    packet[2] = 0x04;
    packet[3] = 0xfd;

    union float_bytes data;

    for (i=0; i<num_packets; i++){
        
        
        data.data = read_float(i*4);

        packet[4] = data.float_string[0];
        chk = packet[4];
        packet[5] = data.float_string[1];
        chk = chk + packet[5];
        packet[6] = data.float_string[2];
        chk = chk + packet[6];
        packet[7] = data.float_string[3];
        chk = chk + packet[7];

        packet[8] = chk;


        int err_code = 0;
        uint16_t length = 9;
        err_code = ble_nus_data_send(&m_nus, &packet[0], &length, m_conn_handle);
        //APP_ERROR_CHECK(err_code);

        if (err_code != NRF_SUCCESS){
            while (err_code != NRF_SUCCESS){
                nrf_delay_ms(5);
                err_code = ble_nus_data_send(&m_nus, &packet[0], &length, m_conn_handle);
                //APP_ERROR_CHECK(err_code);

            }

        }


    }

    send_file_eof();

    

}

void arm_system(void){


    record_data = false;
    erase_data();
    nrf_delay_ms(100);
    read_baro();
    vehicle_init();
    nrf_delay_ms(100);
    read_baro();
    vehicle_state.ref_pressure = vehicle_state.pressure;

    vehicle_state.ref_altitude = vehicle_state.ref_pressure/101.325;
    vehicle_state.ref_altitude = pow(vehicle_state.ref_altitude,0.190284);
    vehicle_state.ref_altitude = 1.0 - vehicle_state.ref_altitude;
    vehicle_state.ref_altitude = vehicle_state.ref_altitude * 145366.45;

    uint8_t index = 0;

    for (index=0; index<64; index++){
        buffer[index].altitude = vehicle_state.altitude;
        buffer[index].pressure = vehicle_state.pressure;
        buffer[index].velocity = 0;
    }
    bufferStart = 0;

//    vehicle_state.raw_altitude = 0.0;
    file_length = 0;
    data_time = 0.0;
    timeToBurnout = 0.0;
    armedForLaunch = true;
    launchDetect = false;
    armedForLanding = false;
    landed = false;
    isIdle = false;

    K1 = K1_boost;
    K2 = K2_boost;
    K3 = K3_boost;
//    nrf_delay_ms(MAIN_LOOP_PERIOD);


}

void update_batt(void){
    unsigned char packet[7] = {0};
    unsigned char chk = 0;

    packet[0] = 0xf5;
    packet[1] = 0x02;
    packet[2] = 0x02;
    packet[3] = 0xf9;

    uint16_t batt_v;
    batt_v = GetBatteryVoltage1();

    packet[5] = batt_v >> 8;
    packet[4] = batt_v;

    chk = packet[4];
    chk = chk + packet[5];

    packet[6] = chk;

    int err_code = 0;
    uint16_t length = 7;
    err_code = ble_nus_data_send(&m_nus, &packet[0], &length, m_conn_handle);

}

void send_status_packet(){

    struct status status;
    status.isRecording = record_data;
    status.isArmedForLaunch = armedForLaunch;

    union status_bytes status_bytes;

    status_bytes.status = status;

    unsigned char packet[6] = {0};
    unsigned char chk = 0;

    packet[0] = 0xf5;
    packet[1] = 0x06;
    packet[2] = 0x01;
    packet[3] = 0xfc;

    packet[4] = status_bytes.bytes[0];
    packet[5] = status_bytes.bytes[0];

    int err_code = 0;
    uint16_t length = 6;
    err_code = ble_nus_data_send(&m_nus, &packet[0], &length, m_conn_handle);

}

int8_t bmp388_set_forced_mode_with_osr(struct bmp3_dev *dev)
{
    int8_t rslt;
    /* Used to select the settings user needs to change */
    uint16_t settings_sel;

    /* Select the pressure and temperature sensor to be enabled */
    dev->settings.press_en = BMP3_ENABLE;
    dev->settings.temp_en = BMP3_ENABLE;
    /* Select the oversampling settings for pressure and temperature */
    dev->settings.odr_filter.press_os = BMP3_OVERSAMPLING_8X;
    dev->settings.odr_filter.temp_os = BMP3_NO_OVERSAMPLING;
    dev->settings.odr_filter.iir_filter = BMP3_IIR_FILTER_DISABLE;
    /* Assign the settings which needs to be set in the sensor */
    settings_sel = BMP3_PRESS_EN_SEL | BMP3_TEMP_EN_SEL | BMP3_PRESS_OS_SEL | BMP3_TEMP_OS_SEL;
    /* Write the settings in the sensor */
    rslt = bmp3_set_sensor_settings(settings_sel, dev);

    /* Select the power mode */
    dev->settings.op_mode = BMP3_FORCED_MODE;
    /* Set the power mode in the sensor */
    rslt = bmp3_set_op_mode(dev);

    return rslt;
}

void buffer_data(void){
    buffer[bufferStart].pressure = vehicle_state.pressure;
    buffer[bufferStart].altitude = vehicle_state.altitude;
    buffer[bufferStart].velocity = vehicle_state.velocity;

    bufferStart = bufferStart + 1;

    if (bufferStart == 64){
      bufferStart = 0;
    }
}


/**@brief Application main function.
 */
int main(void)
{
    

    bool erase_bonds;



    // Initialize.
//    uart_init();
//    log_init();
    timers_init();

    uint64_t dev_id = *((uint64_t*) NRF_FICR->DEVICEADDR);
    char dev_str[10];
    sprintf(dev_str, "%i", dev_id);

    strncpy(DEVICE_NAME, "FlightSketch--", 14);
    DEVICE_NAME[14] = dev_str[0];
    DEVICE_NAME[15] = dev_str[1];
    DEVICE_NAME[16] = dev_str[2];
    DEVICE_NAME[17] = dev_str[3];
    DEVICE_NAME[18] = dev_str[4];
    DEVICE_NAME[19] = dev_str[5];
    DEVICE_NAME[20] = dev_str[6];
    DEVICE_NAME[21] = dev_str[7];
    DEVICE_NAME[22] = dev_str[8];
    DEVICE_NAME[23] = dev_str[9];
    DEVICE_NAME[24] = dev_str[10];

    Adc12bitPolledInitialise();
    uint16_t batt_v;
    bool batt_read = false;
    batt_v = GetBatteryVoltage1();
    batt_read = true;

//    buttons_leds_init(&erase_bonds);
    power_management_init();
    ble_stack_init();
    sd_power_dcdc_mode_set(NRF_POWER_DCDC_ENABLE);
    gap_params_init();
    gatt_init();
    services_init();
    advertising_init();
    conn_params_init();

    // Start execution.
//    printf("\r\nUART started.\r\n");
//    NRF_LOG_INFO("Debug logging for UART over RTT started.");
    advertising_start();
    uint32_t result = 0;
    result = sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_ADV,0,4);

    nrf_gpio_cfg_output(29);
    nrf_gpio_pin_set(29);
    nrf_gpio_cfg_output(9);
    nrf_gpio_pin_set(9);
    spi_init();

    //bmp280_config();

    bmp388.dev_id = 0;
    bmp388.intf = BMP3_SPI_INTF;
    bmp388.read = BMP_280_read;
    bmp388.write = BMP_280_write;
    bmp388.delay_ms = nrf_delay_ms;

    rslt = bmp3_init(&bmp388);
    rslt = bmp388_set_forced_mode_with_osr(&bmp388);
    nrf_delay_ms(100);
    bmp388_read();

    vehicle_init();
    vehicle_state.max_altitude = 0.0;
    nrf_delay_ms(100);
    read_baro();
    nrf_delay_ms(100);
    vehicle_state.ref_pressure = vehicle_state.pressure;

    vehicle_state.ref_altitude = vehicle_state.ref_pressure/101.325;
    vehicle_state.ref_altitude = pow(vehicle_state.ref_altitude,0.190284);
    vehicle_state.ref_altitude = 1.0 - vehicle_state.ref_altitude;
    vehicle_state.ref_altitude = vehicle_state.ref_altitude * 145366.45;

    K1 = K1_boost;
    K2 = K2_boost;
    K3 = K3_boost;

    log_dt = dt*((float) (save_update_int + 1));
    t1 = dt;
    t2 = 0.5*dt*dt;

    isIdle = true;

    uint8_t started;
    uint8_t finished;
    unsigned int tmp_file_length;

    started = read_rec_start();
    finished = read_rec_finished();

    if (started == 0x01 && finished == 0xFF){
      scan_for_eof();
    }
    else {
        read_file_length();
    }

    application_timers_start();

    nrf_gpio_cfg_output(17);
    nrf_gpio_pin_clear(17);

    nrf_delay_ms(2000);

    nrf_gpio_pin_set(17);

    nrf_gpio_cfg_output(8);
    nrf_gpio_cfg_output(7);
    

    idle_state_handle();

    // Enter main loop.

    uint8_t led1_period = 200;
    uint8_t led1_dc = 5;
    uint8_t led1_counter = 0;
    bool led1_on = false;


    while(1){   

        if (download_request){
            download_request = false;
            parsePacket_typeF4();
        }

        if (arm_request){
            arm_request = false;
            arm_system();
            nrf_gpio_pin_clear(17);
            led1_on = true;
            

        }

        if (main_loop_update){
            main_loop_update = false;

            if (armedForLaunch) {
                uint8_t index = 0;
                uint8_t buffer_address;

                vehicle_state.ref_pressure = 0;

                for (index=0; index<10; index++){
                    buffer_address = bufferStart + 1 + index;
                    if (buffer_address > 63) {
                        buffer_address = 0;
                    }
                    vehicle_state.ref_pressure = vehicle_state.ref_pressure + buffer[buffer_address].pressure;
                }

                vehicle_state.ref_pressure = vehicle_state.ref_pressure * 0.1;


                vehicle_state.ref_pressure = buffer[buffer_address].pressure;
                vehicle_state.ref_altitude = vehicle_state.ref_pressure/101.325;
                vehicle_state.ref_altitude = pow(vehicle_state.ref_altitude,0.190284);
                vehicle_state.ref_altitude = 1.0 - vehicle_state.ref_altitude;
                vehicle_state.ref_altitude = vehicle_state.ref_altitude * 145366.45;
            }

            if ((vehicle_state.velocity > 30.0) && (vehicle_state.altitude > 10.0) && (!record_data) && (file_length == 0)){
                armedForLaunch = false;
                nrf_gpio_pin_set(17);
                led1_on = false;
                launchDetect = true;
                boost = true;
                start_data_recording();
                write_buffer();
            }

            if (data_time > 540.0){
                record_data = false;
            }

            if (landed) {
                isIdle = true;
                record_data = false;
                landed = false;
                store_file_length();
                nrf_delay_us(700);
                store_recording_finished();
            }

            send_update++;
            save_update++;
            batt_update++;

            if (!isIdle){
                read_baro();
                if (baro_error){
                    vehicle_state.max_altitude = vehicle_state.max_altitude + 1.0;
                }
                update_state();
            }

            if (send_update > send_update_int){
                NRF_LOG_INFO("send update");
                send_update = 0;
                send_update_packet();
                send_status_packet();
            }
            
            if (save_update > save_update_int){
                save_update = 0;
                if (record_data){
                    write_data(file_length);
                }
                else{
                    buffer_data();
                }
            }

            if (batt_update > batt_update_int){
                batt_update = 0;
                update_batt();
            }

            if (led1_on) {
                led1_counter = led1_counter + 1;
                if (led1_counter > led1_dc){
                    nrf_gpio_pin_set(17);
                }
                if (led1_counter > led1_period) {
                    led1_counter = 0;
                    nrf_gpio_pin_clear(17);
                }
            }

        }

        idle_state_handle();
    }
}


/**
 * @}
 */
