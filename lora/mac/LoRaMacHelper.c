/******************************************************************************
 * @file LoRaMacHelper.c
 * @author  Insight SiP
 * @version V2.3.0
 * @date    14-june-2019 
 * @brief LoRaMacHelper implementation file.
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
 
#include "LoRaMacHelper.h"
#include "nrf_delay.h"

//logs
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#define LMH_VERIFY_SUCCESS(err_code)        \
do                                          \
{                                           \
    if (!(err_code == LORAMAC_STATUS_OK))   \
    {                                       \
        return err_code;                    \
    }                                       \
} while (0)

#if defined (REGION_EU868)
#include "LoRaMacTest.h"
#define LORAWAN_DUTYCYCLE_ON                        true 	/**< LoRaWAN ETSI duty cycle control enable/disable. Please note that ETSI mandates duty cycled transmissions. Use only for test purposes */
#define USE_SEMTECH_DEFAULT_CHANNEL_LINEUP          0
#if (USE_SEMTECH_DEFAULT_CHANNEL_LINEUP == 1)
#define LC4                { 867100000, 0, { ( (DR_5 << 4) | DR_0 ) }, 0 }
#define LC5                { 867300000, 0, { ( (DR_5 << 4) | DR_0 ) }, 0 }
#define LC6                { 867500000, 0, { ( (DR_5 << 4) | DR_0 ) }, 0 }
#define LC7                { 867700000, 0, { ( (DR_5 << 4) | DR_0 ) }, 0 }
#define LC8                { 867900000, 0, { ( (DR_5 << 4) | DR_0 ) }, 0 }
#define LC9                { 868800000, 0, { ( (DR_7 << 4) | DR_7 ) }, 2 }
#define LC10               { 868300000, 0, { ( (DR_6 << 4) | DR_6 ) }, 1 }
#endif
#endif

#define LORAWAN_ADR_ON                  1                   /**< LoRaWAN Adaptive Data Rate enabled (the end-device should be static here). */
#define LORAWAN_ADR_OFF            	0                   /**< LoRaWAN Adaptive Data Rate disabled. */

static char strlog1[64];
static char strlog2[64];
static char strlog3[64];

static uint8_t m_devEui[]   = LORAWAN_DEVICE_EUI;           /**< End-device identifier */
static uint8_t m_appEui[]   = LORAWAN_APPLICATION_EUI;      /**< Application identifier */
static uint8_t m_appKey[]   = LORAWAN_APPLICATION_KEY;      /**< Application key */
static uint8_t m_nwkSKey[]  = LORAWAN_NWKSKEY;              /**< Network session key */
static uint8_t m_appSKey[]  = LORAWAN_APPSKEY;              /**< Application session key */
static uint32_t m_devAddr   = LORAWAN_DEVICE_ADDRESS;       /**< End-device address */
static uint32_t m_netID     = LORAWAN_NETWORK_ID;           /**< Network ID */

static MlmeReqJoin_t JoinParameters;

static LoRaMacPrimitives_t LoRaMacPrimitives;               /**< LoRaMAC events variable */
static LoRaMacCallback_t LoRaMacCallbacks;                  /**< LoRaMAC callback variable */
static MibRequestConfirm_t mibReq;                          /**< LoRaMAC MIB-RequestConfirm variable */

static lmh_callback_t *m_callbacks;
static lmh_compliance_test_t m_compliance_test;             /**< LoRaWAN compliance tests data */
static lmh_param_t m_param = {LORAWAN_ADR_ON, LORAWAN_DEFAULT_DATARATE, LORAWAN_PUBLIC_NETWORK, LORAWAN_DEFAULT_TX_POWER};

static bool m_adr_enable_init;
static TimerEvent_t ComplianceTestTxNextPacketTimer;
static TimerEvent_t TxNextPacketTimer;                      /**< Timer to handle the application data transmission duty cycle */
static bool m_duty_cycle;                            

static bool compliance_test_tx (void)
{
    McpsReq_t mcpsReq;
    LoRaMacTxInfo_t txInfo;
  
    if (m_compliance_test.link_check == true)
    {
        m_compliance_test.link_check = false;
        m_compliance_test.data_buffer_size = 3;
        m_compliance_test.data_buffer[0] = 5;
        m_compliance_test.data_buffer[1] = m_compliance_test.demod_margin;
        m_compliance_test.data_buffer[2] = m_compliance_test.nb_gateways;
        m_compliance_test.state = 1;
    }
    else
    {
        switch (m_compliance_test.state)
        {
            case 4:
                m_compliance_test.state = 1;
                break;

            case 1:
                m_compliance_test.data_buffer_size = 2;
                m_compliance_test.data_buffer[0] = m_compliance_test.downlink_counter >> 8;
                m_compliance_test.data_buffer[1] = m_compliance_test.downlink_counter;
                break;
        }
    }
    
    if (LoRaMacQueryTxPossible(m_compliance_test.data_buffer_size, &txInfo) != LORAMAC_STATUS_OK)
    {
        // Send empty frame in order to flush MAC commands
        mcpsReq.Type = MCPS_UNCONFIRMED;
        mcpsReq.Req.Unconfirmed.fBuffer = NULL;
        mcpsReq.Req.Unconfirmed.fBufferSize = 0;
        mcpsReq.Req.Unconfirmed.Datarate = LORAWAN_DEFAULT_DATARATE;
    }
    else
    {
        if (m_compliance_test.is_tx_confirmed == LMH_UNCONFIRMED_MSG)
        {
            mcpsReq.Type = MCPS_UNCONFIRMED;
            mcpsReq.Req.Unconfirmed.fPort = LORAWAN_CERTIF_PORT;
            mcpsReq.Req.Unconfirmed.fBufferSize = m_compliance_test.data_buffer_size;
            mcpsReq.Req.Unconfirmed.fBuffer = &(m_compliance_test.data_buffer);
            mcpsReq.Req.Unconfirmed.Datarate = LORAWAN_DEFAULT_DATARATE;
        }
        else
        {
            mcpsReq.Type = MCPS_CONFIRMED;
            mcpsReq.Req.Confirmed.fPort = LORAWAN_CERTIF_PORT;
            mcpsReq.Req.Confirmed.fBufferSize = m_compliance_test.data_buffer_size;
            mcpsReq.Req.Confirmed.fBuffer = &(m_compliance_test.data_buffer);
            mcpsReq.Req.Confirmed.NbTrials = 8;
            mcpsReq.Req.Confirmed.Datarate = LORAWAN_DEFAULT_DATARATE;
        }
    }

    // certification test on-going
    TimerStart(&ComplianceTestTxNextPacketTimer);
	
    if (LoRaMacMcpsRequest(&mcpsReq) == LORAMAC_STATUS_OK)
    {
        return false;
    }
	
    return true;
}

