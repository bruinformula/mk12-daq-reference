/**
 * ===================================================================
 *  File Name: at24c16.h
 *  Type     : Device Controller C-header
 *  Purpose  : AT24C16 - Serial EEPROM
 *  Version  : 1.1
 * ===================================================================
 *  Description
 * 		* Adopted from the original at24cxx.h by G.RUIZ
 * 		* AT24C16 16Kb EEPROM controller for STM32L5xx.
 * 		* 16 bytes per page for 128 pages.
 * 		* Communication is thru I2C.
 * 		* DeviceID LSNibble is based on A2..A0 straps (for AT24C16/32/64)
 * ===================================================================
 *  Revision History
 *  	Version/Date : v1.1 / 2026-May-11 / J.JIANG
 * 			* Updated for STM32L5xx HAL and changed to just have AT24C16 support for simplicity.
 * 
 * ===================================================================
 */


#ifndef __AT24C16__
#define __AT24C16__


#include <stdint.h>
#include "stm32l5xx_hal.h"  // For I2C_HandleTypeDef

typedef uint8_t ReturnType;
#define PASS (ReturnType)1
#define FAIL (ReturnType)0





/**
 * @brief	I2C device address for AT24Cxx devices.
 * 			Least significant nibble depends on straps,
 * 			or if used by the device as bank address.
 */
#define DEVICE_ID	( 0xA0		)


/**
 * @brief	Supported devices:
 */
typedef enum
{
	AT24C16,
	AT24C32,
	AT24C64
} AT24C_Type;


/**
 * @brief	Read access type
 */
typedef enum
{
	READ_CURRENT,
	READ_ADDRESSED
} ReadAccessMode;


/**
 * @brief	AT24C16 device specs
 */

#define AT24C16_ADDR_BYTE_SIZE			( 1U		)
#define AT24C16_WRITE_CYCLE_TIME_MS		( 10U		)
#define AT24C16_BYTE_PER_PAGE			( 16U		)
#define AT24C16_PAGE_SIZE				( 128U		)

#define AT24C32_64_ADDR_BYTE_SIZE		( 2U		)
#define AT24C32_64_WRITE_CYCLE_TIME_MS	( 20U		)
#define AT24C32_64_BYTE_PER_PAGE		( 32U		)
#define AT24C32_PAGE_SIZE				( 128U		)
#define AT24C64_PAGE_SIZE				( 256U		)

#define AT24C16_MAX_ADDR_BYTE_SIZE		( AT24C32_64_ADDR_BYTE_SIZE )
#define AT24C16_MAX_BYTE_PER_PAGE		( AT24C32_64_BYTE_PER_PAGE )
#define AT24C16_BUFFER_ARRAY_SIZE		( AT24C16_MAX_ADDR_BYTE_SIZE + AT24C16_MAX_BYTE_PER_PAGE )

/**
 * @brief	AT24C16 object. User must create a variable at the main project,
 * 			then call InitializeEEPROM(). Requires a G_HAL_I2C_Handle object.
 *
 */
typedef struct AT24C16 AT24C16_Handle;


struct AT24C16
{
	/* Device model (currently supported: AT24C16, AT24C32, AT24C64) */
	AT24C_Type model_type;

	/* EEPROM's I2C address, initialized by the InitializeEEPROM() factory function */
	uint8_t i2cAddr;

	/* Pointer to a G_HAL I2C object */
	I2C_HandleTypeDef * i2c;

	/**
	 * @brief	Write byte/s to EEPROM, starting from the given address
	 * @param	eeprom - The AT24C16 handler object
	 * @param	addr - EEPROM address
	 * @param	size - Number of bytes to write
	 * @param	wrbuffer - Pointer to the array of bytes to be written to EEPROM.
	 * 			@attention	Do not write directly to the G_HAL_I2C_Handle object's buffer!
	 * 						Use a byte array and pass it by reference using this function.
	 * @return	0x01 = Success; 0x00 = Failed
	 *
	 */
	ReturnType ( * write )( AT24C16_Handle * eeprom, uint16_t addr, uint16_t size, uint8_t * wbuffer );

	/**
	 * @brief	Read byte/s from EEPROM.
	 * @param	eeprom - The AT24C16 handler object
	 * @param	accessMode - If READ_CURRENT, no start address is send, and will sequentially read the memory.
	 *						If READ_ADDRESSED, sequentially reads the memory starting from the given address.
	* @param	addr - EEPROM start address
	* @param	size - Number of bytes to read
	* @param	rbuffer - Pointer to the array of bytes read from EEPROM
	* @return	0x01 = Success; 0x00 = Failed
	*/
	ReturnType ( * read )( AT24C16_Handle * eeprom, ReadAccessMode accessMode, uint16_t addr, uint16_t size, uint8_t * rbuffer );

