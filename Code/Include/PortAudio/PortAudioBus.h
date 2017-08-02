#pragma once

#include <AzCore/EBus/EBus.h>
#include <AlternativeAudio\IAudioSource.h>

namespace PortAudio {
	struct PASError {
		int code;
		const char * str;

		PASError() :
			code(0),
			str("NoError") {
		};
	};

	enum EAudioSection {
		eAS_Music,
		eAS_SFX,
		eAS_Voice,
		eAS_Environment,
		eAS_UI,
		eAS_Count
	};

	enum EAudioResampleQuality {
		eARQ_Best,
		eARQ_Medium,
		eARQ_Fastest,
		eARQ_Zero_Order_Hold,
		eARQ_Linear
	};

	struct PortAudioDevice {
		AZStd::string name;
		int maxOutputChannels;
		double lowLatancy;
		double highLatency;
		double defaultSampleRate;
		AZ_RTTI(PortAudioDevice, "{39F2DC01-980A-4B47-9D52-7C22A9DDADDB}");

		AZStd::string GetName() { return this->name; }
		int GetMaxChannels() { return this->maxOutputChannels; }
		double GetDefaultSampleRate() { return this->defaultSampleRate; }
	};

	class PortAudioRequests
		: public AZ::EBusTraits {
	public:
		static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
		static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
	public: //stream control
		virtual void SetStream(double samplerate, AlternativeAudio::AudioFrame::Type audioFormat, int device, void * hostApiSpecificStreamInfo) = 0;
		virtual void SetResampleQuality(EAudioResampleQuality quality) = 0;
		virtual int GetDefaultDevice() = 0;
		virtual PortAudioDevice GetDefaultDeviceInfo() = 0;
		virtual AZStd::vector<PortAudioDevice> GetDevices() = 0;
	public: //audio source control
		virtual long long PlaySource(AlternativeAudio::IAudioSource * source, EAudioSection section) = 0;
		virtual void PauseSource(long long id) = 0;
		virtual void ResumeSource(long long id) = 0;
		virtual void StopSource(long long id) = 0;
		virtual bool IsPlaying(long long id) = 0;
		virtual AlternativeAudio::AudioSourceTime GetTime(long long id) = 0;
		virtual void SetTime(long long id, double time) = 0;
	public: //volume control
		virtual void SetMasterVol(float vol) = 0;
		virtual float GetMasterVol() = 0;
		virtual void SetVolume(float vol, EAudioSection section) = 0;
		virtual float GetVolume(EAudioSection section) = 0;
		virtual void SetSourceVolume(float vol, long long id) = 0;
		virtual float GetSourceVolume(long long id) = 0;
	public: //error checking
		virtual bool HasError() = 0;
		virtual PASError GetError() = 0;
	};
	using PortAudioRequestBus = AZ::EBus<PortAudioRequests>;
} // namespace PortAudio