/**@brief Function executed on TxNextPacket Timeout event
 */
static void OnComplianceTestTxNextPacketTimerEvent (void)
{
    compliance_test_tx();
}

/** @brief Function executed on TxNextPacket Timeout event
 */
static void OnTxNextPacketTimerEvent( void )
{
    MibRequestConfirm_t mibReq;
    LoRaMacStatus_t status;

    TimerStop( &TxNextPacketTimer );

    mibReq.Type = MIB_NETWORK_JOINED;
    status = LoRaMacMibGetRequestConfirm( &mibReq );

    if (status == LORAMAC_STATUS_OK)
    {
        if (mibReq.Param.IsNetworkJoined == false)
        {
            // Network not joined yet. Try to join again
            MlmeReq_t mlmeReq;
            mlmeReq.Type = MLME_JOIN;
            mlmeReq.Req.Join.DevEui = m_devEui;
            mlmeReq.Req.Join.AppEui = m_appEui;
            mlmeReq.Req.Join.AppKey = m_appKey;
            mlmeReq.Req.Join.Datarate = LORAWAN_DEFAULT_DATARATE;

            LoRaMacMlmeRequest(&mlmeReq);
        }
    }
}

/**@brief MCPS-Confirm event function
 *
 * @param[in] McpsConfirm - Pointer to the confirm structure, containing confirm attributes.
 */
static void McpsConfirm (McpsConfirm_t *mcpsConfirm)
{
    if (mcpsConfirm->Status == LORAMAC_EVENT_INFO_STATUS_OK)
    {
        switch (mcpsConfirm->McpsRequest)
        {
            case MCPS_UNCONFIRMED:
            {
                // Check Datarate
                // Check TxPower
                break;
            }

            case MCPS_CONFIRMED:
            {
                // Check Datarate
                // Check TxPower
                // Check AckReceived
                // Check NbTrials
                break;
            }

            case MCPS_PROPRIETARY:
            {
                break;
            }

            default:
                break;
        }
    }
}

/**@brief MCPS-Indication event function
 *
 * @param[in] mcpsIndication	Pointer to the indication structure, containing indication attributes.
 */