	/**
	 * @brief	Read a stream of bytes to EEPROM, starting from the given address
	 * 			The difference between read() and read_seq() is in the underlying hardware-level function;
	 * 			In read(), it performs separate write() into read(). In read_seq(), it uses sequential read,
	 * 			which encapsulates write() into read() in one function.
	 * @param	eeprom - The AT24C16 handler object
	 * @param	addr - EEPROM address
	 * @param	size - Number of bytes to read
	 * @param	rbuffer - Pointer to the array of bytes read from EEPROM
	 * @return	0x01 = Success; 0x00 = Failed
	 *
	 */
	ReturnType ( * read_seq )( AT24C16_Handle * eeprom, uint16_t addr, uint16_t size, uint8_t * rbuffer );

	/**
	 * @brief	Non-blocking function to write byte/s to EEPROM, starting from the given address.
	 * @param	eeprom - The AT24C16 handler object
	 * @param	addr - EEPROM address
	 * @param	size - Number of bytes to write
	 * @param	wbuffer - Pointer to the array of bytes to be written to EEPROM
	 * @return	0x01 = PASS
	 * @return	0x00 = FAIL
	 *
	 */
	ReturnType ( * write_nb )( AT24C16_Handle * eeprom, uint16_t addr, uint16_t size, uint8_t * wbuffer );

	void ( * write_addr_nb )( AT24C16_Handle * eeprom, uint16_t addr );

	/**
	 * @brief	Non-blocking function to read byte/s from EEPROM.
	 * @brief	The read bytes are stored in eeprom.int_buffer.
	 * @param	eeprom - The AT24C16 handler object
	* @param	size - Number of bytes to read
	* @return	0x01 = PASS
	* @return	0x00 = FAIL
	*/
	ReturnType ( * read_nb )( AT24C16_Handle * eeprom, uint16_t size );

	/**
	 * @brief	Non-blocking function to read a stream of bytes to EEPROM, starting from the given address
	 * 			The difference between read() and read_seq() is in the underlying hardware-level function;
	 * 			In read(), it performs separate write() into read(). In read_seq(), it uses sequential read,
	 * 			which encapsulates write() into read() in one function.
	 * @param	eeprom - The AT24C16 handler object
	 * @param	size - Number of bytes to read
	 * @return	0x01 = PASS
	 * @return	0x00 = FAIL
	 */
	ReturnType ( * read_seq_nb )( AT24C16_Handle * eeprom, uint16_t addr, uint16_t size );

	/**
	 * @brief	Checks for the current I2C state.
	 * @param	i2c G_HAL_I2C_Handle defined in g_hal_i2c.h
	 * @return	0x01 = PASS (state = I2C_STATE_IDLE)
	 * @return	0x00 = FAIL (state != I2C_STATE_IDLE)
	 */
	ReturnType ( * i2c_is_idle )( I2C_HandleTypeDef * i2c );

	/**
	 * @brief	Performs an I2C-write using DMA.
	 * @param	eeprom - The AT24C16 handler object
	 * @param	size - Number of bytes to write
	 * @param	wrbuffer - Pointer to the array of bytes to be written to EEPROM. Must include the EEPROM address.
	 * @return	0x00 (FAIL)
	 * @return	0x01 (PASS)
	 */
	ReturnType ( * write_dma )( AT24C16_Handle * eeprom, uint16_t addr, uint16_t size, uint8_t * wbuffer );

	/**
	 * @brief	Performs an I2C-read using DMA.
	 * @param	eeprom - The AT24C16 handler object
	 * @param	size - Number of bytes to read
	 * @return	0x00 (FAIL)
	 * @return	0x01 (PASS)
	 *
	 */
	ReturnType ( * read_dma )( AT24C16_Handle * eeprom, uint16_t size );

	/* Internal byte buffer for interrupt/DMA-based communications. Also used as a readback buffer. */
	uint8_t int_buffer[AT24C16_BUFFER_ARRAY_SIZE];
};



#ifdef USE_DYNAMIC_ALLOCATION
#include <stdlib.h>
	/**
	 * @brief	Factory to dynamically allocate an AT24C16 handler object.
	 * @param	type - Device model.
	 * @param	i2c - Pointer to an I2C object which will be used by *eeprom handler object.
	 * @param	devID_lsn - Least significant nibble of the device (through straps or as bank addressing)
	 * @return	The AT24C16 object handler created by this factory.
	 *
	 */
	AT24C16_Handle * InitializeEEPROM( AT24C_Type type, I2C_HandleTypeDef * i2c, uint8_t devID_lsn );
#else
	/**
	 * @brief	Factory to initialize an AT24C16 handler object.
	 * @param	eeprom - Pointer to an AT24C16 handler object.
	 * @param	type - Device model.
	 * @param	i2c - Pointer to a G_HAL_I2C_Handle object which will be used by this *AT24C16 handler object.
	 * @param	devID_lsn - Least significant nibble of the device (through straps or as bank addressing)
	 * @return	0x01 = Success; 0x00 = Failed
	 */
	extern ReturnType InitializeEEPROM( AT24C16_Handle * eeprom, AT24C_Type type, I2C_HandleTypeDef * i2c, uint8_t devID_lsn );
#endif





#endif	///__AT24CXX__
