/**
 * ===================================================================
 *  File Name: at24c16.c
 *  Type     : Device Controller C-source
 *  Purpose  : AT24C16 - Serial EEPROM
 *  Version  : 5.0
 * ===================================================================
 *  Description
 *      * AT24C16 16Kb EEPROM controller for STM32L5xx.
 *      * 16 bytes per page for 128 pages.
 *      * Communication is thru I2C.
 *      * DeviceID LSNibble is based on A2..A0 straps (for AT24C16/32/64)
 * ===================================================================
 *  Revision History
 *  Version/Date : v1.0 / 2025-Oct-24 / G.RUIZ  - Initial release
 *  Version/Date : v2.0 / 2025-Dec-20 / G.RUIZ  - G_HAL library
 *  Version/Date : v4.4 / 2026-Mar-06 / G.RUIZ  - DMA functions
 *  Version/Date : v5.0 / 2026-May-11 / J.JIANG
 *      * Ported from G_HAL abstraction to STM32 HAL (HAL_I2C_Mem_Write/Read)
 *      * Fixed AT24C16_Handle type name (was conflicting with enum constant)
 *      * Fixed eeprom_wait_for_ready missing current_addr argument
 * ===================================================================
 */

#include "at24c16.h"


/* ===================================================================
 * Forward declarations
 * =================================================================== */

static ReturnType eeprom_write_page( AT24C16_Handle * eeprom, uint16_t addr, uint16_t size, uint8_t * wbuffer );
static ReturnType eeprom_wait_for_ready( AT24C16_Handle * eeprom, uint16_t current_addr );
static uint8_t    get_dynamic_dev_addr( AT24C16_Handle * eeprom, uint16_t addr );
static void       get_mem_addr_params( AT24C16_Handle * eeprom, uint16_t addr, uint16_t * mem_addr, uint16_t * mem_addr_size );

ReturnType eeprom_write( AT24C16_Handle * eeprom, uint16_t addr, uint16_t size, uint8_t * wbuffer );
ReturnType eeprom_read( AT24C16_Handle * eeprom, ReadAccessMode accessMode, uint16_t addr, uint16_t size, uint8_t * rbuffer );
ReturnType eeprom_read_seq( AT24C16_Handle * eeprom, uint16_t addr, uint16_t size, uint8_t * rbuffer );
ReturnType eeprom_write_nb( AT24C16_Handle * eeprom, uint16_t addr, uint16_t size, uint8_t * wbuffer );
void       eeprom_send_memory_address_nb( AT24C16_Handle * eeprom, uint16_t addr );
ReturnType eeprom_read_nb( AT24C16_Handle * eeprom, uint16_t size );
ReturnType eeprom_read_seq_nb( AT24C16_Handle * eeprom, uint16_t addr, uint16_t size );
ReturnType eeprom_check_i2c_status( I2C_HandleTypeDef * i2c );
ReturnType eeprom_write_dma( AT24C16_Handle * eeprom, uint16_t addr, uint16_t size, uint8_t * wbuffer );
ReturnType eeprom_read_dma( AT24C16_Handle * eeprom, uint16_t size );


/* ===================================================================
 * Factory Function
 * =================================================================== */

#if defined( USE_DYNAMIC_ALLOCATION )
#include <stdlib.h>
	AT24C16_Handle * InitializeEEPROM( AT24C_Type type, I2C_HandleTypeDef * i2c, uint8_t devID_lsn )
	{
		AT24C16_Handle * obj = (AT24C16_Handle *)malloc( sizeof( AT24C16_Handle ) );
		if( obj == NULL || i2c == NULL )
			return NULL;

		obj->model_type    = type;
		obj->i2c           = i2c;
		obj->i2cAddr       = DEVICE_ID | ( devID_lsn & 0x0E );
		obj->write         = &eeprom_write;
		obj->read          = &eeprom_read;
		obj->read_seq      = &eeprom_read_seq;
		obj->write_nb      = &eeprom_write_nb;
		obj->write_addr_nb = &eeprom_send_memory_address_nb;
		obj->read_nb       = &eeprom_read_nb;
		obj->read_seq_nb   = &eeprom_read_seq_nb;
		obj->i2c_is_idle   = &eeprom_check_i2c_status;
		obj->write_dma     = &eeprom_write_dma;
		obj->read_dma      = &eeprom_read_dma;

		return obj;
	}
#else
	ReturnType InitializeEEPROM( AT24C16_Handle * eeprom, AT24C_Type type, I2C_HandleTypeDef * i2c, uint8_t devID_lsn )
	{
		if( i2c == NULL )
			return FAIL;

		eeprom->model_type    = type;
		eeprom->i2c           = i2c;
		eeprom->i2cAddr       = DEVICE_ID | ( devID_lsn & 0x0E );
		eeprom->write         = &eeprom_write;
		eeprom->read          = &eeprom_read;
		eeprom->read_seq      = &eeprom_read_seq;
		eeprom->write_nb      = &eeprom_write_nb;
		eeprom->write_addr_nb = &eeprom_send_memory_address_nb;
		eeprom->read_nb       = &eeprom_read_nb;
		eeprom->read_seq_nb   = &eeprom_read_seq_nb;
		eeprom->i2c_is_idle   = &eeprom_check_i2c_status;
		eeprom->write_dma     = &eeprom_write_dma;
		eeprom->read_dma      = &eeprom_read_dma;

		return PASS;
	}
