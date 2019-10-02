/******************************************************************************
 * @file LoRaMacHelper.h
 * @author  Insight SiP
 * @version V2.2.0
 * @date    14-may-2019 
 * @brief LoRaMacHelper declaration file.
 *
 * @attention
 *	THIS SOFTWARE IS PROVIDED BY INSIGHT SIP "AS IS" AND ANY EXPRESS
 *	OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 *	OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *	DISCLAIMED. IN NO EVENT SHALL INSIGHT SIP OR CONTRIBUTORS BE
 *	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 *	GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 *	OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/

#ifndef __LORAMACHELPER_H__
#define __LORAMACHELPER_H__

#include "stdint.h"
#include "Commissioning.h"
#include "board.h"
#include "LoRaMac.h"
#include "Region.h"


#define LORAWAN_CONFIRMED_MSG_ON 	0               /**< LoRaWAN confirmed messages */
#define LORAWAN_CERTIF_PORT 		224             /**< LoRaWAN certification port */
#define LORAWAN_APP_PORT		2 		/**< LoRaWAN application port, do not use 224. It is reserved for certification */
#define LORAWAN_APP_DATA_MAX_SIZE       242             /**< LoRaWAN User application data buffer size*/
#define LORAWAN_DEFAULT_DATARATE        DR_0            /**< LoRaWAN Default datarate*/
#define LORAWAN_DEFAULT_TX_POWER        TX_POWER_0      /**< LoRaWAN Default tx power*/

typedef struct lmh_param_s
{
    bool    adr_enable;                 /**< Activation state of adaptative Datarate */
    int8_t  tx_data_rate;               /**< Uplink datarate, if AdrEnable is off */
    bool    enable_public_network;	/**< Enable or disable a public network */
    int8_t tx_power;                    /**< Uplink power */
} lmh_param_t;

typedef enum
{
    LMH_RESET = 0,
    LMH_SET = !LMH_RESET
} lmh_join_status;

// Forward declaration of the LoRaMacStatus_t type.
typedef LoRaMacStatus_t lmh_error_code_t;

// Forward declaration of the ChannelParams_t type.
typedef ChannelParams_t lmh_channel_param_t;


typedef enum 
{
    LMH_UNCONFIRMED_MSG = 0, 
    LMH_CONFIRMED_MSG = !LMH_UNCONFIRMED_MSG
} lmh_confirm;

/**@brief Application Data structure
 */
typedef struct
{ 
    uint8_t* buffer; 		/**< point to the tx data buffer */ 
    uint8_t buffsize; 		/**< tx data buffer size */
    uint8_t port; 		/**< Port on which the data is sent */ 
    uint8_t confirmed;          /**< =1 if frame must be confirmed */ 
} lmh_app_tx_data_t;

/**@brief Application Data structure
 */
typedef struct
{ 
    uint8_t* buffer; 		/**< point to the LoRa rx data buffer */ 
    uint8_t buffsize; 		/**< rx data buffer size */
    uint8_t port; 		/**< Port on which the data is received */ 
    int16_t rssi;
    uint8_t snr;
} lmh_app_rx_data_t;

/**@brief LoRaMac Helper Callbacks
 */
typedef struct lmh_callback_s
{
/**@brief Get the current battery level
 * @retval value  	battery level ( 0: very low, 254: fully charged )
 */
    uint8_t (*BoardGetBatteryLevel) (void);

/**@brief Gets the board 64 bits unique ID 
 * @param[in] id	Pointer to an array that will contain the Unique ID
 */
    void (*BoardGetUniqueId) (uint8_t *id);

/**@brief Returns a pseudo random seed generated using the MCU Unique ID
 * @retval seed Generated pseudo random seed
 */
    uint32_t (*BoardGetRandomSeed) (void);
	
/**@brief Process Rx Data received from Lora network 
 * @param[in] AppData 	Rx structure
 */
    void (*lmh_RxData) (lmh_app_rx_data_t *appdata);
    
/**@brief callback indicating EndNode has just joined
 */
    void (*lmh_has_joined) (void);
    
/**@brief Confirms the class change 
 * @param[in] Class A, B, or C
 */
    void (*lmh_ConfirmClass) (DeviceClass_t Class);

/**@brief callback indicating that the node received an acknowlege from the network (following a confirmed message) 
 */
    void (*lmh_RxAck) (void);
  
} lmh_callback_t;

/**@brief LoRaWAN compliance tests support data
 */
typedef struct LoraMacHelper_ComplianceTest_s
{
    bool running;
    uint8_t state;
    bool is_tx_confirmed;
    uint8_t app_port;
    uint8_t data_buffer_size;
    uint8_t data_buffer[64];
    uint16_t downlink_counter;
    bool link_check;
    uint8_t demod_margin;
    uint8_t nb_gateways;
} lmh_compliance_test_t;

/**@brief LoWaWan stack Initialisation
 *
 * @param[in] callbacks	Pointer to structure containing the callback functions
 * @param[in] LoRaParam	Pointer to structure containing the parameters
 */
lmh_error_code_t lmh_init (lmh_callback_t *callbacks);

/**
 * @brief  Set application EUI
 * @param[in]  pointer to provided appEUI
 */
void lmh_appEui_set (uint8_t *appEui);

/**
 * @brief  Get application EUI
 * @param[out]   pointer to appEUI
 */
void lmh_appEui_get (uint8_t *appEui);

/**
 * @brief  Set the application session key
 * @param[in]  pointer to provided appKey
 */
void lmh_appKey_set (uint8_t *appKey);

/**
 * @brief  Get the application session key
 * @param[out]  pointer to appKey
 */
void lmh_appKey_get  (uint8_t *appKey);

/**
 * @brief  Set the network session key
 * @param[in]  pointer to provided nwkSKey
 */
