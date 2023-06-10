#include "common.h"
#include <calico/nds/bios.h>
#include <calico/nds/pm.h>

alignas(2) u8 g_mwlCalibData[MWL_CALIB_NVRAM_MAX_SZ];

bool mwlCalibLoad(void)
{
	MwlCalibData* calib = mwlGetCalibData();

	// Check if calibration data is already loaded
	if (calib->crc16 != 0) {
		dietPrint("[MWL] Calib already loaded\n");
		return true;
	}

	// Read data from NVRAM
	if (!pmReadNvram(g_mwlCalibData, MWL_CALIB_NVRAM_OFFSET, MWL_CALIB_NVRAM_MAX_SZ)) {
		dietPrint("[MWL] Calib load fail\n");
		goto _fail;
	}

	if (calib->total_len > (MWL_CALIB_NVRAM_MAX_SZ-offsetof(MwlCalibData, total_len))) {
		dietPrint("[MWL] Bad calib size\n");
		goto _fail;
	}

	if (calib->crc16 != svcGetCRC16(0, &calib->total_len, calib->total_len)) {
		dietPrint("[MWL] Calib CRC fail\n");
		goto _fail;
	}

	dietPrint("[MWL] Calib v%u, RF type %u\n", calib->version, calib->rf_type);
	dietPrint("[MWL] MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
		calib->mac_addr[0]&0xff, calib->mac_addr[0]>>8,
		calib->mac_addr[1]&0xff, calib->mac_addr[1]>>8,
		calib->mac_addr[2]&0xff, calib->mac_addr[2]>>8);

	calib->crc16 = 1;
	return true;

_fail:
	calib->crc16 = 0;
	return false;
}
