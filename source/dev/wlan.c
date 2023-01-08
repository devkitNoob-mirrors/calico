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

WlanIeHdr* wlanFindRsnOrWpaIe(void* rawdata, unsigned rawdata_len)
{
	u8* pos = (u8*)rawdata;
	WlanIeHdr* wpa = NULL;
	while (rawdata_len > sizeof(WlanIeHdr)) {
		WlanIeHdr* ie = (WlanIeHdr*)pos;
		pos += 2;
		rawdata_len -= 2;

		if (ie->len <= rawdata_len) {
			pos += ie->len;
			rawdata_len -= ie->len;
		} else {
			break;
		}

		if (ie->id == WlanEid_RSN) {
			return ie;
		}

		if (ie->id == WlanEid_Vendor && ie->len >= 4 && ie->data[0] == 0x00 && ie->data[1] == 0x50 && ie->data[2] == 0xf2 && ie->data[3] == 0x01) {
			wpa = ie;
		}
	}

	return wpa;
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
		WlanIeHdr* elhdr = netbufPopHeaderType(pPacket, WlanIeHdr);
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

			case WlanEid_SSID: {
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
