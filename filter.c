#include <stdio.h>
#include <stdlib.h>

#define M_PI 3.14159265358979323846


#define CHUNK_SIZE 64
#define DEVICE_CHANNELS 2
#define DEVICE_SAMPLERATE 48000
typedef float audio_format;

void lowpass(audio_format *buf, size_t buflen, audio_format *starting_value, double cutoff)
{
    double dt = 1.0 / (double)DEVICE_SAMPLERATE;
    double rc = 1.0 / (2.0 * M_PI * cutoff);
    double alpha = dt / (rc + dt); 

    for (int i = 0; i < DEVICE_CHANNELS; ++i) {
        buf[i] = starting_value[i] + (alpha * (buf[i] - starting_value[i]));
    }
 
    for (size_t i = DEVICE_CHANNELS; i < buflen; i += DEVICE_CHANNELS) {
        for (int j = 0; j < DEVICE_CHANNELS; ++j) {
            buf[i+j] = buf[i-DEVICE_CHANNELS+j] + (alpha * (buf[i+j] - buf[i-DEVICE_CHANNELS+j]));
        }
    }
}

int main(int argc, char *argv[]) 
{
    static size_t buflen = CHUNK_SIZE * DEVICE_CHANNELS;

    static audio_format previous_frame[DEVICE_CHANNELS] = {0};
    static audio_format buf[CHUNK_SIZE * DEVICE_CHANNELS] = {0};

    double cutoff = 120.0;
    if (argc > 1) {
        if (argc == 2) {
            cutoff = atof(argv[1]);
        }
    }

    while (1) {

        for (int i = 0; i < DEVICE_CHANNELS; ++i) {
            previous_frame[i] = buf[buflen - DEVICE_CHANNELS + i];
        }

        size_t nread = fread(buf, sizeof(audio_format) * DEVICE_CHANNELS, CHUNK_SIZE, stdin);
        if (nread == 0 || feof(stdin)) {
            return EXIT_SUCCESS;
        }

        lowpass(buf, nread * DEVICE_CHANNELS, previous_frame, cutoff);

        size_t nwrote = 0;
        while (nwrote < nread) {
            size_t n = fwrite(buf, sizeof(audio_format) * DEVICE_CHANNELS, nread, stdout);
            if (n == 0 || feof(stdout)) {
                return EXIT_SUCCESS;
            }

            nwrote += n;
        }

    }
}
