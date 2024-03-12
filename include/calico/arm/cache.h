#pragma once
#if __ARM_ARCH < 5
#error "This header is for ARMv5+ only"
#endif

#include "../types.h"
#include "cp15.h"

/*! @addtogroup cache
	@{
*/

MK_EXTERN_C_START

/*! @brief Emits a "drain write buffer" instruction, also known as DataSynchronizationBarrier (DSB) in Armv6+
	@note Such a barrier is needed when sharing memory with the other CPU and with DMA; though, all cache
	manipulation functions include this instruction, thus explicit calls to this function aren't usually needed
*/
void armDrainWriteBuffer(void);

//! @brief Cleans and invalidates all data cache lines
void armDCacheFlushAll(void);

/*! @brief Cleans and invalidates data cache lines by address
	@param addr Start address (any pointer type)
	@param size Size of the address range
	@note Cache operations by address are done on cache line boundaries, ie. from `addr_ & ~0x1F` to
	`(addr_ + size) & ~0x1F` where `addr_ = (uintptr_t)addr`
	@note If `size` is large, consider using @ref armDCacheFlushAll instead. On the 3DS Arm9 Kernel,
	Nintendo uses 16 KiB as threshold
*/
void armDCacheFlush(const volatile void* addr, size_t size);

/*! @brief Invalidates data cache lines by address
	@param addr Start address (any pointer type)
	@param size Size of the address range
	@note Cache operations by address are done on cache line boundaries, ie. from `addr_ & ~0x1F` to
	`(addr_ + size) & ~0x1F` where `addr_ = (uintptr_t)addr`
	@note If `addr` and `(uintptr_t)addr + size` aren't 32-byte aligned, there is a risk of data loss
	@note If `size` is large, consider using @ref armDCacheInvalidateAll instead. On the 3DS Arm9 Kernel,
	Nintendo uses 16 KiB as threshold
*/
void armDCacheInvalidate(const volatile void* addr, size_t size);

//! @brief Invalidates all instruction cache lines
void armICacheInvalidateAll(void);

/*! @brief Invalidates instruction cache lines by address
	@param addr Start address (any pointer type)
	@param size Size of the address range
	@note Cache operations by address are done on cache line boundaries, ie. from `addr_ & ~0x1F` to
	`(addr_ + size) & ~0x1F` where `addr_ = (uintptr_t)addr`.
	@note Consider just using @ref armICacheInvalidateAll unless `size` is known to be small
*/
void armICacheInvalidate(const volatile void* addr, size_t size);

MK_EXTERN_C_END

//! @}