#endif


/* ===================================================================
 * Internal helpers
 * =================================================================== */

/*
 * For AT24C16, the upper 3 bits of the 11-bit memory address are encoded
 * in the I2C device address (bits P2..P0, positions [3:1]).
 * For AT24C32/64, the static address from initialization is returned.
 */
static uint8_t get_dynamic_dev_addr( AT24C16_Handle * eeprom, uint16_t addr )
{
	if( eeprom->model_type == AT24C16 )
	{
		uint8_t page_bits = ((addr >> 8) & 0x07) << 1;
		return DEVICE_ID | page_bits;
	}
	return eeprom->i2cAddr;
}

/* Fills mem_addr and mem_addr_size for HAL_I2C_Mem_Write/Read calls. */
static void get_mem_addr_params( AT24C16_Handle * eeprom, uint16_t addr,
                                 uint16_t * mem_addr, uint16_t * mem_addr_size )
{
	if( eeprom->model_type == AT24C16 )
	{
		*mem_addr      = addr & 0x00FF;
		*mem_addr_size = I2C_MEMADD_SIZE_8BIT;
	}
	else
	{
		*mem_addr      = addr;
		*mem_addr_size = I2C_MEMADD_SIZE_16BIT;
	}
}

/* Polls until the EEPROM ACKs (internal write cycle complete) or times out. */
static ReturnType eeprom_wait_for_ready( AT24C16_Handle * eeprom, uint16_t current_addr )
{
	uint8_t dev_addr = get_dynamic_dev_addr( eeprom, current_addr );
	HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady( eeprom->i2c, dev_addr, 300,
	                                                   AT24C16_WRITE_CYCLE_TIME_MS * 2 );
	return ( status == HAL_OK ) ? PASS : FAIL;
}


/* ===================================================================
 * Blocking functions
 * =================================================================== */

static ReturnType eeprom_write_page( AT24C16_Handle * eeprom, uint16_t addr, uint16_t size, uint8_t * wbuffer )
{
	if( eeprom == NULL )
		return FAIL;

	/* Wait for any previous write cycle to complete before starting a new one */
	if( eeprom_wait_for_ready( eeprom, addr ) != PASS )
		return FAIL;

	uint8_t  dev_addr = get_dynamic_dev_addr( eeprom, addr );
	uint16_t mem_addr, mem_addr_size;
	get_mem_addr_params( eeprom, addr, &mem_addr, &mem_addr_size );

	HAL_StatusTypeDef status = HAL_I2C_Mem_Write( eeprom->i2c, dev_addr, mem_addr, mem_addr_size,
	                                               wbuffer, size, 100 );
	return ( status == HAL_OK ) ? PASS : FAIL;
}

ReturnType eeprom_write( AT24C16_Handle * eeprom, uint16_t addr, uint16_t size, uint8_t * wbuffer )
{
	if( eeprom == NULL )
		return FAIL;

	uint16_t page_size      = ( eeprom->model_type == AT24C16 ) ? AT24C16_BYTE_PER_PAGE : AT24C32_64_BYTE_PER_PAGE;
	uint16_t current_addr   = addr;
	uint8_t  * data_ptr     = wbuffer;
	uint16_t bytes_remaining = size;

	while( bytes_remaining > 0 )
	{
		uint16_t bytes_to_page_end = page_size - ( current_addr % page_size );
		uint16_t chunk_size = ( bytes_remaining < bytes_to_page_end ) ? bytes_remaining : bytes_to_page_end;

		if( eeprom_write_page( eeprom, current_addr, chunk_size, data_ptr ) != PASS )
			return FAIL;

		current_addr    += chunk_size;
		data_ptr        += chunk_size;
		bytes_remaining -= chunk_size;

		if( bytes_remaining > 0 )
			eeprom_wait_for_ready( eeprom, current_addr );
	}

	return PASS;
}

ReturnType eeprom_read( AT24C16_Handle * eeprom, ReadAccessMode accessMode, uint16_t addr, uint16_t size, uint8_t * rbuffer )
{
	if( eeprom == NULL )
		return FAIL;

	HAL_StatusTypeDef status;

	if( accessMode == READ_ADDRESSED )
	{
		uint8_t  dev_addr = get_dynamic_dev_addr( eeprom, addr );
		uint16_t mem_addr, mem_addr_size;
		get_mem_addr_params( eeprom, addr, &mem_addr, &mem_addr_size );
		status = HAL_I2C_Mem_Read( eeprom->i2c, dev_addr, mem_addr, mem_addr_size, rbuffer, size, 100 );
	}
	else
	{
		/* READ_CURRENT: sequential read from EEPROM's internal address pointer */
		status = HAL_I2C_Master_Receive( eeprom->i2c, eeprom->i2cAddr, rbuffer, size, 100 );
	}

	return ( status == HAL_OK ) ? PASS : FAIL;
}

