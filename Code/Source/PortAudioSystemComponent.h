#pragma once

#include <AzCore/Component/Component.h>

#include <PortAudio/PortAudioBus.h>
#include <AlternativeAudio/IErrorHandler.h>


#include "PortAudioCommon.h"

namespace PortAudio
{
    class PortAudioSystemComponent
        : public AZ::Component
        , protected PortAudioRequestBus::Handler
		, public AlternativeAudio::IErrorHandler
    {
	public:
		PortAudioSystemComponent();
		~PortAudioSystemComponent();
    public:
        AZ_COMPONENT(PortAudioSystemComponent, "{1E19EC45-499C-47EE-8FC7-915F6916FF48}");

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

    protected:
        ////////////////////////////////////////////////////////////////////////
        // PortAudioRequestBus interface implementation
	protected: //stream control
		void SetStream(double samplerate, AlternativeAudio::AudioFrame::Type audioFormat, int device, void * hostApiSpecificStreamInfo);
		void SetResampleQuality(EAudioResampleQuality quality);
		int GetDefaultDevice() { return Pa_GetDefaultOutputDevice(); }
		PortAudioDevice GetDefaultDeviceInfo() { return this->devices[Pa_GetDefaultOutputDevice()]; }
		AZStd::vector<PortAudioDevice> GetDevices() { return this->devices; }
	protected: //audio source control
		long long PlaySource(AlternativeAudio::IAudioSource * source, EAudioSection section);
		void PauseSource(long long id);
		void ResumeSource(long long id);
		void StopSource(long long id);
		bool IsPlaying(long long id);
		AlternativeAudio::AudioSourceTime GetTime(long long id);
		void SetTime(long long id, double time);
	/*protected: //volume control
		void SetMasterVol(float vol);
		float GetMasterVol();
		void SetVolume(float vol, EAudioSection section);
		float GetVolume(EAudioSection section);
		void SetSourceVolume(float vol, long long id);
		float GetSourceVolume(long long id);*/
        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // AZ::Component interface implementation
        void Init() override;
        void Activate() override;
        void Deactivate() override;
        ////////////////////////////////////////////////////////////////////////
	private:
		static int m_initializeCount;
		static bool m_initialized;
		bool m_devicesEnumerated;
	private:
		struct PlayingAudioSource {
			AlternativeAudio::IAudioSource* audioSource{ nullptr };
			long long currentFrame{ 0 };
			long long startFrame{ 0 };
			long long endFrame{ 0 };
			bool loop{ false };
			bool paused{ false };
			EAudioSection section{ eAS_Music };
			float vol{ 1.0f };
		};
	private:
		typedef AZStd::unordered_map<long long, PlayingAudioSource *> PlayingAudioSourcesMap;
		PlayingAudioSourcesMap m_playingAudioSource;

		long long m_nextPlayID;

		std::vector<long long> m_stoppedAudioFiles;
	private:
		//float m_masterVol;// , m_musicVol, m_sfxVol;
		//float m_vols[eAS_Count];
		//AZStd::vector<float> m_vols;
	public:
		bool HasError() { return AlternativeAudio::IErrorHandler::HasError(); }
		AlternativeAudio::IError GetError() { return AlternativeAudio::IErrorHandler::GetError(); }
	private: //audio stream
		PaStream *m_pAudioStream;
		AlternativeAudio::AudioFrame::Type m_audioFormat;
		int m_device;
		void * m_hostApiSpecificStreamInfo;
	private: //samplerate conversion
		double m_sampleRate;
		SRC_STATE * m_pSrcState;
		EAudioResampleQuality m_rsQuality;
	private: //mutexes
		AZStd::mutex m_callbackMutex, m_errorMutex;
	protected:
		int paCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData);
	public:
		static int paCallbackCommon(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData);
	public:
		AZStd::vector<PortAudioDevice> devices;
    };
}
