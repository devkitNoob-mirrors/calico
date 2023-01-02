#pragma once
#include "netbuf.h"

MEOW_CONSTEXPR unsigned wlanFreqToChannel(unsigned freq_mhz)
{
	if (freq_mhz == 2484) {
		return 14;
	} else if (freq_mhz < 2484) {
		return (freq_mhz - 2407) / 5;
	} else if (freq_mhz < 5000) {
		return 15 + ((freq_mhz - 2512) / 20);
	} else {
		return (freq_mhz - 5000) / 5;
	}
}

MEOW_CONSTEXPR unsigned wlanChannelToFreq(unsigned ch)
{
	if (ch == 14) {
		return 2484;
	} else if (ch < 14) {
		return 2407 + 5*ch;
	} else if (ch < 27) {
		return 2512 + 20*(ch-15);
	} else {
		return 5000 + 5*ch;
	}
}
