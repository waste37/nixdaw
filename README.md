## *Nix Style DAW
This project aims to build a suite of commands that can turn your shell in a simple DAW via unix shell idioms of simplicity and composability.

This project is developed using the miniaudio library, great thanks to the developers!!

## The Core Idea:

We could play a drum loop and play a midi melody with our keyboard like follows:

```sh
DRUMS='nd_play drumloop.mp3 -loop | nd_equalizer -p drums'
MELODY='nd_midiin | nd_synth -p epiano | nd_reverb -p dream'
nd_mixer -lane $MELODY -lane $DRUMS -gains '-2.0,-1.0' -send-stdin 0  | nd_play
```

We could record a midi file while hearing it like follows:
```sh
$ nd_midiin -o line.midi | nd_synth -p "E piano" | nd_play
```

And maybe play it back with a different instrument:
```sh
cat line.midi | synth -p "Dreamy Pad" | nd_play
```

You could record a raw guitar input to file while hearing it with effects:
```sh
nd_audioin -in "Audio Input 1" -o out.wav | nd_amp -p clean | nd_reverb -p boxy | nd_play 
```

Or record the guitar after adding the effects as follows:
```sh
GUITAR='nd_audioin -in "Audio Channel 1" | nd_amp -p clean | nd_reverb -p boxy' 
mixer -lane $GUITAR -monitor 0 | nd_waveencode -o out.wav
```

With these tools making audio tracks to put inside audacity and build a song is pretty easy.
