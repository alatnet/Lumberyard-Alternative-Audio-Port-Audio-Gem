place libsamplerate and port audio library files here.

File Structure:
```
3rdParty/
├── libsamplerate
│   ├── bin
│   │   └── libsamplerate-0.dll
│   ├── include
│   │   └── samplerate.h
│   └── lib
│       └── libsamplerate-0.lib
├── portaudio
│   ├── bin
│   │   ├── Debug
│   │   │   └── portaudio_x64.dll
│   │   └── Release
│   │       └── portaudio_x64.dll
│   ├── include
│   │   ├── pa_asio.h
│   │   ├── pa_jack.h
│   │   ├── pa_linux_alsa.h
│   │   ├── pa_mac_core.h
│   │   ├── pa_win_ds.h
│   │   ├── pa_win_wasapi.h
│   │   ├── pa_win_waveformat.h
│   │   ├── pa_win_wdmks.h
│   │   ├── pa_win_wmme.h
│   │   └── portaudio.h
│   └── lib
        ├── Debug
        │   ├── portaudio_x64.lib
        │   └── portaudio_x64.pdb
        └── Release
            ├── portaudio_x64.lib
            └── portaudio_x64.pdb
```