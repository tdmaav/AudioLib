# AudioLib

## Usage:

```c++
#include "AudioLib.h"

manager = new AudioManager();

audio = manager->load("ocean.ogg", true);
audio->volume = 0.5f;
audio->play();
```