static void McpsIndication (McpsIndication_t *mcpsIndication)
{
    lmh_app_rx_data_t app_data;
	
    if (mcpsIndication->Status != LORAMAC_EVENT_INFO_STATUS_OK)
    {
        return;
    }

    switch (mcpsIndication->McpsIndication)
    {
        case MCPS_UNCONFIRMED:
        {
            break;
        }

        case MCPS_CONFIRMED:
        {
            break;
        }

        case MCPS_PROPRIETARY:
        {
            break;
        }

        case MCPS_MULTICAST:
        {
            break;
        }

        default:
            break;
    }

    // Check Multicast
    // Check Port
    // Check Datarate
    // Check FramePending
    if( mcpsIndication->FramePending == true )
    {
        // The server signals that it has pending data to be sent.
        // We schedule an uplink as soon as possible to flush the server.
        OnTxNextPacketTimerEvent( );
    }
    // Check Buffer
    // Check BufferSize
    // Check Rssi
    // Check Snr
    // Check RxSlot
    if (m_compliance_test.running == true)
    {
	 m_compliance_test.downlink_counter++;
    }

    if (mcpsIndication->AckReceived == true)
    {
        m_callbacks->lmh_RxAck();
    }

    if (mcpsIndication->RxData == true)
    {
        switch (mcpsIndication->Port)
        {
            case LORAWAN_CERTIF_PORT:
                // Compliance not started yet, start it
                if (m_compliance_test.running == false)
                {
                    // Check compliance test enable command (i)
                    if (    (mcpsIndication->BufferSize == 4) 		&&
                            (mcpsIndication->Buffer[0] == 0x01) 	&&
                            (mcpsIndication->Buffer[1] == 0x01) 	&&
                            (mcpsIndication->Buffer[2] == 0x01) 	&&
                            (mcpsIndication->Buffer[3] == 0x01) )
                    {
                        m_compliance_test.is_tx_confirmed = LMH_UNCONFIRMED_MSG;
                        m_compliance_test.data_buffer_size = 2;
                        m_compliance_test.downlink_counter = 0;
                        m_compliance_test.link_check = false;
                        m_compliance_test.demod_margin = 0;
                        m_compliance_test.nb_gateways = 0;
                        m_compliance_test.running = true;
                        m_compliance_test.state = 1;

                        MibRequestConfirm_t mibReq;
                        mibReq.Type = MIB_ADR;
                        LoRaMacMibGetRequestConfirm(&mibReq);
                        m_adr_enable_init = mibReq.Param.AdrEnable;
					  
                        mibReq.Type = MIB_ADR;
                        mibReq.Param.AdrEnable = true;
                        LoRaMacMibSetRequestConfirm(&mibReq);

#if defined (REGION_EU868)
                        LoRaMacTestSetDutyCycleOn(false);
#endif

                        TimerInit(&ComplianceTestTxNextPacketTimer, OnComplianceTestTxNextPacketTimerEvent);
                        TimerSetValue(&ComplianceTestTxNextPacketTimer,  5000); 
					
                        // confirm test mode activation
                        compliance_test_tx();			
                    }
                }
                // Compliance is started, check which stage we are at and take action
                else
                {
                    m_compliance_test.state = mcpsIndication->Buffer[0];
                    switch (m_compliance_test.state)
                    {
                        case 0: // Check compliance test disable command (ii)
                        {
                            m_compliance_test.downlink_counter = 0;
                            m_compliance_test.running = false;

                            MibRequestConfirm_t mibReq;
                            mibReq.Type = MIB_ADR;
                            mibReq.Param.AdrEnable = m_adr_enable_init;
                            LoRaMacMibSetRequestConfirm(&mibReq);
#if defined (REGION_EU868)
                            LoRaMacTestSetDutyCycleOn(LORAWAN_DUTYCYCLE_ON);
#endif
                            break;
                        }

                        case 1: // (iii, iv)
                        {
                            m_compliance_test.data_buffer_size = 2;
                            break;
                        }

                        case 2: // Enable confirmed messages (v)
                        {
                            m_compliance_test.is_tx_confirmed = LMH_CONFIRMED_MSG;
                            m_compliance_test.state = 1;
                            break;
                        }	

                        case 3:  // Disable confirmed messages (vi)
                        {
                            m_compliance_test.is_tx_confirmed = LMH_UNCONFIRMED_MSG;
                            m_compliance_test.state = 1;
                            break;
                        }
						
                        case 4: // (vii)
                        {
                            m_compliance_test.data_buffer_size = mcpsIndication->BufferSize;
                            m_compliance_test.data_buffer[0] = 4;
                            for( uint8_t i = 1; i < MIN(m_compliance_test.data_buffer_size, LORAWAN_APP_DATA_MAX_SIZE); i++ )
                            {
                                m_compliance_test.data_buffer[i] = mcpsIndication->Buffer[i] + 1;
                            }
                            break;	
                        }

                        case 5: // (viii)
                        {
                            MlmeReq_t mlmeReq;
                            mlmeReq.Type = MLME_LINK_CHECK;
                            LoRaMacMlmeRequest(&mlmeReq);
                            break;	
                        }

                        case 6: // (ix)
                        {
                            MlmeReq_t mlmeReq;

                            // Disable TestMode and revert back to normal operation
                            m_compliance_test.is_tx_confirmed = LORAWAN_CONFIRMED_MSG_ON;
                            m_compliance_test.downlink_counter = 0;
                            m_compliance_test.running = false;

                            MibRequestConfirm_t mibReq;
                            mibReq.Type = MIB_ADR;
                            mibReq.Param.AdrEnable = m_adr_enable_init;
                            LoRaMacMibSetRequestConfirm(&mibReq);
#if defined (REGION_EU868)
                            LoRaMacTestSetDutyCycleOn(LORAWAN_DUTYCYCLE_ON);
#endif
                            mlmeReq.Type = MLME_JOIN;
                            mlmeReq.Req.Join = JoinParameters;
                            LoRaMacMlmeRequest(&mlmeReq);
                            break;	
                        }

                        case 7: // (x)
                        {
                            if (mcpsIndication->BufferSize == 3)
                            {
                                    MlmeReq_t mlmeReq;
                                    mlmeReq.Type = MLME_TXCW;
                                    mlmeReq.Req.TxCw.Timeout = (uint16_t)( (mcpsIndication->Buffer[1] << 8) | mcpsIndication->Buffer[2] );
                                    LoRaMacMlmeRequest(&mlmeReq);
                            }
                            else if (mcpsIndication->BufferSize == 7)
                            {
                                MlmeReq_t mlmeReq;
                                mlmeReq.Type = MLME_TXCW_1;
                                mlmeReq.Req.TxCw.Timeout = (uint16_t)( (mcpsIndication->Buffer[1] << 8 ) | mcpsIndication->Buffer[2]);
                                mlmeReq.Req.TxCw.Frequency = (uint32_t)( (mcpsIndication->Buffer[3] << 16) | (mcpsIndication->Buffer[4] << 8) | mcpsIndication->Buffer[5] ) * 100;
                                mlmeReq.Req.TxCw.Power = mcpsIndication->Buffer[6];
                                LoRaMacMlmeRequest(&mlmeReq);
                            }
                            m_compliance_test.state = 1;
                            break;
                        }

                        default:
                            break;
                    }
                }  
                  
                if (m_compliance_test.running == false)
                {
                    // cerification test stops
                    TimerStop(&ComplianceTestTxNextPacketTimer);
                }          		
                break;
			
            default:
                app_data.port = mcpsIndication->Port;
                app_data.buffsize = mcpsIndication->BufferSize;
                app_data.buffer = mcpsIndication->Buffer;
                app_data.rssi = mcpsIndication->Rssi;
                app_data.snr = mcpsIndication->Snr;
                m_callbacks->lmh_RxData(&app_data);
                break;
        }
    }
}

/**@brief MLME-Confirm event function
 *
 * @param[in] MlmeConfirm	Pointer to the confirm structure, containing confirm attributes.
 */
