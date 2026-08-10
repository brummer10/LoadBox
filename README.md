# LoadBox

<p align="center">
    <img src="https://github.com/brummer10/LoadBox/blob/main/LoadBox.png?raw=true" />
</p>

A lightweight stereo Impulse Response and NAM profile loader plugin,
split out from [NeuralRack](https://github.com/brummer10/NeuralRack).

## Features

- True stereo processing: independent IR/NAM slot per channel, with
  a switchable Stereo / Mix mode
- Per-channel and master gain, plus a mix control between channels
  in Mix mode
- Available as CLAP, VST2 and VST3

## Supported files

It supports A1/A2 [*.nam (outboard) profiles](https://www.tone3000.com/search?gears=outboard) by using the
[NeuralAudio](https://github.com/mikeoliphant/NeuralAudio) engine.

For Impulse Response file convolution it uses [FFTConvolver](https://github.com/HiFi-LoFi/FFTConvolver).

Resampling is done by [libzita-resampler](https://kokkinizita.linuxaudio.org/linuxaudio/zita-resampler/resampler.html).

## Building

Get the source tree with submodules:

```shell
    git clone https://github.com/brummer10//LoadBox.git
    cd LoadBox
    git submodule update --init --recursive
```
Build:

```shell
    make            # builds clap, vst2 and vst3 into ../bin/
    make clap       # just the CLAP plugin
    make vst3       # just the VST3 plugin
    make vst2       # just the VST2 plugin
    make install    # installs into ~/.clap, ~/.vst, ~/.vst3 (or /usr/lib/... as root)
```

## License

BSD-3-Clause
