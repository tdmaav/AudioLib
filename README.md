# AudioLib
Simple lightweight header-only audio library for iOS

## Features
* support for OGG & WAV
* seamless loop playback

## Usage:

```c++
#include "AudioLib.h"

manager = new AudioLib::Manager();

sound = manager->load("ocean.ogg", true);
sound->volume = 0.5f;
sound->play();
```
