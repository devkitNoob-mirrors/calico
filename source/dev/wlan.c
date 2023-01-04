#include <calico/types.h>
#include <calico/dev/netbuf.h>
#include <calico/dev/wlan.h>
#include <string.h>

WlanBssDesc* wlanFindOrAddBss(WlanBssDesc* desc_table, unsigned* num_entries, void* bssid, int rssi)
{
	unsigned i;
	for (i = 0; i < *num_entries; i ++) {
		if (memcmp(desc_table[i].bssid, bssid, 6) == 0) {
			if (rssi > desc_table[i].rssi) {
				// Newer beacon has stronger signal - update the old one
				break;
			} else {
				// Drop this beacon as it has a weaker signal
				return NULL;
			}
		}
	}

	if (i == WLAN_MAX_BSS_ENTRIES) {
		// Out of space
		return NULL;
	} else if (i == *num_entries) {
		// We have a new BSS
		(*num_entries) ++;
	}

	return &desc_table[i];
}

void wlanParseBeacon(WlanBssDesc* desc, NetBuf* pPacket)
{
	WlanBeaconHdr* hdr = netbufPopHeaderType(pPacket, WlanBeaconHdr);
	if (!hdr) {
		return;
	}

	// Fill in basic info
	memset(desc, 0, sizeof(*desc));
	desc->ieee_caps = hdr->capabilities;

	while (pPacket->len) {
		WlanBeaconIeHdr* elhdr = netbufPopHeaderType(pPacket, WlanBeaconIeHdr);
		if (!elhdr) {
			break;
		}

		void* data = netbufPopHeader(pPacket, elhdr->len);
		if (!data) {
			break;
		}

		switch (elhdr->id) {
			default: {
				//dietPrint(" eid[%.2X] len=%u\n", elhdr->type, elhdr->len);
				break;
			}

			case WlanBeaconEid_SSID: {
				if (elhdr->len <= WLAN_MAX_SSID_LEN) {
					desc->ssid_len = elhdr->len;
					memcpy(desc->ssid, data, elhdr->len);
				}
				break;
			}

			// TODO: Rates, encryption
		}
	}
}
