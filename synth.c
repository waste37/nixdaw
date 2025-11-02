#include <assert.h>
#include <stdio.h>
#include <sys/time.h>

#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include <errno.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define MS_TO_NS 1000000L

typedef float audio_format;

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

#define PI 3.14159265359f

enum wave_type {
    WAVE_TYPE_SINE = 0,
    WAVE_TYPE_SQUARE,
    WAVE_TYPE_NOISE
};

typedef struct {
    double amplitude; // 0; 1
    double frequency; // Hz
    int type;
} wave;

typedef struct {
    wave w;
    int format;
    size_t channels;
    size_t samplerate; 

    double phase;
    size_t buflen;
    audio_format *buf;
} wave_generator;

wave create_wave(int type, double amplitude, double frequency)
{ 
    return (wave){
        .type = type,
        .amplitude = fmax(-1, fmin(amplitude, 1)),
        .frequency = frequency
    };
}

audio_format sample_wave(wave w, double phase) 
{
    switch (w.type) {
        case WAVE_TYPE_SINE: {
            return sin(phase) * w.amplitude;
        }
        case WAVE_TYPE_SQUARE: {
            return copysign(1.0, sin(phase) * w.amplitude);
        }
        case WAVE_TYPE_NOISE: {
            double s = rand() % INT32_MAX;
            s = s / (INT32_MAX * 0.5);
            s -= 0.5;
            return s;
        }
        default: assert(0 && "Unreachable");
    }
}

wave_generator create_wave_generator(wave w, size_t channels, size_t samplerate)
{ 
    wave_generator gen = {0};
    gen.w = w;
    gen.phase = 0.0;
    gen.channels = channels;
    gen.samplerate = samplerate;

    if (gen.samplerate > SIZE_MAX / gen.channels) {
        gen.buf = NULL;
        gen.buflen = 0;
        return gen;
    }

    gen.buflen = gen.samplerate * gen.channels;
    gen.buf = malloc(sizeof(audio_format) * gen.buflen);
    return gen;
}

int generate_wave(wave_generator *gen) 
{
    if (!gen || !gen->buf) return 0;
    double tau = 2.0 * M_PI;
    double phase_increment = tau * (gen->w.frequency / (double)gen->samplerate);
    size_t buf_offset = 0;

    for (size_t frame = 0; frame < gen->samplerate; ++frame) { 
        for (size_t ch = 0; ch < gen->channels; ++ch) {
            gen->buf[buf_offset++] = sample_wave(gen->w, gen->phase);
        }

        gen->phase += phase_increment;
        if (gen->phase >= tau) gen->phase -= tau;
        else if (gen->phase < 0.0) gen->phase += tau;
    }
    return 1;
}

struct additive_synth {
    wave w1;
    wave w2;
};

#define CHUNK_SIZE 2048

static double keyboard_notes[] = {
    [' '] = 0,
    ['a'] = 65.4,  // C3
    ['s'] = 73.4,  // D3
    ['d'] = 82.4,  // E3
    ['f'] = 87.4,  // F3
    ['g'] = 98.0,  // G3
    ['h'] = 110.0,  // A3
    ['j'] = 123.5,  // B3
    ['k'] = 130.8,  // C4
    ['l'] = 146.8,  // D4
};

int main() 
{
    wave sine = create_wave(WAVE_TYPE_NOISE, 1.0, 440);
    wave_generator generator = create_wave_generator(sine, 2, 48000);
    setbuf(stdout, 0);
// this has to move to another thread. In the main one we will only have input handling...
    size_t written_total = 0;
    while (1) {
        //char c = getchar();
        //generator.w = create_wave(WAVE_TYPE_SQUARE, 1.0, keyboard_notes[c]);
        generate_wave(&generator);
        size_t written = 0;
        while (written < generator.buflen) {
            size_t frames_remaining = (generator.buflen - written) / generator.channels;
            size_t frames_to_write = (frames_remaining > CHUNK_SIZE) ? CHUNK_SIZE : frames_remaining;
            size_t items_to_write = frames_to_write * generator.channels;
            if (items_to_write == 0) break;
            size_t n = fwrite(generator.buf + written, sizeof(audio_format), items_to_write, stdout);
            if (n == 0) {
                if (ferror(stdout)) {
                    perror("fwrite");
                    clearerr(stdout);
                    return EXIT_FAILURE;
                }
            }
            written += n;
        }
    }

    fclose(stdout);
}
