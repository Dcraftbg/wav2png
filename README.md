# wav2png

wav2png is a simple wav to png converter written in C.

## Quickstart
```sh
make
./wav2png myWavFile.wav -o myWavFile.png
```

## Modes
Currently supported modes include:
- raw    - raw wave f32 data as ARGB
- gray   - grayscale data as 1 byte colors
- colors - full colors data as ARGB
- heat   - heatmap data as ARGB
