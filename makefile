all: build/nd_play build/nd_synth


build/nd_play: build/play.o build/miniaudio.o
	clang -o $@ build/play.o build/miniaudio.o -lm

build/nd_synth: build/miniaudio.o build/synth.o
	clang -o $@ build/synth.o build/miniaudio.o -lm

build/%.o : %.c
	@mkdir -p build
	clang -c $< -o $@

.PHONY: clean

clean:
	rm -rf build/*