static void MlmeConfirm (MlmeConfirm_t *mlmeConfirm)
{
    switch (mlmeConfirm->MlmeRequest)
    {
        case MLME_JOIN:
        {
            if (mlmeConfirm->Status == LORAMAC_EVENT_INFO_STATUS_OK)
            {
                // Status is OK, node has joined the network
                m_callbacks->lmh_has_joined();
            }
            break;
        }

        case MLME_LINK_CHECK:
        {
            if (mlmeConfirm->Status == LORAMAC_EVENT_INFO_STATUS_OK)
            {
                // Check DemodMargin
                // Check NbGateways
                if (m_compliance_test.running == true)
                {
                    m_compliance_test.link_check = true;
                    m_compliance_test.demod_margin = mlmeConfirm->DemodMargin;
                    m_compliance_test.nb_gateways = mlmeConfirm->NbGateways;
                }
            }
            break;
        }

        default:
            break;
    }
}

/**
 * @brief   MLME-Indication event function
 *
 * @param[in] mlmeIndication - Pointer to the indication structure.
 */
static void MlmeIndication( MlmeIndication_t *mlmeIndication )
{
    switch( mlmeIndication->MlmeIndication )
    {
        case MLME_SCHEDULE_UPLINK:
        {// The MAC signals that we shall provide an uplink as soon as possible
            OnTxNextPacketTimerEvent( );
            break;
        }
        default:
            break;
    }
}


lmh_error_code_t lmh_init (lmh_callback_t *callbacks)
{
    LoRaMacStatus_t error_status;
    m_callbacks = callbacks;

#if (STATIC_DEVICE_EUI != 1)
    m_callbacks->BoardGetUniqueId(m_devEui);  
#endif
	
#if (OVER_THE_AIR_ACTIVATION != 0)
    NRF_LOG_INFO("OTAA"); 
    sprintf(strlog2, "AppEui=%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X", m_appEui[0], m_appEui[1], m_appEui[2], m_appEui[3], m_appEui[4], m_appEui[5], m_appEui[6], m_appEui[7]);
    NRF_LOG_INFO("%s", (uint32_t)strlog2);
    sprintf(strlog1, "DevEui=%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X", m_devEui[0], m_devEui[1], m_devEui[2], m_devEui[3], m_devEui[4], m_devEui[5], m_devEui[6], m_devEui[7]);
    NRF_LOG_INFO("%s", (uint32_t)strlog1);
    sprintf(strlog3, "AppKey=%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X",          \
                        m_appKey[0], m_appKey[1], m_appKey[2], m_appKey[3], m_appKey[4], m_appKey[5], m_appKey[6], m_appKey[7],         \
                        m_appKey[8], m_appKey[9], m_appKey[10], m_appKey[11], m_appKey[12], m_appKey[13], m_appKey[14], m_appKey[15]);	
    NRF_LOG_INFO ("%s", (uint32_t)strlog3);
#else	
#if (STATIC_DEVICE_ADDRESS != 1)
    // Random seed initialization
    srand1(m_callbacks->BoardGetRandomSeed());
    // Choose a random device address
    m_devAddr = randr (0, 0x01FFFFFF);
#endif
    NRF_LOG_INFO("ABP"); 
    sprintf (strlog1, "DevEui=%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X", m_devEui[0], m_devEui[1], m_devEui[2], m_devEui[3], m_devEui[4], m_devEui[5], m_devEui[6], m_devEui[7]);
    NRF_LOG_INFO ("%s", (uint32_t)strlog1);
    NRF_LOG_INFO("DevAdd=%08X", m_devAddr);
    sprintf (strlog2, "NwkSKey=%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X",                                \
                        m_nwkSKey[0], m_nwkSKey[1], m_nwkSKey[2], m_nwkSKey[3], m_nwkSKey[4], m_nwkSKey[5], m_nwkSKey[6], m_nwkSKey[7],         \
                        m_nwkSKey[8], m_nwkSKey[9], m_nwkSKey[10], m_nwkSKey[11], m_nwkSKey[12], m_nwkSKey[13], m_nwkSKey[14], m_nwkSKey[15]);	
    NRF_LOG_INFO ("%s", (uint32_t)strlog2);
    sprintf (strlog3, "AppSKey=%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X",                                \
                        m_appSKey[0], m_appSKey[1], m_appSKey[2], m_appSKey[3], m_appSKey[4], m_appSKey[5], m_appSKey[6], m_appSKey[7],         \
                        m_appSKey[8], m_appSKey[9], m_appSKey[10], m_appSKey[11], m_appSKey[12], m_appSKey[13], m_appSKey[14], m_appSKey[15]);	
    NRF_LOG_INFO ("%s", (uint32_t)strlog3);
#endif 
	
    LoRaMacPrimitives.MacMcpsConfirm = McpsConfirm;
    LoRaMacPrimitives.MacMcpsIndication = McpsIndication;
    LoRaMacPrimitives.MacMlmeConfirm = MlmeConfirm;
    LoRaMacPrimitives.MacMlmeIndication = MlmeIndication;
    LoRaMacCallbacks.GetBatteryLevel = m_callbacks->BoardGetBatteryLevel;
	
    LoRaMacRegion_t region;
#if defined (REGION_AS923)
    region = LORAMAC_REGION_AS923;
#elif defined (REGION_AU915)
    region = LORAMAC_REGION_AU915;
#elif defined (REGION_CN470)
    region = LORAMAC_REGION_CN470;
#elif defined (REGION_CN779)
    region = LORAMAC_REGION_CN779;
#elif defined (REGION_EU433)
    region = LORAMAC_REGION_EU433;
#elif defined (REGION_IN865)
    region = LORAMAC_REGION_IN865;
#elif defined (REGION_EU868)
    region = LORAMAC_REGION_EU868;
#elif defined (REGION_KR920)
    region = LORAMAC_REGION_KR920;
#elif defined (REGION_US915)
    region = LORAMAC_REGION_US915;
#elif defined (REGION_US915_HYBRID)
    region = LORAMAC_REGION_US915_HYBRID;
#else
     #error "Please define a region in the compiler options."
#endif	
    error_status = LoRaMacInitialization(&LoRaMacPrimitives, &LoRaMacCallbacks, region);
    if (error_status != LORAMAC_STATUS_OK)
    {
        return error_status;
    }

    TimerInit(&TxNextPacketTimer, OnTxNextPacketTimerEvent);

    mibReq.Type = MIB_ADR;
    mibReq.Param.AdrEnable = m_param.adr_enable;
    LoRaMacMibSetRequestConfirm(&mibReq);
	
    mibReq.Type = MIB_CHANNELS_TX_POWER;
    mibReq.Param.ChannelsTxPower = m_param.tx_power;
    LoRaMacMibSetRequestConfirm(&mibReq);	

    mibReq.Type = MIB_PUBLIC_NETWORK;
    mibReq.Param.EnablePublicNetwork = m_param.enable_public_network;
    LoRaMacMibSetRequestConfirm(&mibReq);
                      
    mibReq.Type = MIB_DEVICE_CLASS;
    mibReq.Param.Class= CLASS_A;
    LoRaMacMibSetRequestConfirm(&mibReq);
	
#if defined (REGION_EU868)
    m_duty_cycle = true;
    LoRaMacTestSetDutyCycleOn(LORAWAN_DUTYCYCLE_ON);
#if (USE_SEMTECH_DEFAULT_CHANNEL_LINEUP == 1)
    LoRaMacChannelAdd (3, (ChannelParams_t)LC4);
    LoRaMacChannelAdd (4, (ChannelParams_t)LC5);
    LoRaMacChannelAdd (5, (ChannelParams_t)LC6);
    LoRaMacChannelAdd (6, (ChannelParams_t)LC7);
    LoRaMacChannelAdd (7, (ChannelParams_t)LC8);
    LoRaMacChannelAdd (8, (ChannelParams_t)LC9);
    LoRaMacChannelAdd (9, (ChannelParams_t)LC10);

    mibReq.Type = MIB_RX2_DEFAULT_CHANNEL;
    mibReq.Param.Rx2DefaultChannel = (Rx2ChannelParams_t){869525000, DR_3};
    LoRaMacMibSetRequestConfirm (&mibReq);

    mibReq.Type = MIB_RX2_CHANNEL;
    mibReq.Param.Rx2Channel = (Rx2ChannelParams_t){869525000, DR_3};
    LoRaMacMibSetRequestConfirm (&mibReq);
#endif
#else
    m_duty_cycle = false;
#endif
	
    return LORAMAC_STATUS_OK;
}

