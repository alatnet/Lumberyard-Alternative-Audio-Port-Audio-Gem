#pragma once

//for sample rate conversion
//http://www.mega-nerd.com/SRC/index.html
#include <samplerate.h>

//build for a specific platform
#if defined(WIN32) || defined(__MINGW32__) || defined(__MINGW64__ ) || defined(BOOST_OS_WINDOWS)
	#define PSA_WIN
#elif defined(__linux__) || defined(__unix__ ) || defined(BOOST_OS_LINUX) || defined(BOOST_OS_UNIX)
	#define PSA_NIX
#elif defined(__APPLE__) || defined(BOOST_OS_MACOS)
	#define PSA_MAC
#endif

#define PAS_Millsecond(millisecond) (double)((double)millisecond/(double)1000.0f)

//port audio system
#include <portaudio.h>
#ifdef PAS_WIN
	#include <pa_win_wmme.h>
	#include <pa_win_ds.h>
	#include <pa_win_wdmks.h>
	#include <pa_asio.h>
	#include <pa_win_wasapi.h>
	#include <pa_win_waveformat.h>
#elif PAS_MAC
	#include <pa_mac_core.h>
	#include <pa_jack.h>
#elif PAS_NIX
	#include <pa_linux_alsa.h>
	#include <pa_jack.h>
#endif