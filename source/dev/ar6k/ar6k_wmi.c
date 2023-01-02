#include "common.h"
#include <string.h>

void _ar6kWmiEventRx(Ar6kDev* dev, NetBuf* pPacket)
{
	Ar6kWmiCtrlHdr* evthdr = netbufPopHeaderType(pPacket, Ar6kWmiCtrlHdr);
	if (!evthdr) {
		dietPrint("[AR6K] WMI event RX too small\n");
		return;
	}

	switch (evthdr->id) {
		default: {
			dietPrint("[AR6K] WMI unkevt %.4X (%u)\n", evthdr->id, pPacket->len);
			break;
		}

		case Ar6kWmiEventId_Ready: {
			if (dev->wmi_ready) {
				dietPrint("[AR6K] dupe WMI_READY\n");
				break;
			}

			Ar6kWmiEvtReady* evt = netbufPopHeaderType(pPacket, Ar6kWmiEvtReady);
			if (!evt) {
				dietPrint("[AR6K] invalid WMI event size\n");
				break;
			}

			// Retrieve address to app host interest area
			u32 hi_app_host_interest = 0;
			if (!ar6kDevReadRegDiag(dev, dev->hia_addr+0x00, &hi_app_host_interest)) {
				dietPrint("[AR6K] cannot get AHI area\n");
				break;
			}

			// Set WMI protocol version
			if (!ar6kDevWriteRegDiag(dev, hi_app_host_interest, AR6K_WMI_PROTOCOL_VER)) {
				dietPrint("[AR6K] cannot set WMI ver\n");
				break;
			}

			// WMI is now ready for use!
			memcpy(dev->macaddr, evt->macaddr, 6);
			dev->wmi_ready = true;
			break;
		}
	}
}

void ar6kWmiWaitReady(Ar6kDev* dev)
{
	// Wait for WMI to be ready
	while (!dev->wmi_ready) {
		threadSleep(1000);
	}

	dietPrint("[AR6K] WMI ready!\n       MAC %.2X:%.2X:%.2X:%.2X:%.2X:%.2X\n",
		dev->macaddr[0], dev->macaddr[1], dev->macaddr[2],
		dev->macaddr[3], dev->macaddr[4], dev->macaddr[5]);
}
