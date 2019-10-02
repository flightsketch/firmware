
 /******************************************************************************
 * @file    isp4580.h
 * @author  Insight SiP
 * @version V2.0.0
 * @date    30-january-2019
 * @brief   Board (isp4580) specific functions declaration.
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

#ifndef __BOARD_ISP4580_H__
#define __BOARD_ISP4580_H__

#define LPS22HB_ADDR            0x5C // Pressure
#define SHTC1_ADDR              0x70 // Humidity
#define MAX44009_ADDR           0x4A // Light
#define LSM9DS1_ACC_GYRO_ADDR 	0x6A // Acc/Gyro
#define LSM9DS1_MAG_ADDR        0x1C // mag

#define PIN_IMU_INT2_AG         4
#define PIN_IMU_DEN_AG          5
#define PIN_UART_RX             6
#define PIN_TWI_SCL             7
#define PIN_UART_TX             8
#define PIN_TWI_SDA             12
#define PIN_RED_LED             13                  // Red LED
#define PIN_PRESS_INTDRDY       14                  // Pressure sensor IRQ
#define PIN_YELLOW_LED          15                  // Green LED
#define PIN_GREEN_LED           16                  // Yellow LED
#define PIN_USER_BUTTON         18
#define PIN_LIG_INT             20                  // Light sensor IRQ

#endif // __BOARD_ISP4580_H__
