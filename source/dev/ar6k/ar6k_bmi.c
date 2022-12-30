#include "common.h"

bool ar6kBmiBufferSend(Ar6kDev* dev, const void* buffer, size_t size)
{
	u32 credits = 0;
	while (!credits) {
		if (!sdioCardReadExtended(dev->sdio, 1, 0x000450, &credits, 4)) {
			return false;
		}
		credits &= 0xff;
	}

	return sdioCardWriteExtended(dev->sdio, 1, 0x000800 + 0x800 - size, buffer, size);
}

bool ar6kBmiBufferRecv(Ar6kDev* dev, void* buffer, size_t size)
{
	// TODO: why does official code not have the credits loop?
	return sdioCardReadExtended(dev->sdio, 1, 0x000800 + 0x800 - size, buffer, size);
}

bool ar6kBmiGetTargetInfo(Ar6kDev* dev, Ar6kBmiTargetInfo* info)
{
	struct {
		u32 id;
	} cmd = { .id = Ar6kBmiCmd_GetTargetInfo };

	if (!ar6kBmiBufferSend(dev, &cmd, sizeof(cmd))) {
		return false;
	}

	if (!ar6kBmiBufferRecv(dev, &info->target_ver, sizeof(u32))) {
		return false;
	}

	if (info->target_ver == UINT32_MAX) { // TARGET_VERSION_SENTINAL (sic)
		// Read size of target info struct
		u32 total_size = 0;
		if (!ar6kBmiBufferRecv(dev, &total_size, sizeof(u32))) {
			return false;
		}

		// Ensures it matches our size
		if (total_size != sizeof(u32) + sizeof(*info)) {
			dietPrint("bmiTargetInfo bad size %lu\n", total_size);
			return false;
		}

		// Read the target info, this time for real
		return ar6kBmiBufferRecv(dev, info, sizeof(*info));
	} else {
		// Older protocol - fill in the gaps
		info->target_type = 1; // TARGET_TYPE_AR6001
		return true;
	}
}
