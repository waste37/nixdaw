#define MA_NO_ENCODING
#include "miniaudio.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define DEVICE_FORMAT       ma_format_f32
#define DEVICE_CHANNELS     2
#define DEVICE_SAMPLE_RATE  48000
#define CHUNK_SIZE 800

int finished = 0;

#define MS_TO_NS 1000000
int msleep(long msec)
{
    struct timespec ts;
    int res;
    if (msec < 0) {
        errno = EINVAL;
        return -1;
    }
    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * MS_TO_NS;
    do { res = nanosleep(&ts, &ts); } while (res && errno == EINTR);
    return res;
}

ma_device *get_and_start_device(
    ma_format format, ma_uint32 channels, ma_uint32 sample_rate, 
    ma_device_data_proc callback, void *user_data) 
{
    ma_device_config device_config;
    static ma_device device;
    device_config = ma_device_config_init(ma_device_type_playback);
    device_config.playback.format   = format;
    device_config.playback.channels = channels;
    device_config.sampleRate        = sample_rate;
    device_config.dataCallback      = callback;
    device_config.pUserData         = user_data;
    if (ma_device_init(0, &device_config, &device) != MA_SUCCESS) {
        fprintf(stderr, "Failed to open playback device.\n");
        return 0;
    }

    if (ma_device_start(&device) != MA_SUCCESS) {
        fprintf(stderr, "Failed to start playback device.\n");
        ma_device_uninit(&device);
        return 0;
    }
    return &device;
}

void decoder_audio_callback(ma_device* dev, void* out, const void* in, ma_uint32 frame_count)
{
    ma_decoder* decoder = (ma_decoder*)dev->pUserData;
    if (!decoder) return;
    ma_uint64 frames_read = 0;
    ma_result result = ma_data_source_read_pcm_frames(decoder, out, frame_count, &frames_read);
    if (result != MA_SUCCESS || frames_read < frame_count) {
        finished = 1;
    }
}

static ma_float buf[CHUNK_SIZE * DEVICE_CHANNELS];

void play_from_file(char *filename, int loop, int file_output) 
{
    ma_decoder decoder;
    ma_decoder_config decoder_config = ma_decoder_config_init(DEVICE_FORMAT, DEVICE_CHANNELS, DEVICE_SAMPLE_RATE);
    ma_result result = ma_decoder_init_file(filename, &decoder_config, &decoder);
    ma_device *device = 0;
    if (result != MA_SUCCESS) {
        fprintf(stderr, "Could not load file: %s\n", filename);
        exit(EXIT_FAILURE);
    }

    if (loop) ma_data_source_set_looping(&decoder, MA_TRUE);

    if (!file_output) {
        device = get_and_start_device(
            decoder.outputFormat, decoder.outputChannels, decoder.outputSampleRate, 
            decoder_audio_callback, &decoder
        );
        if (!device) {
            ma_decoder_uninit(&decoder);
            exit(EXIT_FAILURE);
        }
    }

    while (!finished) {
        if (file_output) {
            ma_uint64 n_read = 0;

            if (ma_data_source_read_pcm_frames((ma_data_source*)&decoder, buf, CHUNK_SIZE, &n_read) != MA_SUCCESS) {
                exit(EXIT_SUCCESS);
            }

            size_t n_written = 0;

            while (n_written < n_read) {
                size_t n = fwrite(buf, sizeof(ma_float) * DEVICE_CHANNELS, n_read, stdout);
                if (n == 0 || feof(stdout)) {
                    exit(EXIT_FAILURE);
                }

                n_written += n;
            }

        } else { msleep(100); }
    }

    if (device) ma_device_uninit(device);
    ma_decoder_uninit(&decoder);
}

