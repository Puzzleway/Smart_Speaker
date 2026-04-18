#ifndef _KWS_H
#define _KWS_H

enum DeviceMode
{
	ONLINE_MODE,
	OFFLINE_MODE
};

int init_sherpa_kws();
int sherpa_kws(float *float_buffer, int resample_frams);

#endif
