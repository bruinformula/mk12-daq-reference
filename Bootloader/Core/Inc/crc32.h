/**
 * @file    crc32.h
 * @brief   IEEE 802.3 CRC-32 (Ethernet / Python binascii compatible)
 * @author  Bruin Formula Racing (UCLA Formula SAE)
 */

#ifndef INC_CRC32_H_
#define INC_CRC32_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Compute standard IEEE 802.3 CRC-32 (polynomial 0xEDB88320).
 *        Matches Python `binascii.crc32(data)` exactly.
 */
uint32_t CRC32_Calculate(const uint8_t *data, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* INC_CRC32_H_ */
