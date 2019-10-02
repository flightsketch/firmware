/*
  ______                              _
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2013 Semtech

Description: SX126x driver specific target board functions implementation

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Miguel Luis and Gregory Cristian
*/

/******************************************************************************
 * @file    sx126x-board.c
 * @author  Insight SiP
 * @version V2.1.0
 * @date    07-may-2019 
 * @brief   SX126x implementation compatible with LoraWan/semtech drivers.
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


#include "board.h"
//#include "radio.h" // Not needed
#include "sx126x/sx126x.h"
#include "sx126x-board.h"

void SX126xIoInit (void)
{
    GpioInit(&SX126x.Spi.Nss,	PIN_LORA_NSS,		PIN_OUTPUT, PIN_PUSH_PULL, PIN_NO_PULL, 1);
    GpioInit(&SX126x.BUSY, 	PIN_LORA_BUSY,		PIN_INPUT,  PIN_PUSH_PULL, PIN_NO_PULL, 0);
    GpioInit(&SX126x.DIO1,	PIN_LORA_DIO_1, 	PIN_INPUT,  PIN_PUSH_PULL, PIN_NO_PULL, 0);
}

void SX126xIoIrqInit (DioIrqHandler dioIrq)
{
    GpioSetInterrupt(&SX126x.DIO1, IRQ_RISING_EDGE, IRQ_HIGH_PRIORITY, dioIrq);
}

void SX126xIoDeInit (void)
{
    GpioInit(&SX126x.Spi.Nss,	PIN_LORA_NSS,		PIN_INPUT, PIN_PUSH_PULL, PIN_NO_PULL, 0);
    GpioInit(&SX126x.BUSY,	PIN_LORA_BUSY,		PIN_INPUT, PIN_PUSH_PULL, PIN_NO_PULL, 0);
    GpioInit(&SX126x.DIO1,	PIN_LORA_DIO_1, 	PIN_INPUT, PIN_PUSH_PULL, PIN_NO_PULL, 0);
}

void SX126xReset (void)
{
    DelayMs(10);
    GpioInit(&SX126x.Reset, PIN_LORA_RESET, PIN_OUTPUT, PIN_PUSH_PULL, PIN_NO_PULL, 0);
    DelayMs(20);
    GpioInit(&SX126x.Reset, PIN_LORA_RESET, PIN_INPUT, PIN_PUSH_PULL, PIN_NO_PULL, 0); // internal pull-up
    DelayMs(10);
}

void SX126xWaitOnBusy (void)
{
    while (GpioRead( &SX126x.BUSY ) == 1);
}

void SX126xWakeup (void)
{
    BoardDisableIrq();

    GpioWrite(&SX126x.Spi.Nss, 0);

    SpiInOut(&SX126x.Spi, RADIO_GET_STATUS);
    SpiInOut(&SX126x.Spi, 0x00);

    GpioWrite(&SX126x.Spi.Nss, 1);

    // Wait for chip to be ready.
    SX126xWaitOnBusy();

    BoardEnableIrq();
}

void SX126xWriteCommand (RadioCommands_t command, uint8_t *buffer, uint16_t size)
{
    SX126xCheckDeviceReady();

    GpioWrite(&SX126x.Spi.Nss, 0);

    SpiInOut(&SX126x.Spi, (uint8_t)command);

    for (uint16_t i = 0; i < size; i++)
    {
        SpiInOut(&SX126x.Spi, buffer[i]);
    }

    GpioWrite(&SX126x.Spi.Nss, 1);

    if(command != RADIO_SET_SLEEP)
    {
        SX126xWaitOnBusy();
    }
}

void SX126xReadCommand (RadioCommands_t command, uint8_t *buffer, uint16_t size)
{
    SX126xCheckDeviceReady();

    GpioWrite(&SX126x.Spi.Nss, 0);

    SpiInOut(&SX126x.Spi, (uint8_t)command);
    SpiInOut(&SX126x.Spi, 0x00);
    for (uint16_t i = 0; i < size; i++)
    {
        buffer[i] = SpiInOut(&SX126x.Spi, 0);
    }

    GpioWrite(&SX126x.Spi.Nss, 1);

    SX126xWaitOnBusy();
}

void SX126xWriteRegisters (uint16_t address, uint8_t *buffer, uint16_t size)
{
    SX126xCheckDeviceReady();

    GpioWrite(&SX126x.Spi.Nss, 0);
    
    SpiInOut(&SX126x.Spi, RADIO_WRITE_REGISTER);
    SpiInOut(&SX126x.Spi, (address & 0xFF00) >> 8);
    SpiInOut(&SX126x.Spi, address & 0x00FF);
    
    for (uint16_t i = 0; i < size; i++)
    {
        SpiInOut(&SX126x.Spi, buffer[i]);
    }

    GpioWrite (&SX126x.Spi.Nss, 1);

    SX126xWaitOnBusy();
}

void SX126xWriteRegister (uint16_t address, uint8_t value)
{
    SX126xWriteRegisters(address, &value, 1);
}

void SX126xReadRegisters (uint16_t address, uint8_t *buffer, uint16_t size)
{
    SX126xCheckDeviceReady();

    GpioWrite(&SX126x.Spi.Nss, 0);

    SpiInOut(&SX126x.Spi, RADIO_READ_REGISTER);
    SpiInOut(&SX126x.Spi, (address & 0xFF00) >> 8);
    SpiInOut(&SX126x.Spi, address & 0x00FF);
    SpiInOut(&SX126x.Spi, 0);
    for (uint16_t i = 0; i < size; i++)
    {
        buffer[i] = SpiInOut(&SX126x.Spi, 0);
    }
    GpioWrite(&SX126x.Spi.Nss, 1);

    SX126xWaitOnBusy();
}

uint8_t SX126xReadRegister (uint16_t address)
{
    uint8_t data;
    SX126xReadRegisters(address, &data, 1);
    return data;
}

void SX126xWriteBuffer (uint8_t offset, uint8_t *buffer, uint8_t size)
{
    SX126xCheckDeviceReady();

    GpioWrite(&SX126x.Spi.Nss, 0);

    SpiInOut(&SX126x.Spi, RADIO_WRITE_BUFFER);
    SpiInOut(&SX126x.Spi, offset);
    for(uint16_t i = 0; i < size; i++)
    {
        SpiInOut(&SX126x.Spi, buffer[i]);
    }
    GpioWrite(&SX126x.Spi.Nss, 1);

    SX126xWaitOnBusy();
}

void SX126xReadBuffer (uint8_t offset, uint8_t *buffer, uint8_t size)
{
    SX126xCheckDeviceReady();

    GpioWrite(&SX126x.Spi.Nss, 0);

    SpiInOut(&SX126x.Spi, RADIO_READ_BUFFER);
    SpiInOut(&SX126x.Spi, offset);
    SpiInOut(&SX126x.Spi, 0);
    for(uint16_t i = 0; i < size; i++)
    {
        buffer[i] = SpiInOut(&SX126x.Spi, 0);
    }
    GpioWrite(&SX126x.Spi.Nss, 1);

    SX126xWaitOnBusy();
}

void SX126xSetRfTxPower (int8_t power)
{
    SX126xSetTxParams(power, RADIO_RAMP_40_US);
}

uint8_t SX126xGetPaSelect (uint32_t channel)
{
#if defined(ISP4520_EU) || defined(ISP4520_JP) 
    return SX1261;
#elif defined(ISP4520_US)
    return SX1262;
#else
    #error "Please define an ISP4520 configuration"
#endif
}

uint32_t SX126xGetBoardTcxoWakeupTime( void )
{
    return BOARD_TCXO_WAKEUP_TIME;
}

static void SX126xSetDio3AsOutput (void)
{
    // Set bit 3 of register 0x0580 to 1
    uint8_t reg_0x0580;

    SX126xWaitOnBusy();
    GpioWrite(&SX126x.Spi.Nss, 0);
    SpiInOut(&SX126x.Spi, RADIO_READ_REGISTER);
    SpiInOut(&SX126x.Spi, (0x0580 & 0xFF00) >> 8);
    SpiInOut(&SX126x.Spi, 0x0580 & 0x00FF);
    SpiInOut(&SX126x.Spi, 0);
    reg_0x0580 = SpiInOut(&SX126x.Spi, 0);
    GpioWrite(&SX126x.Spi.Nss, 1);

    SX126xWaitOnBusy();
    GpioWrite(&SX126x.Spi.Nss, 0);
    SpiInOut(&SX126x.Spi, RADIO_WRITE_REGISTER);
    SpiInOut(&SX126x.Spi, (0x0580 & 0xFF00) >> 8);
    SpiInOut(&SX126x.Spi, 0x0580 & 0x00FF);
    SpiInOut(&SX126x.Spi, reg_0x0580 | 0x08);
    GpioWrite (&SX126x.Spi.Nss, 1);

    // Set bit 3 of register 0x0583 to 0
    uint8_t reg_0x0583;

    SX126xWaitOnBusy();
    GpioWrite(&SX126x.Spi.Nss, 0);
    SpiInOut(&SX126x.Spi, RADIO_READ_REGISTER);
    SpiInOut(&SX126x.Spi, (0x0583 & 0xFF00) >> 8);
    SpiInOut(&SX126x.Spi, 0x0583 & 0x00FF);
    SpiInOut(&SX126x.Spi, 0);
    reg_0x0583 = SpiInOut(&SX126x.Spi, 0);
    GpioWrite(&SX126x.Spi.Nss, 1);

    SX126xWaitOnBusy();
    GpioWrite(&SX126x.Spi.Nss, 0);
    SpiInOut(&SX126x.Spi, RADIO_WRITE_REGISTER);
    SpiInOut(&SX126x.Spi, (0x0583 & 0xFF00) >> 8);
    SpiInOut(&SX126x.Spi, 0x0583 & 0x00FF);
    SpiInOut(&SX126x.Spi, reg_0x0583 & ~0x08);
    GpioWrite (&SX126x.Spi.Nss, 1);

    // Set bit 3 of register 0x0584 to 0
    uint8_t reg_0x0584;

    SX126xWaitOnBusy();
    GpioWrite(&SX126x.Spi.Nss, 0);
    SpiInOut(&SX126x.Spi, RADIO_READ_REGISTER);
    SpiInOut(&SX126x.Spi, (0x0584 & 0xFF00) >> 8);
    SpiInOut(&SX126x.Spi, 0x0584 & 0x00FF);
    SpiInOut(&SX126x.Spi, 0);
    reg_0x0584 = SpiInOut(&SX126x.Spi, 0);
    GpioWrite(&SX126x.Spi.Nss, 1);

    SX126xWaitOnBusy();
    GpioWrite(&SX126x.Spi.Nss, 0);
    SpiInOut(&SX126x.Spi, RADIO_WRITE_REGISTER);
    SpiInOut(&SX126x.Spi, (0x0584 & 0xFF00) >> 8);
    SpiInOut(&SX126x.Spi, 0x0584 & 0x00FF);
    SpiInOut(&SX126x.Spi, reg_0x0584 & ~0x08);
    GpioWrite (&SX126x.Spi.Nss, 1);

    // Set bit 3 of register 0x0585 to 0
    uint8_t reg_0x0585;

    SX126xWaitOnBusy();
    GpioWrite(&SX126x.Spi.Nss, 0);
    SpiInOut(&SX126x.Spi, RADIO_READ_REGISTER);
    SpiInOut(&SX126x.Spi, (0x0585 & 0xFF00) >> 8);
    SpiInOut(&SX126x.Spi, 0x0585 & 0x00FF);
    SpiInOut(&SX126x.Spi, 0);
    reg_0x0585 = SpiInOut(&SX126x.Spi, 0);
    GpioWrite(&SX126x.Spi.Nss, 1);

    SX126xWaitOnBusy();
    GpioWrite(&SX126x.Spi.Nss, 0);
    SpiInOut(&SX126x.Spi, RADIO_WRITE_REGISTER);
    SpiInOut(&SX126x.Spi, (0x0585 & 0xFF00) >> 8);
    SpiInOut(&SX126x.Spi, 0x0585 & 0x00FF);
    SpiInOut(&SX126x.Spi, reg_0x0585 & ~0x08);
    GpioWrite (&SX126x.Spi.Nss, 1);

    // Set bits [0 to 2] of register 0x0920 to 0x06 (3V output)
    SX126xWaitOnBusy();
    GpioWrite(&SX126x.Spi.Nss, 0);
    SpiInOut(&SX126x.Spi, RADIO_WRITE_REGISTER);
    SpiInOut(&SX126x.Spi, (0x0920 & 0xFF00) >> 8);
    SpiInOut(&SX126x.Spi, 0x0920 & 0x00FF);
    SpiInOut(&SX126x.Spi, 0x06);
    GpioWrite (&SX126x.Spi.Nss, 1);
}

static void SX126xSetDio3State (bool state)
{
    uint8_t reg_0x0920;

    SX126xWaitOnBusy();
    GpioWrite(&SX126x.Spi.Nss, 0);
    SpiInOut(&SX126x.Spi, RADIO_READ_REGISTER);
    SpiInOut(&SX126x.Spi, (0x0920 & 0xFF00) >> 8);
    SpiInOut(&SX126x.Spi, 0x0920 & 0x00FF);
    SpiInOut(&SX126x.Spi, 0);
    reg_0x0920 = SpiInOut(&SX126x.Spi, 0);
    GpioWrite(&SX126x.Spi.Nss, 1);

    if (state)
    {
        SX126xWaitOnBusy();
        GpioWrite(&SX126x.Spi.Nss, 0);
        SpiInOut(&SX126x.Spi, RADIO_WRITE_REGISTER);
        SpiInOut(&SX126x.Spi, (0x0920 & 0xFF00) >> 8);
        SpiInOut(&SX126x.Spi, 0x0920 & 0x00FF);
        SpiInOut(&SX126x.Spi, reg_0x0920 | 0x08);
        GpioWrite (&SX126x.Spi.Nss, 1);
    }
    else
    {
        SX126xWaitOnBusy();
        GpioWrite(&SX126x.Spi.Nss, 0);
        SpiInOut(&SX126x.Spi, RADIO_WRITE_REGISTER);
        SpiInOut(&SX126x.Spi, (0x0920 & 0xFF00) >> 8);
        SpiInOut(&SX126x.Spi, 0x0920 & 0x00FF);
        SpiInOut(&SX126x.Spi, reg_0x0920 & ~0x08);
        GpioWrite (&SX126x.Spi.Nss, 1);       
    }
}

void SX126xAntSwOn (void)
{
    SX126xSetDio3AsOutput();
    SX126xSetDio3State(true);
}

void SX126xAntSwOff (void)
{
    SX126xSetDio3State(false);
}

bool SX126xCheckRfFrequency (uint32_t frequency)
{
    // Implement check. Currently all frequencies are supported
    return true;
}

void SX126xGetStats (uint16_t* nb_pkt_received, uint16_t* nb_pkt_crc_error, uint16_t* nb_pkt_length_error)
{
    uint8_t buf[6];

    SX126xReadCommand( RADIO_GET_STATS, buf, 6 );

    *nb_pkt_received        = (buf[0] << 8) | buf[1];
    *nb_pkt_crc_error       = (buf[2] << 8) | buf[3];
    *nb_pkt_length_error    = (buf[4] << 8) | buf[5];
}

void SX126xResetStats (void)
{
    uint8_t buf[6] = { 0x00 };

    SX126xWriteCommand(RADIO_RESET_STATS, buf, 6);
}