void stdin_audio_callback(ma_device* dev, void* out, const void* in, ma_uint32 frame_count) 
{
    memset(out, 0, frame_count * DEVICE_CHANNELS * sizeof(ma_float));
    ma_pcm_rb *ring = (ma_pcm_rb *)dev->pUserData;
    if (!ring) return;

    ma_uint32 frames_read = frame_count;
    void* ptr;
    ma_pcm_rb_acquire_read(ring, &frames_read, &ptr);
    memcpy((ma_float*)out, ptr, frames_read * DEVICE_CHANNELS * sizeof(ma_float));

    if (frames_read < frame_count) { // handle ring buffer wrapping
        ma_pcm_rb_commit_read(ring, frames_read);
        ma_uint32 offset = frames_read;
        frames_read = frame_count - frames_read;
        ma_pcm_rb_acquire_read(ring, &frames_read, &ptr);
        memcpy((ma_float*)out + offset * DEVICE_CHANNELS, ptr, frames_read * DEVICE_CHANNELS * sizeof(ma_float));
    }

    ma_pcm_rb_commit_read(ring, frames_read);
}

void play_from_stdin() 
{
    //setbuf(stdin, 0);
    ma_pcm_rb ring;
    ma_result result = ma_pcm_rb_init(DEVICE_FORMAT, DEVICE_CHANNELS, 200000, NULL, NULL, &ring);

    if (result != MA_SUCCESS) {
        fprintf(stderr, "error: Could not allocate ring buffer\n");
        exit(EXIT_FAILURE);
    }

    ma_device *device = get_and_start_device(
        DEVICE_FORMAT, DEVICE_CHANNELS, DEVICE_SAMPLE_RATE, 
        stdin_audio_callback, &ring
    );

    ma_float temp[CHUNK_SIZE * DEVICE_CHANNELS];
    while (1) {
        size_t frames_read = fread(temp, sizeof(ma_float) * DEVICE_CHANNELS, CHUNK_SIZE, stdin);
        if (!frames_read || feof(stdin)) {
            break;
        }

        size_t frames_written = 0;
        while (frames_written < frames_read) {
            ma_uint32 frames_available = frames_read - frames_written;
            ma_float *ptr;
            ma_pcm_rb_acquire_write(&ring, &frames_available, (void**)&ptr);

            if (frames_available == 0) {
                msleep(1);
                continue;
            }

            memcpy(ptr, temp + frames_written * DEVICE_CHANNELS,
                   frames_available * DEVICE_CHANNELS * sizeof(ma_float));
            ma_pcm_rb_commit_write(&ring, frames_available);
            frames_written += frames_available;
        }
    }
    ma_device_uninit(device);
}

int cstrequals(char *s1, char *s2) 
{
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return *s1 == *s2;
}

_Noreturn void usage(char *name)
{
    printf("usage: %s [FILENAME [-loop]]\n", name);
    printf("Plays the current input out of your speakers\n\n");
    printf("Parameters:\n");
    printf("FILENAME: When passing a FILENAME it will be played.\n");
    printf("          Otherwise raw data from stdin will be sent to the audio device.\n");
    printf("   -loop: The player repeats the input forever.\n");
    printf("          Only valid if you pass FILENAME.\n");
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
    if (argc > 3) {
        fprintf(stderr, "error: Wrong parameter count!\n");
        usage(argv[0]);
    }

    int loop = 0;
    char *filename = 0;
    int file_output = !isatty(STDOUT_FILENO);

    if (argc == 1) {
        if (file_output) {
            fprintf(stderr, "error: using %s as an passthrough filter\n", argv[0]);
            usage(argv[0]);
        }

        puts("playing from stdin");
        play_from_stdin();
        return EXIT_SUCCESS;
    }

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-loop")) {
            if (argc != 3) {
                fprintf(stderr, "error: Must pass FILENAME when passing -loop!\n");
                usage(argv[0]);
            }
            loop = 1;
        } else {
            filename = argv[i];
        }
    }

    if (filename) play_from_file(filename, loop, file_output);
    else assert(0 && "unreachable");

    return EXIT_SUCCESS;
}