void lmh_appEui_set (uint8_t *appEui)
{
    memcpy(m_appEui, appEui, sizeof(m_appEui));
}

void lmh_appEui_get (uint8_t *appEui)
{
    memcpy(appEui, m_appEui, sizeof(m_appEui));
}

void lmh_appKey_set (uint8_t *appKey)
{
    memcpy(m_appKey, appKey, sizeof(m_appKey));
}

void lmh_appKey_get  (uint8_t *appKey)
{
    memcpy(appKey, m_appKey, sizeof(m_appKey));
}

void lmh_nwkSKey_set (uint8_t *nwkSKey)
{
    memcpy(m_nwkSKey, nwkSKey, sizeof(m_nwkSKey));
}

void lmh_nwkSKey_get (uint8_t *nwkSKey)
{
     memcpy(nwkSKey, m_nwkSKey, sizeof(m_nwkSKey));
}

void lmh_appSKey_set (uint8_t *appSKey)
{
    memcpy(m_appSKey, appSKey, sizeof(m_appSKey));
}

void lmh_appSKey_get (uint8_t *appSKey)
{
    memcpy(appSKey, m_appSKey, sizeof(m_appSKey));
}

void lmh_devAddr_set (uint32_t devAddr)
{
    m_devAddr = devAddr;
}

void lmh_devAddr_get (uint32_t *devAddr)
{
    *devAddr = m_devAddr;
}

void lmh_devEui_set (uint8_t *devEui)
{
    memcpy1(m_devEui, devEui, sizeof(m_devEui));
}

void lmh_devEui_get (uint8_t *devEui)
{
    memcpy1(devEui, m_devEui, sizeof(m_devEui));
}

lmh_error_code_t lmh_netId_set (uint32_t netId)
{
    LoRaMacStatus_t status;

    mibReq.Type = MIB_NET_ID;
    mibReq.Param.NetID = netId;
    status = LoRaMacMibSetRequestConfirm(&mibReq);
    LMH_VERIFY_SUCCESS(status);

    m_netID = netId;

    return LORAMAC_STATUS_OK;
}

lmh_error_code_t lmh_netId_get (uint32_t *netId)
{
    LoRaMacStatus_t status;

    mibReq.Type = MIB_NET_ID;
    status = LoRaMacMibGetRequestConfirm(&mibReq);
    LMH_VERIFY_SUCCESS(status);

    *netId = mibReq.Param.NetID;

    return LORAMAC_STATUS_OK;
}

lmh_error_code_t lmh_adaptativeDataRate_set (uint8_t adrEnable)
{
    LoRaMacStatus_t status;

    mibReq.Type = MIB_ADR;
    mibReq.Param.AdrEnable = adrEnable;
    status = LoRaMacMibSetRequestConfirm(&mibReq);	
    LMH_VERIFY_SUCCESS(status);

    return LORAMAC_STATUS_OK;
}