void lmh_nwkSKey_set (uint8_t *nwkSKey);

/**
 * @brief  Get the network session key
 * @param[out]  pointer to nwkSKey
 */
void lmh_nwkSKey_get  (uint8_t *nwkSKey);

/**
 * @brief  Set the application session key
 * @param[in]  pointer to provided appSKey
 */
void lmh_appSKey_set (uint8_t *appSKey);

/**
 * @brief  Get the application session key
 * @param[out]  pointer to appSKey
 */
void lmh_appSKey_get (uint8_t *appSKey);

/**
 * @brief  Set the device address
 * @param[in]  pointer to provided devAddr
 */
void lmh_devAddr_set (uint32_t devAddr);

/**
 * @brief  Get the device address
 * @param[out]  pointer to devAddr
 */
void lmh_devAddr_get (uint32_t *devAddr);

/**
 * @brief  Set device EUI
 * @param[in]  pointer to the provided devEui
 */
void lmh_devEui_set (uint8_t *devEui);

/**
 * @brief  Get device EUI
 * @param[out]  pointer to devEui
 */
void lmh_devEui_get (uint8_t *devEui);

/**
 * @brief  Set the Network ID
 * @param[in]  pointer to the provided netId
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_netId_set (uint32_t netId);

/**
 * @brief  Get the Network ID
 * @param[out]  pointer to the netId
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_netId_get (uint32_t *netId);

/**
 * @brief  Set the adaptative data rate
 * @param[in]  pointer to the provided adrEnable
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_adaptativeDataRate_set (uint8_t adrEnable);

/**
 * @brief  Get the adaptative data rate
 * @param[out]  pointer to the adrEnable
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_adaptativeDataRate_get (uint8_t *adrEnable);

/**@brief Set Lora Class
 * @Note Only switch from class A to class B/C OR from  class B/C to class A is allowed
 * @param[in] classRequest
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_class_set (uint8_t classRequest);

/**@brief get the current Lora Class
 * @param[in] DeviceClass_t NewClass
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_class_get (uint8_t *currentClass);

/**@brief Join a Lora Network
 * @param[in] activation mode (true=OTAA, false=ABP)
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_join (bool otaa_mode);

/**@brief Check if the network is joined
 * @param[out] 1 if joined, 0 if not
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_joinStatus_get (uint8_t *join_status);

/**@brief Send data
 * @param[in] app_data Pointer to data structure to be sent
 * @param[in] is_txconfirmed do we need confirmation?
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_send (lmh_app_tx_data_t* app_data);

/**@brief Set the data rate
 * @param[in] data_rate data rate
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_datarate_set (uint8_t data_rate);

/**@brief Get the data rate
 * @param[out] pointer to data_rate
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_datarate_get (uint8_t *data_rate);

/**@brief Set the join accept delay 1
 * @param[in] delay
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_joinAcceptDelay1_set (uint32_t delay);

/**@brief Get the join accept delay 1
 * @param[out] pointer to delay
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_joinAcceptDelay1_get  (uint32_t *delay);

/**@brief Set the join accept delay 2
 * @param[in] delay
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_joinAcceptDelay2_set (uint32_t delay);

/**@brief Get the join accept delay 2
 * @param[out] pointer to delay
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_joinAcceptDelay2_get  (uint32_t *delay);

/**@brief Set the public Network mode
 * @param[in] public_mode
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_publicNetwork_set (uint8_t public_mode);

/**@brief Get the public Network mode
 * @param[out] pointer to public_mode
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_publicNetwork_get (uint8_t *public_mode);

/**@brief Set the rx delay 1
 * @param[in] delay
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_rxDelay1_set (uint32_t delay);

/**@brief Get the rx delay 1
 * @param[out] pointer to delay
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_rxDelay1_get  (uint32_t *delay);

/**@brief Set the rx delay 2
 * @param[in] delay
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_rxDelay2_set (uint32_t delay);

/**@brief Get the rx delay 2
 * @param[out] pointer to delay
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_rxDelay2_get  (uint32_t *delay);

/**@brief Set the rx data rate 2
 * @param[in] dr
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_rxDataRate2_set (uint8_t dr);

/**@brief Get the rx data rate 2
 * @param[out] pointer to dr
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_rxDataRate2_get  (uint8_t *dr);

/**@brief Set the rx frequency 2
 * @param[in] freq
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_rxFrequency2_set (uint32_t freq);

/**@brief Get the rx frequency 2
 * @param[out] pointer to freq
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_rxFrequency2_get  (uint32_t *freq);

/**@brief Set the tx power
 * @param[in] tx_power
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_txPower_set (uint8_t tx_power);

/**@brief Get the tx power
 * @param[out] pointer to tx_power
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_txPower_get  (uint8_t *tx_power);

/**@brief Set the duty cycle
 * @param[in] duty_cycle
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_dutyCycle_set (bool duty_cycle);

/**@brief Get the duty cycle
 * @param[out] pointer to duty_cycle
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_dutyCycle_get  (bool *duty_cycle);

/**@brief Get the uplink and downling counters
 * @param[out] pointer to up_counter
 * @param[out] pointer to down_counter
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_counter_get (uint8_t *up_counter, uint8_t *down_counter);

/**@brief Set a channel info
 * @param[in] id                id of the channel
 * @param[in] channel_param     Parameters of the channel, frequency, dr min and  dr max
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_channel_set (uint8_t id, lmh_channel_param_t channel_param);


/**@brief Get all channel info
 * @param[out] channel_param     Parameters of the channel, frequency, dr min and  dr max
 * @retval lmh_error_code_t
 */
lmh_error_code_t lmh_channels_get (lmh_channel_param_t *channel_param);

#endif
