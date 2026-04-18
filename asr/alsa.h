#ifndef _ALSA_H
#define _ALSA_H

#include <alsa/asoundlib.h>

#define RECORD_DEVICE    "hw:0,0"
#define RATE             44100

int init_alsa();
void resample_linear(int16_t *input, int in_len,
                          int16_t *output, int out_len);

#endif