lmh_error_code_t lmh_adaptativeDataRate_get (uint8_t *adrEnable)
{
    LoRaMacStatus_t status;

    mibReq.Type = MIB_ADR;
    status = LoRaMacMibGetRequestConfirm(&mibReq);	
    LMH_VERIFY_SUCCESS(status);

    *adrEnable = mibReq.Param.AdrEnable;

    return LORAMAC_STATUS_OK;
}

lmh_error_code_t lmh_class_set (uint8_t classRequest)
{
    LoRaMacStatus_t status;
    DeviceClass_t currentClass, newClass;

    if (classRequest == 'A' || classRequest == 'a')
    {
        newClass = CLASS_A;
    }
    else if (classRequest == 'B' || classRequest == 'b')
    {
        newClass = CLASS_B;
    }
    else if (classRequest == 'C' || classRequest == 'c')
    {
        newClass = CLASS_C;
    }
  
    mibReq.Type = MIB_DEVICE_CLASS;
    LoRaMacMibGetRequestConfirm(&mibReq);
    currentClass = mibReq.Param.Class;
	
    // attempt to swicth only if class update
    if (currentClass != newClass)
    {
        // CLASS B NOT IMPLEMENTED
        if (newClass == CLASS_B)
        {
            return LORAMAC_STATUS_PARAMETER_INVALID;
        }

        mibReq.Param.Class = newClass;
        status = LoRaMacMibSetRequestConfirm(&mibReq);
        LMH_VERIFY_SUCCESS(status);
    }

    return LORAMAC_STATUS_OK;
}

lmh_error_code_t lmh_class_get (uint8_t *classRequest)
{
    LoRaMacStatus_t status;
    DeviceClass_t currentClass;
  
    mibReq.Type = MIB_DEVICE_CLASS;
    LoRaMacMibGetRequestConfirm (&mibReq);
    if (status != LORAMAC_STATUS_OK)
    {
        return status;
    }
  
    currentClass = mibReq.Param.Class;

    if (currentClass == CLASS_A)
    {
        *classRequest = 'A';
    }
    else if (currentClass == CLASS_B)
    {
        *classRequest = 'B';
    }
    else if (currentClass == CLASS_C)
    {
        *classRequest = 'C';
    }

    return LORAMAC_STATUS_OK;
}

lmh_error_code_t lmh_join (bool otaa_mode)
{
    LoRaMacStatus_t status;
    MlmeReq_t mlmeReq;
  
    mlmeReq.Type = MLME_JOIN;
    mlmeReq.Req.Join.DevEui = m_devEui;
    mlmeReq.Req.Join.AppEui = m_appEui;
    mlmeReq.Req.Join.AppKey = m_appKey;
    mlmeReq.Req.Join.Datarate = m_param.tx_data_rate;
 
    JoinParameters = mlmeReq.Req.Join;
    if (otaa_mode)  // OTAA
    {
        status = LoRaMacMlmeRequest(&mlmeReq);
        LMH_VERIFY_SUCCESS(status);
    }
    else            // ABP
    {
        mibReq.Type = MIB_NET_ID;
        mibReq.Param.NetID = m_netID;
        status = LoRaMacMibSetRequestConfirm(&mibReq);
        LMH_VERIFY_SUCCESS(status);

        mibReq.Type = MIB_DEV_ADDR;
        mibReq.Param.DevAddr = m_devAddr;
        status = LoRaMacMibSetRequestConfirm(&mibReq);
        LMH_VERIFY_SUCCESS(status);

        mibReq.Type = MIB_NWK_SKEY;
        mibReq.Param.NwkSKey = m_nwkSKey;
        status = LoRaMacMibSetRequestConfirm(&mibReq);
        LMH_VERIFY_SUCCESS(status);

        mibReq.Type = MIB_APP_SKEY;
        mibReq.Param.AppSKey = m_appSKey;
        status =  LoRaMacMibSetRequestConfirm(&mibReq);
        LMH_VERIFY_SUCCESS(status);

        mibReq.Type = MIB_NETWORK_JOINED;
        mibReq.Param.IsNetworkJoined = true;
        status = LoRaMacMibSetRequestConfirm(&mibReq);
        LMH_VERIFY_SUCCESS(status);

        m_callbacks->lmh_has_joined();
    }

    return LORAMAC_STATUS_OK;
}

lmh_error_code_t lmh_tx_power_set (uint8_t tx_power)
{	
    LoRaMacStatus_t status;

    mibReq.Type = MIB_CHANNELS_TX_POWER;
    mibReq.Param.ChannelsTxPower = tx_power;
    LoRaMacMibSetRequestConfirm(&mibReq);	
    LMH_VERIFY_SUCCESS(status);

    return LORAMAC_STATUS_OK;
}

lmh_error_code_t lmh_joinStatus_get (uint8_t *join_status)
{
    LoRaMacStatus_t status;

    mibReq.Type = MIB_NETWORK_JOINED;
    status = LoRaMacMibGetRequestConfirm(&mibReq);
    LMH_VERIFY_SUCCESS(status);

    if (mibReq.Param.IsNetworkJoined == true)
    {
        *join_status = 1;
    }
    else
    {
        *join_status = 0;
    }

    return LORAMAC_STATUS_OK;
}

