# AudioLib

## Usage:

```c++
#include "AudioLib.h"

manager = new AudioManager();

sound = manager->load("ocean.ogg", true);
sound->volume = 0.5f;
sound->play();
```