ReturnType eeprom_read_seq( AT24C16_Handle * eeprom, uint16_t addr, uint16_t size, uint8_t * rbuffer )
{
	return eeprom_read( eeprom, READ_ADDRESSED, addr, size, rbuffer );
}


/* ===================================================================
 * Non-blocking (interrupt) functions
 * =================================================================== */

ReturnType eeprom_write_nb( AT24C16_Handle * eeprom, uint16_t addr, uint16_t size, uint8_t * wbuffer )
{
	if( eeprom == NULL )
		return FAIL;

	uint8_t  dev_addr = get_dynamic_dev_addr( eeprom, addr );
	uint16_t mem_addr, mem_addr_size;
	get_mem_addr_params( eeprom, addr, &mem_addr, &mem_addr_size );

	/* Copy to internal buffer so it stays valid for the duration of the IT transfer */
	uint16_t copy_size = ( size <= AT24C16_MAX_BYTE_PER_PAGE ) ? size : AT24C16_MAX_BYTE_PER_PAGE;
	for( uint16_t i = 0; i < copy_size; i++ )
		eeprom->int_buffer[i] = wbuffer[i];

	HAL_StatusTypeDef status = HAL_I2C_Mem_Write_IT( eeprom->i2c, dev_addr, mem_addr, mem_addr_size,
	                                                  eeprom->int_buffer, copy_size );
	return ( status == HAL_OK ) ? PASS : FAIL;
}

void eeprom_send_memory_address_nb( AT24C16_Handle * eeprom, uint16_t addr )
{
	/* Legacy stub: HAL_I2C_Mem_Read_IT handles the address phase internally */
	(void)eeprom;
	(void)addr;
}

ReturnType eeprom_read_nb( AT24C16_Handle * eeprom, uint16_t size )
{
	if( eeprom == NULL )
		return FAIL;

	/* Reads from EEPROM's current internal address pointer */
	HAL_StatusTypeDef status = HAL_I2C_Master_Receive_IT( eeprom->i2c, eeprom->i2cAddr,
	                                                       eeprom->int_buffer, size );
	return ( status == HAL_OK ) ? PASS : FAIL;
}

ReturnType eeprom_read_seq_nb( AT24C16_Handle * eeprom, uint16_t addr, uint16_t size )
{
	if( eeprom == NULL )
		return FAIL;

	uint8_t  dev_addr = get_dynamic_dev_addr( eeprom, addr );
	uint16_t mem_addr, mem_addr_size;
	get_mem_addr_params( eeprom, addr, &mem_addr, &mem_addr_size );

	HAL_StatusTypeDef status = HAL_I2C_Mem_Read_IT( eeprom->i2c, dev_addr, mem_addr, mem_addr_size,
	                                                 eeprom->int_buffer, size );
	return ( status == HAL_OK ) ? PASS : FAIL;
}

ReturnType eeprom_check_i2c_status( I2C_HandleTypeDef * i2c )
{
	return ( HAL_I2C_GetState( i2c ) == HAL_I2C_STATE_READY ) ? PASS : FAIL;
}


/* ===================================================================
 * DMA functions
 * =================================================================== */

ReturnType eeprom_write_dma( AT24C16_Handle * eeprom, uint16_t addr, uint16_t size, uint8_t * wbuffer )
{
	if( eeprom == NULL )
		return FAIL;

	uint8_t  dev_addr = get_dynamic_dev_addr( eeprom, addr );
	uint16_t mem_addr, mem_addr_size;
	get_mem_addr_params( eeprom, addr, &mem_addr, &mem_addr_size );

	uint16_t copy_size = ( size <= AT24C16_MAX_BYTE_PER_PAGE ) ? size : AT24C16_MAX_BYTE_PER_PAGE;
	for( uint16_t i = 0; i < copy_size; i++ )
		eeprom->int_buffer[i] = wbuffer[i];

	HAL_StatusTypeDef status = HAL_I2C_Mem_Write_DMA( eeprom->i2c, dev_addr, mem_addr, mem_addr_size,
	                                                   eeprom->int_buffer, copy_size );
	return ( status == HAL_OK ) ? PASS : FAIL;
}

ReturnType eeprom_read_dma( AT24C16_Handle * eeprom, uint16_t size )
{
	if( eeprom == NULL )
		return FAIL;

	/* Reads from EEPROM's current internal address pointer via DMA */
	HAL_StatusTypeDef status = HAL_I2C_Master_Receive_DMA( eeprom->i2c, eeprom->i2cAddr,
	                                                        eeprom->int_buffer, size );
	return ( status == HAL_OK ) ? PASS : FAIL;
}