lmh_error_code_t lmh_send (lmh_app_tx_data_t* app_data)
{
    McpsReq_t mcpsReq;
    LoRaMacTxInfo_t txInfo;
    LoRaMacStatus_t status;
  
    /*if certification test are on going, application data is not sent*/
    if (m_compliance_test.running == true)
    {
        return LORAMAC_STATUS_BUSY;
    }
    
    if (LoRaMacQueryTxPossible(app_data->buffsize, &txInfo) != LORAMAC_STATUS_OK)
    {
        // Send empty frame in order to flush MAC commands
        mcpsReq.Type = MCPS_UNCONFIRMED;
        mcpsReq.Req.Unconfirmed.fBuffer = NULL;
        mcpsReq.Req.Unconfirmed.fBufferSize = 0;
        mcpsReq.Req.Unconfirmed.Datarate = m_param.tx_data_rate;
    }
    else
    {
        if (app_data->confirmed == LMH_UNCONFIRMED_MSG)
        {
            mcpsReq.Type = MCPS_UNCONFIRMED;
            mcpsReq.Req.Unconfirmed.fPort = app_data->port;
            mcpsReq.Req.Unconfirmed.fBufferSize = app_data->buffsize;
            mcpsReq.Req.Unconfirmed.fBuffer = app_data->buffer;
            mcpsReq.Req.Unconfirmed.Datarate = m_param.tx_data_rate;
        }
        else
        {
            mcpsReq.Type = MCPS_CONFIRMED;
            mcpsReq.Req.Confirmed.fPort = app_data->port;
            mcpsReq.Req.Confirmed.fBufferSize = app_data->buffsize;
            mcpsReq.Req.Confirmed.fBuffer = app_data->buffer;
            mcpsReq.Req.Confirmed.NbTrials = 8;
            mcpsReq.Req.Confirmed.Datarate = m_param.tx_data_rate;
        }
    }

    status = LoRaMacMcpsRequest(&mcpsReq);
    LMH_VERIFY_SUCCESS(status);

    return LORAMAC_STATUS_OK;
}  

lmh_error_code_t lmh_datarate_set (uint8_t data_rate)
{
    m_param.tx_data_rate = data_rate;

    return LORAMAC_STATUS_OK;
}

lmh_error_code_t lmh_datarate_get (uint8_t *data_rate)
{
    *data_rate = m_param.tx_data_rate;

    return LORAMAC_STATUS_OK;
}

lmh_error_code_t lmh_joinAcceptDelay1_set (uint32_t delay)
{
    LoRaMacStatus_t status;

    mibReq.Type = MIB_JOIN_ACCEPT_DELAY_1;
    mibReq.Param.JoinAcceptDelay1 = delay;
    status = LoRaMacMibSetRequestConfirm(&mibReq);
    LMH_VERIFY_SUCCESS(status);

    return LORAMAC_STATUS_OK;
}

lmh_error_code_t lmh_joinAcceptDelay1_get (uint32_t *delay)
{
    LoRaMacStatus_t status;

    mibReq.Type = MIB_JOIN_ACCEPT_DELAY_1;
    status = LoRaMacMibGetRequestConfirm(&mibReq);
    LMH_VERIFY_SUCCESS(status);

    *delay =  mibReq.Param.JoinAcceptDelay1;

    return LORAMAC_STATUS_OK;
}

lmh_error_code_t lmh_joinAcceptDelay2_set (uint32_t delay)
{
    LoRaMacStatus_t status;

    mibReq.Type = MIB_JOIN_ACCEPT_DELAY_2;
    mibReq.Param.JoinAcceptDelay2 = delay;
    status = LoRaMacMibSetRequestConfirm(&mibReq);
    LMH_VERIFY_SUCCESS(status);

    return LORAMAC_STATUS_OK;
}

lmh_error_code_t lmh_joinAcceptDelay2_get (uint32_t *delay)
{
    LoRaMacStatus_t status;

    mibReq.Type = MIB_JOIN_ACCEPT_DELAY_2;
    status = LoRaMacMibGetRequestConfirm(&mibReq);
    LMH_VERIFY_SUCCESS(status);

    *delay =  mibReq.Param.JoinAcceptDelay2;

    return LORAMAC_STATUS_OK;
}

lmh_error_code_t lmh_publicNetwork_set (uint8_t public_mode)
{
    LoRaMacStatus_t status;

    mibReq.Type = MIB_PUBLIC_NETWORK;
    mibReq.Param.EnablePublicNetwork = public_mode;
    status = LoRaMacMibSetRequestConfirm(&mibReq);
    LMH_VERIFY_SUCCESS(status);

    return LORAMAC_STATUS_OK;
}

lmh_error_code_t lmh_publicNetwork_get (uint8_t *public_mode)
{
    LoRaMacStatus_t status;

    mibReq.Type = MIB_PUBLIC_NETWORK;
    status = LoRaMacMibGetRequestConfirm(&mibReq);
    LMH_VERIFY_SUCCESS(status);

    *public_mode =  mibReq.Param.EnablePublicNetwork;

    return LORAMAC_STATUS_OK;
}

lmh_error_code_t lmh_rxDelay1_set (uint32_t delay)
{
    LoRaMacStatus_t status;

    mibReq.Type = MIB_RECEIVE_DELAY_1;
    mibReq.Param.ReceiveDelay1 = delay;
    status = LoRaMacMibSetRequestConfirm(&mibReq);
    LMH_VERIFY_SUCCESS(status);

    return LORAMAC_STATUS_OK;
}

lmh_error_code_t lmh_rxDelay1_get (uint32_t *delay)
{
    LoRaMacStatus_t status;

    mibReq.Type = MIB_RECEIVE_DELAY_1;
    status = LoRaMacMibGetRequestConfirm(&mibReq);
    LMH_VERIFY_SUCCESS(status);

    *delay =  mibReq.Param.ReceiveDelay1;

    return LORAMAC_STATUS_OK;
}

lmh_error_code_t lmh_rxDelay2_set (uint32_t delay)
{
    LoRaMacStatus_t status;

    mibReq.Type = MIB_RECEIVE_DELAY_2;
    mibReq.Param.ReceiveDelay2 = delay;
    status = LoRaMacMibSetRequestConfirm(&mibReq);
    LMH_VERIFY_SUCCESS(status);

    return LORAMAC_STATUS_OK;
}

