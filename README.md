# AudioLib
Simple C++ lightweight audio library

## Features
* support for Android & iOS
* support for OGG, WAV & generative sounds
* seamless loop playback
* header-only

## Limitations
* no streaming
* only 44100, 22050 and 11025 sample rates are supported

## Roadmap
* fft based filters
* streaming
* mp3 support

## Usage:

```c++
#include "AudioLib.h"

manager = new AudioLib::Manager();

sound = manager->load("ocean.ogg", -1);
sound->volume = 0.5f;
sound->pan = 0.25f;
sound->play();
```
