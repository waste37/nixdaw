#define MA_IMPLEMENTATION
#define MA_NO_ENCODING
#include "miniaudio.h"

#include <assert.h>
#include <stdio.h>

#define DEVICE_FORMAT       ma_format_f32
#define DEVICE_CHANNELS     2
#define DEVICE_SAMPLE_RATE  48000

int finished = 0;

void decoder_audio_callback(ma_device* dev, void* out, const void* in, ma_uint32 frame_count)
{
    ma_decoder* decoder = (ma_decoder*)dev->pUserData;
    if (!decoder) return;

    ma_uint64 frames_read = 0;
    ma_result result = ma_data_source_read_pcm_frames(decoder, out, frame_count, &frames_read);
    if (result != MA_SUCCESS) {
        finished = 1;
    }

    if (frames_read < frame_count) {
        finished = 1;
    }

    (void)in;
}

void stdin_audio_callback(ma_device* dev, void* out, const void* in, ma_uint32 frame_count) 
{
    size_t samples_count = frame_count * DEVICE_CHANNELS;
    size_t bytes_count = samples_count * sizeof(float);
    size_t bytes_read = fread(out, 1, bytes_count, stdin);
    if (bytes_read < bytes_count) {
        finished = 1;
    }
    if (feof(stdin)) {
        finished = 1;
    }
}

void play_from_file(char *filename, int loop) 
{
    ma_result result;
    ma_decoder decoder;
    ma_device_config device_config;
    ma_device device;

    result = ma_decoder_init_file(filename, 0, &decoder);
    if (result != MA_SUCCESS) {
        printf("Could not load file: %s\n", filename);
        exit(EXIT_FAILURE);
    }

    if (loop) ma_data_source_set_looping(&decoder, MA_TRUE);

    device_config = ma_device_config_init(ma_device_type_playback);
    device_config.playback.format   = decoder.outputFormat;
    device_config.playback.channels = decoder.outputChannels;
    device_config.sampleRate        = decoder.outputSampleRate;
    device_config.dataCallback      = decoder_audio_callback;
    device_config.pUserData         = &decoder;

    if (ma_device_init(0, &device_config, &device) != MA_SUCCESS) {
        fprintf(stderr, "Failed to open playback device.\n");
        ma_decoder_uninit(&decoder);
        exit(EXIT_FAILURE);
    }

    if (ma_device_start(&device) != MA_SUCCESS) {
        fprintf(stderr, "Failed to start playback device.\n");
        ma_device_uninit(&device);
        ma_decoder_uninit(&decoder);
        exit(EXIT_FAILURE);
    }

    while (!finished) ma_sleep(100); 
    ma_device_uninit(&device);
    ma_decoder_uninit(&decoder);
}

void play_from_stdin() 
{
    ma_result result;
    ma_device_config device_config;
    ma_device device;

    device_config = ma_device_config_init(ma_device_type_playback);
    device_config.playback.format   = DEVICE_FORMAT;
    device_config.playback.channels = DEVICE_CHANNELS;
    device_config.sampleRate        = DEVICE_SAMPLE_RATE;
    device_config.dataCallback      = stdin_audio_callback;

    if (ma_device_init(0, &device_config, &device) != MA_SUCCESS) {
        fprintf(stderr, "Failed to open playback device.\n");
        exit(EXIT_FAILURE);
    }

    if (ma_device_start(&device) != MA_SUCCESS) {
        fprintf(stderr, "Failed to start playback device.\n");
        ma_device_uninit(&device);
        exit(EXIT_FAILURE);
    }

    while (!finished) ma_sleep(100); 
    ma_device_uninit(&device);
}

int cstrequals(char *s1, char *s2) {
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

    if (argc == 1) {
        play_from_stdin();
        return EXIT_SUCCESS;
    }

    int loop = 0;
    char *filename = 0;
    // either 2 or 3.
  
    for (int i = 1; i < argc; ++i) {
        if (cstrequals(argv[i], "-loop")) {
            if (argc != 3) {
                fprintf(stderr, "error: Must pass FILENAME when passing -loop!\n");
                usage(argv[0]);
            }
            loop = 1;
        } else {
            filename = argv[i];
        }
    }

    if (filename) play_from_file(filename, loop);
    else assert(0 && "unreachable");
}
