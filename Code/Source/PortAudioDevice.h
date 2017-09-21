#pragma once

#include "PortAudioCommon.h"
#include <AlternativeAudio\Device\OAudioDevice.h>

namespace PortAudio {
	class PortAudioDevice : public AlternativeAudio::OAudioDevice {
	public:
		AZ_RTTI(PortAudioDevice, "{9CAB599F-6A78-4389-B4B9-C775F1EC64A8}", AlternativeAudio::OAudioDevice);
	public:
		PortAudioDevice();
		PortAudioDevice(long long deviceIndex, double samplerate, AlternativeAudio::AudioFrame::Type audioFormat, void * userdata);
		~PortAudioDevice();
	public:
		void SetStream(double samplerate, AlternativeAudio::AudioFrame::Type audioFormat, void * userdata);
		void SetResampleQuality(AlternativeAudio::AAResampleQuality quality);
		AlternativeAudio::OAudioDeviceInfo GetDeviceInfo() { return this->m_info; }
	public:
		long long PlaySource(AlternativeAudio::IAudioSource * source);
		void PauseSource(long long id);
		void ResumeSource(long long id);
		void StopSource(long long id);
		bool IsPlaying(long long id);
		AlternativeAudio::AudioSourceTime GetTime(long long id);
		void SetTime(long long id, double time);
	public:
		void Queue(bool startstop);
	public:
		void SetMaster(bool onoff);
	private:
		bool m_isMaster;
		bool m_isQueue;
	public:
		static void Reflect(AZ::SerializeContext* serialize) {
			serialize->Class<PortAudioDevice, OAudioDevice>()
				->Version(0)
				->SerializerForEmptyClass();
		}

		static void Behavior(AZ::BehaviorContext* behaviorContext) {
		}
	private:
		AlternativeAudio::OAudioDeviceInfo m_info;
	private:
		struct PlayingAudioSource {
			AlternativeAudio::IAudioSource* audioSource{ nullptr };
			long long currentFrame{ 0 };
			long long startFrame{ 0 };
			long long endFrame{ 0 };
			bool loop{ false };
			bool paused{ false };
			float vol{ 1.0f };
			AlternativeAudio::AAFlagHandler flags;
		};
	private:
		typedef AZStd::unordered_map<long long, PlayingAudioSource *> PlayingAudioSourcesMap;
		PlayingAudioSourcesMap m_playingAudioSource;

		long long m_nextPlayID;

		std::vector<long long> m_stoppedAudioFiles;
	private: //audio stream
		PaStream *m_pAudioStream;
		AlternativeAudio::AudioFrame::Type m_audioFormat;
		long long m_device;
		void * m_hostApiSpecificStreamInfo;
	private: //samplerate conversion
		double m_sampleRate;
		SRC_STATE * m_pSrcState;
		AlternativeAudio::AAResampleQuality m_rsQuality;
	private: //mutexes
		AZStd::mutex m_callbackMutex, m_errorMutex;
	private:
		static AlternativeAudio::AudioFrame::Frame * CreateBuffer(AlternativeAudio::AudioFrame::Type type, long long length);
		static int getNumberOfChannels(AlternativeAudio::AudioFrame::Type type);
	protected:
		int paCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData);
	public:
		static int paCallbackCommon(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData);

	private:
		enum QueueType { PLAY,PAUSE,RESUME,STOP };
		class QueuedCommand {
		public:
			QueuedCommand(long long uID, PortAudioDevice* pad) : m_uID(uID), m_pad(pad) {}
		public:
			virtual QueueType getType() = 0;
			virtual void Execute() = 0;
		public:
			long long m_uID;
			PortAudioDevice* m_pad;
		};

		class PlayQueueCommand : public QueuedCommand {
		public:
			PlayQueueCommand(long long uID, PlayingAudioSource* pasrc, PortAudioDevice* pad) : QueuedCommand(uID, pad), m_pasrc(pasrc) {}
			void Execute();
			QueueType getType() { return PLAY; }
		public:
			PlayingAudioSource* m_pasrc;
		};

		class PauseQueueCommand : public QueuedCommand {
		public:
			PauseQueueCommand(long long uID, PortAudioDevice* pad) : QueuedCommand(uID, pad) {}
			void Execute();
			QueueType getType() { return PAUSE; }
		};

		class ResumeQueueCommand : public QueuedCommand {
		public:
			ResumeQueueCommand(long long uID, PortAudioDevice* pad) : QueuedCommand(uID, pad) {}
			void Execute();
			QueueType getType() { return RESUME; }
		};

		class StopQueueCommand : public QueuedCommand {
		public:
			StopQueueCommand(long long uID, PortAudioDevice* pad) : QueuedCommand(uID, pad) {}
			void Execute();
			QueueType getType() { return STOP; }
		};

	private:
		AZStd::vector<QueuedCommand *> m_queueCommands;
	};
}