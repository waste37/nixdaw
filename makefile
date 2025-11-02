CFLAGS=-Wall -Wextra

all: build/nd_play build/nd_synth build/nd_filter


build/nd_play: build/play.o build/miniaudio.o
	clang -o $@ build/play.o build/miniaudio.o -lm $(CFLAGS)

build/nd_filter: build/filter.o build/miniaudio.o
	clang -o $@ build/filter.o build/miniaudio.o -lm $(CFLAGS)

build/nd_synth: build/miniaudio.o build/synth.o
	clang -o $@ build/synth.o build/miniaudio.o -lm $(CFLAGS)

build/%.o : %.c
	@mkdir -p build
	clang -c $< -o $@ $(CFLAGS)

.PHONY: clean

clean:
	rm -rf build/*