lmh_error_code_t lmh_rxDelay2_get (uint32_t *delay)
{
    LoRaMacStatus_t status;

    mibReq.Type = MIB_RECEIVE_DELAY_2;
    status = LoRaMacMibGetRequestConfirm(&mibReq);
    LMH_VERIFY_SUCCESS(status);

    *delay =  mibReq.Param.ReceiveDelay2;

    return LORAMAC_STATUS_OK;
}

lmh_error_code_t lmh_rxDataRate2_set (uint8_t dr)
{
    LoRaMacStatus_t status;

    mibReq.Type = MIB_RX2_CHANNEL;
    status = LoRaMacMibGetRequestConfirm(&mibReq);
    LMH_VERIFY_SUCCESS(status);

    mibReq.Param.Rx2Channel.Datarate = dr;
    status = LoRaMacMibSetRequestConfirm(&mibReq);
    LMH_VERIFY_SUCCESS(status);

    return LORAMAC_STATUS_OK;
}

lmh_error_code_t lmh_rxDataRate2_get (uint8_t *dr)
{
    LoRaMacStatus_t status;

    mibReq.Type = MIB_RX2_CHANNEL;
    status = LoRaMacMibGetRequestConfirm(&mibReq);
    LMH_VERIFY_SUCCESS(status);

    *dr =  mibReq.Param.Rx2Channel.Datarate;

    return LORAMAC_STATUS_OK;
}

lmh_error_code_t lmh_rxFrequency2_set (uint32_t freq)
{
    LoRaMacStatus_t status;

    mibReq.Type = MIB_RX2_CHANNEL;
    status = LoRaMacMibGetRequestConfirm(&mibReq);
    LMH_VERIFY_SUCCESS(status);

    mibReq.Param.Rx2Channel.Frequency = freq;
    status = LoRaMacMibSetRequestConfirm(&mibReq);
    LMH_VERIFY_SUCCESS(status);

    return LORAMAC_STATUS_OK;
}

lmh_error_code_t lmh_rxFrequency2_get (uint32_t *freq)
{
    LoRaMacStatus_t status;

    mibReq.Type = MIB_RX2_CHANNEL;
    status = LoRaMacMibGetRequestConfirm(&mibReq);
    LMH_VERIFY_SUCCESS(status);

    *freq =  mibReq.Param.Rx2Channel.Frequency;

    return LORAMAC_STATUS_OK;
}

lmh_error_code_t lmh_txPower_set (uint8_t tx_power)
{
    LoRaMacStatus_t status;

    mibReq.Type = MIB_CHANNELS_TX_POWER;
    mibReq.Param.ChannelsTxPower = tx_power;
    status = LoRaMacMibSetRequestConfirm(&mibReq);
    LMH_VERIFY_SUCCESS(status);

    return LORAMAC_STATUS_OK;
}

lmh_error_code_t lmh_txPower_get (uint8_t *tx_power)
{
    LoRaMacStatus_t status;

    mibReq.Type = MIB_CHANNELS_TX_POWER;
    status = LoRaMacMibGetRequestConfirm(&mibReq);
    LMH_VERIFY_SUCCESS(status);

    *tx_power =  mibReq.Param.ChannelsTxPower;

    return LORAMAC_STATUS_OK;
}

lmh_error_code_t lmh_dutyCycle_set (bool duty_cycle)
{   
#if defined (REGION_EU868)
    m_duty_cycle = duty_cycle;
    LoRaMacTestSetDutyCycleOn(duty_cycle);
#endif
    return LORAMAC_STATUS_OK;
}

lmh_error_code_t lmh_dutyCycle_get (bool *duty_cycle)
{
    *duty_cycle = m_duty_cycle;
     return LORAMAC_STATUS_OK;
}

lmh_error_code_t lmh_counter_get (uint8_t *up_counter, uint8_t *down_counter)
{
    LoRaMacStatus_t status;

    mibReq.Type = MIB_UPLINK_COUNTER;
    status = LoRaMacMibGetRequestConfirm(&mibReq);
    LMH_VERIFY_SUCCESS(status);

    mibReq.Type = MIB_DOWNLINK_COUNTER;
    status = LoRaMacMibGetRequestConfirm(&mibReq);
    LMH_VERIFY_SUCCESS(status);

    *up_counter =  mibReq.Param.UpLinkCounter;
    *down_counter =  mibReq.Param.DownLinkCounter;

    return LORAMAC_STATUS_OK;
}

lmh_error_code_t lmh_channel_set (uint8_t id, lmh_channel_param_t channel_param)
{
    LoRaMacStatus_t status;

    if (channel_param.Frequency != 0)
    {
        status = LoRaMacChannelAdd(id, (ChannelParams_t){ channel_param.Frequency, 0, channel_param.DrRange, 0 });
        LMH_VERIFY_SUCCESS(status);
    }
    else
    {
        status = LoRaMacChannelRemove(id);
        LMH_VERIFY_SUCCESS(status);
    }

    return LORAMAC_STATUS_OK;
}

lmh_error_code_t lmh_channels_get (lmh_channel_param_t *channel_param)
{
    LoRaMacStatus_t status;

    mibReq.Type = MIB_CHANNELS;
    status = LoRaMacMibGetRequestConfirm(&mibReq);
    LMH_VERIFY_SUCCESS(status);

   // channel_param = mibReq.Param.ChannelList;
    memcpy(channel_param, mibReq.Param.ChannelList, 16*sizeof(lmh_channel_param_t));

    return LORAMAC_STATUS_OK;
}