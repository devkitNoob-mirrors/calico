#include "common.h"
#include <string.h>

void _ar6kWmiEventRx(Ar6kDev* dev, void* pkt, size_t pktsize)
{
	if (pktsize < sizeof(Ar6kWmiCtrlHdr)) {
		dietPrint("[AR6K] WMI event RX too small\n");
		return;
	}

	Ar6kWmiCtrlHdr* evthdr = (Ar6kWmiCtrlHdr*)pkt;
	switch (evthdr->id) {
		default: {
			dietPrint("[AR6K] WMI unkevt %.4X (%u)\n", evthdr->id, pktsize);
			break;
		}

		case Ar6kWmiEventId_Ready: {
			if (dev->wmi_ready) {
				dietPrint("[AR6K] dupe WMI_READY\n");
				break;
			}

			Ar6kWmiEvtReady* evt = (Ar6kWmiEvtReady*)pkt;
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
