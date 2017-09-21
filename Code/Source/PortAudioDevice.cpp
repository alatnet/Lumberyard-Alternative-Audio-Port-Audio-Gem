#include "StdAfx.h"

#include <PortAudioDevice.h>
#include <PortAudio\PortAudioUserData.h>
#include <AlternativeAudio\AlternativeAudioBus.h>
#include "PortAudioInternalBus.h"

namespace PortAudio {
	PortAudioDevice::PortAudioDevice() {
		m_nextPlayID = 0;
		m_pSrcState = nullptr;
		m_pAudioStream = nullptr;
		m_device = Pa_GetDefaultOutputDevice();
		m_rsQuality = AlternativeAudio::eAARQ_Linear;

		m_isMaster = false;

		const PaDeviceInfo * info = Pa_GetDeviceInfo((PaDeviceIndex)this->m_device);

		this->m_info.name = AZStd::string(info->name);
		this->m_info.maxChannels = info->maxOutputChannels;
		this->m_info.defaultSampleRate = info->defaultSampleRate;

		this->SetStream(44100.0, AlternativeAudio::AudioFrame::Type::eT_af2, nullptr);
	}
	PortAudioDevice::PortAudioDevice(long long deviceIndex, double samplerate, AlternativeAudio::AudioFrame::Type audioFormat, void * userdata) {
		m_nextPlayID = 0;
		m_pSrcState = nullptr;
		m_pAudioStream = nullptr;
		m_device = deviceIndex;
		m_rsQuality = AlternativeAudio::eAARQ_Linear;

		const PaDeviceInfo * info = Pa_GetDeviceInfo((PaDeviceIndex)this->m_device);

		this->m_info.name = AZStd::string(info->name);
		this->m_info.maxChannels = info->maxOutputChannels;
		this->m_info.defaultSampleRate = info->defaultSampleRate;

		this->SetStream(samplerate, audioFormat, userdata);
	}

	PortAudioDevice::~PortAudioDevice() {
		//close device
		if (this->m_pAudioStream) {
			Pa_StopStream(this->m_pAudioStream);
			Pa_CloseStream(this->m_pAudioStream);
			this->m_pAudioStream = nullptr;
		}

		if (this->m_pSrcState) {
			src_delete(this->m_pSrcState);
			this->m_pSrcState = nullptr;
		}

		//notify component that device is closing
		EBUS_EVENT(PortAudioInternalNotificationsBus, CloseDevice, this->m_device);
	}

	void PortAudioDevice::SetStream(double samplerate, AlternativeAudio::AudioFrame::Type audioFormat, void* userdata) {
		//if (device == -1) device = Pa_GetDefaultOutputDevice();

		PortAudioUserData* paUserData = static_cast<PortAudioUserData*>(userdata);

		int streamActive = 0;
		if (this->m_pAudioStream) {
			streamActive = Pa_IsStreamActive(this->m_pAudioStream);

			//stop stream if it isnt stopped
			if (Pa_IsStreamStopped(this->m_pAudioStream) != 1) Pa_StopStream(this->m_pAudioStream);

			//close stream
			Pa_CloseStream(this->m_pAudioStream);
			this->m_pAudioStream = nullptr;
		}

		this->m_sampleRate = samplerate;
		//this->m_hostApiSpecificStreamInfo = hostApiSpecificStreamInfo;
		if (paUserData) this->m_hostApiSpecificStreamInfo = paUserData->hostApiSpecificStreamInfo;
		else this->m_hostApiSpecificStreamInfo = nullptr;

		const PaDeviceInfo * info = Pa_GetDeviceInfo((PaDeviceIndex)this->m_device);

		PaStreamParameters streamParams;
		streamParams.device = (PaDeviceIndex)this->m_device; //set device to use
		streamParams.channelCount = PortAudioDevice::getNumberOfChannels(audioFormat);
		streamParams.hostApiSpecificStreamInfo = this->m_hostApiSpecificStreamInfo;
		streamParams.sampleFormat = paFloat32; // 32bit float format
		//streamParams.suggestedLatency = PAS_Millsecond(200); //200 ms ought to satisfy even the worst sound card - effects delta time (higher ms - higher delta)

		if (paUserData && paUserData->suggestedLatency != -1.0) streamParams.suggestedLatency = paUserData->suggestedLatency;
		else streamParams.suggestedLatency = info->defaultLowOutputLatency;

		this->m_audioFormat = audioFormat;

		unsigned long streamFlags = paNoFlag; /* no special modes (clip off, dither off) */
		if (paUserData && paUserData->streamFlags != 0) streamFlags = paUserData->streamFlags;

		int err = Pa_OpenStream(
			&this->m_pAudioStream,
			0, // no input
			&streamParams,
			this->m_sampleRate,
			paFramesPerBufferUnspecified, // let portaudio choose the buffersize
			streamFlags,
			PortAudioDevice::paCallbackCommon, //set the callback to be the function that calls the per class callback
			this
		);

		if (err != paNoError) {
			const char * errStr = Pa_GetErrorText(err);
			pushError(err, errStr);
			AZ_Printf("[PortAudio]", "[PortAudio] Error opening stream: %s\n", errStr);
			this->m_pAudioStream = nullptr;
		}

		SetResampleQuality(this->m_rsQuality);

		//restart stream if it was active
		if (streamActive == 1) Pa_StartStream(this->m_pAudioStream);

		this->m_info.currentFormat = audioFormat;
		this->m_info.currentSampleRate = samplerate;
	}

	void PortAudioDevice::SetResampleQuality(AlternativeAudio::AAResampleQuality quality) {
		this->m_rsQuality = quality;

		int qual = SRC_SINC_BEST_QUALITY;
		switch (quality) {
		case AlternativeAudio::eAARQ_Best:
			qual = SRC_SINC_BEST_QUALITY;
			break;
		case AlternativeAudio::eAARQ_Medium:
			qual = SRC_SINC_MEDIUM_QUALITY;
			break;
		case AlternativeAudio::eAARQ_Fastest:
			qual = SRC_SINC_FASTEST;
			break;
		case AlternativeAudio::eAARQ_Zero_Order_Hold:
			qual = SRC_ZERO_ORDER_HOLD;
			break;
		case AlternativeAudio::eAARQ_Linear:
		default:
			qual = SRC_LINEAR;
			break;
		}

		if (this->m_pSrcState) {
			src_delete(this->m_pSrcState);
			this->m_pSrcState = nullptr;
		}

		this->m_callbackMutex.lock();
		int src_err;
		this->m_pSrcState = src_new(qual, PortAudioDevice::getNumberOfChannels(this->m_audioFormat), &src_err); //create a new sample rate converter

		if (this->m_pSrcState == NULL) {
			const char * errStr = src_strerror(src_err);
			AZ_Printf("[PortAudio]", "[PortAudio] Error opening sample rate converter: %s\n", errStr);
			this->pushError(src_err, errStr);
		}
		this->m_callbackMutex.unlock();

		this->m_info.currentResampleQuality = quality;
	}

	long long PortAudioDevice::PlaySource(AlternativeAudio::IAudioSource * source) {
		//push new audio source to playing sources
		if (source == nullptr || source == NULL) return -1;

		long long uID = this->m_nextPlayID++;

		long long flags = source->GetFlags();

		PlayingAudioSource *playingSource = new PlayingAudioSource();
		playingSource->audioSource = source;
		playingSource->startFrame = 0;
		playingSource->currentFrame = 0;
		playingSource->endFrame = source->GetFrameLength();
		playingSource->loop = ((flags & AlternativeAudio::eAF_Loop) == 1);
		playingSource->paused = ((flags & AlternativeAudio::eAF_PausedOnStart) == 1);
		playingSource->flags.SetFlags(flags);

		this->m_callbackMutex.lock();
		this->m_playingAudioSource.insert(AZStd::make_pair<>(uID, playingSource));
		this->m_callbackMutex.unlock();

		//check if stream is stopped, if it is, start it up.
		if (Pa_IsStreamActive(this->m_pAudioStream) != 1) {
			if (Pa_IsStreamStopped(this->m_pAudioStream) != 1) Pa_StopStream(this->m_pAudioStream); //resets stream time.
			Pa_StartStream(this->m_pAudioStream);
		}

		return uID;

	}

	void PortAudioDevice::PauseSource(long long id) {
		this->m_callbackMutex.lock();
		if (this->m_playingAudioSource.find(id) == this->m_playingAudioSource.end()) {
			m_callbackMutex.unlock();
			return;
		}
		this->m_playingAudioSource[id]->paused = true;
		this->m_callbackMutex.unlock();
	}

	void PortAudioDevice::ResumeSource(long long id) {
		this->m_callbackMutex.lock();
		if (this->m_playingAudioSource.find(id) == this->m_playingAudioSource.end()) {
			m_callbackMutex.unlock();
			return;
		}
		this->m_playingAudioSource[id]->paused = false;
		this->m_callbackMutex.unlock();

		//check if stream is stopped, if it is, start it up.
		if (Pa_IsStreamActive(this->m_pAudioStream) != 1) {
			Pa_StopStream(this->m_pAudioStream); //resets stream time.
			Pa_StartStream(this->m_pAudioStream);
		}
	}

	void PortAudioDevice::StopSource(long long id) {
		this->m_callbackMutex.lock();
		if (this->m_playingAudioSource.find(id) != this->m_playingAudioSource.end()) this->m_playingAudioSource.erase(id);
		this->m_callbackMutex.unlock();
	}

	bool PortAudioDevice::IsPlaying(long long id) {
		this->m_callbackMutex.lock();
		if (this->m_playingAudioSource.find(id) != this->m_playingAudioSource.end()) {
			bool paused = this->m_playingAudioSource[id]->paused;
			this->m_callbackMutex.unlock();
			return !paused;
		}
		this->m_callbackMutex.unlock();

		return false;
	}

	AlternativeAudio::AudioSourceTime PortAudioDevice::GetTime(long long id) {
		long long currTime = 0;
		this->m_callbackMutex.lock();
		if (this->m_playingAudioSource.find(id) != this->m_playingAudioSource.end()) currTime = this->m_playingAudioSource[id]->currentFrame;
		this->m_callbackMutex.unlock();

		AlternativeAudio::AudioSourceTime timeLength;
		timeLength.totalSec = (1.0 * currTime) / this->m_playingAudioSource[id]->audioSource->GetSampleRate();
		timeLength.hrs = (int)(timeLength.totalSec / 3600.0);
		timeLength.minutes = (int)((timeLength.totalSec - (timeLength.hrs * 3600.0)) / 60.0);
		timeLength.sec = timeLength.totalSec - (timeLength.hrs * 3600.0) - (timeLength.minutes * 60.0);

		return timeLength;
	}

	void PortAudioDevice::SetTime(long long id, double time) {
		long long setFrame = (long long)(time * this->m_playingAudioSource[id]->audioSource->GetSampleRate());
		this->m_callbackMutex.lock();
		if (this->m_playingAudioSource.find(id) != this->m_playingAudioSource.end()) {
			if (setFrame < this->m_playingAudioSource[id]->startFrame) setFrame = this->m_playingAudioSource[id]->startFrame;
			if (setFrame > this->m_playingAudioSource[id]->endFrame) setFrame = this->m_playingAudioSource[id]->endFrame;
			this->m_playingAudioSource[id]->currentFrame = setFrame;
		}
		this->m_callbackMutex.unlock();
	}

	void PortAudioDevice::SetMaster(bool onoff) {
		this->m_callbackMutex.lock();
		m_isMaster = onoff;
		this->m_callbackMutex.unlock();
	}

	int PortAudioDevice::paCallbackCommon(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData) {
		//redirect to the individual callback. (gives us individualized and possibly multiple streams at the same time).
		return ((PortAudioDevice *)userData)->paCallback(inputBuffer, outputBuffer, framesPerBuffer, timeInfo, statusFlags, userData);
	}

	int PortAudioDevice::paCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData) {
		if (this->m_playingAudioSource.size() == 0) {
			//this->m_nextPlayID = 0;
			return paComplete; //if we have no audio sources, why are we running?
		}

		long long dflags = this->m_flags.GetFlags();

		int pachannels = PortAudioDevice::getNumberOfChannels(this->m_audioFormat);

		{ //clear the buffer
			float * outputFBuff = (float *)outputBuffer;
			for (int i = 0; i < framesPerBuffer * pachannels; i++) outputFBuff[i] = 0.0f;
		}

		//create a buffer to hold the converted channel audio source
		AlternativeAudio::AudioFrame::Frame * framesOut = PortAudioDevice::CreateBuffer(this->m_audioFormat, framesPerBuffer);

		bool allPaused = true;

		this->m_callbackMutex.lock();
		for (AZStd::pair<long long, PlayingAudioSource *> entry : this->m_playingAudioSource) {
			if (entry.second->paused) continue; //if the audio file is paused, skip it.
			allPaused = false;

			{ // clear the frames out buffer;
				for (int i = 0; i < framesPerBuffer * pachannels; i++) ((float*)framesOut)[i] = 0.0f;
			}

			//get the playing audio source
			PlayingAudioSource* playingsource = entry.second;
			playingsource->audioSource->Seek(playingsource->currentFrame); //seek to the current position of the file
			AlternativeAudio::AudioFrame::Type sourceFrameType = playingsource->audioSource->GetFrameType();
			long long prevFlags = playingsource->flags.GetFlags();

			//get the ratio of the sample rate conversion
			double ratio = this->m_sampleRate / playingsource->audioSource->GetSampleRate();

			long long frameLength = 0;

			//convert to port audio's audio frame format and resample.
			if (ratio == 1.0f || !src_is_valid_ratio(ratio)) { //if the sample rate is the same between the audio source and port audio or if it's not a valid ratio.
				AlternativeAudio::AudioFrame::Frame * srcframes = PortAudioDevice::CreateBuffer(sourceFrameType, framesPerBuffer); //create a buffer to hold the audio sources data

				frameLength = playingsource->audioSource->GetFrames(framesPerBuffer, (float *)srcframes); //get the data.
				playingsource->currentFrame += frameLength;

				//apply before convert dsp
				if (m_isMaster)
					EBUS_EVENT(
						AlternativeAudio::AlternativeAudioDSPBus,
						ProcessEffects,
						AlternativeAudio::AADSPSection::eDS_PerSource_BC,
						sourceFrameType,
						(float*)srcframes,
						framesPerBuffer,
						&playingsource->flags
					);
				else
					this->ProcessEffects(
						AlternativeAudio::AADSPSection::eDS_PerSource_BC,
						sourceFrameType,
						(float*)srcframes,
						framesPerBuffer,
						&playingsource->flags
					);

				//convert the audio source's number of channels to port audio's number of channels.
				EBUS_EVENT(
					AlternativeAudio::AlternativeAudioRequestBus,
					ConvertAudioFrame,
					srcframes,
					framesOut,
					sourceFrameType,
					this->m_audioFormat,
					frameLength
				);
				delete[] srcframes; //free up the memory.

				//apply after convert dsp
				if (m_isMaster)
					EBUS_EVENT(
						AlternativeAudio::AlternativeAudioDSPBus,
						ProcessEffects,
						AlternativeAudio::AADSPSection::eDS_PerSource_AC,
						this->m_audioFormat,
						(float*)framesOut,
						framesPerBuffer,
						&playingsource->flags
					);
				else
					this->ProcessEffects(
						AlternativeAudio::AADSPSection::eDS_PerSource_AC,
						this->m_audioFormat,
						(float*)framesOut,
						framesPerBuffer,
						&playingsource->flags
					);
			} else { //otherwise
				//read frame data.
				long long framesToRead = (((long long)((double)framesPerBuffer / ratio)));
				AlternativeAudio::AudioFrame::Frame * srcframes = PortAudioDevice::CreateBuffer(sourceFrameType, framesToRead);
				long long framesRead = playingsource->audioSource->GetFrames(framesToRead, (float *)srcframes); //get the data.

				//convert to port audio's channels.
				AlternativeAudio::AudioFrame::Frame * convertedSrcFrames = PortAudioDevice::CreateBuffer(this->m_audioFormat, framesToRead);

				//apply before convert dsp
				if (m_isMaster)
					EBUS_EVENT(
						AlternativeAudio::AlternativeAudioDSPBus,
						ProcessEffects,
						AlternativeAudio::AADSPSection::eDS_PerSource_BC,
						sourceFrameType,
						(float*)srcframes,
						framesPerBuffer,
						&playingsource->flags
					);
				else
					this->ProcessEffects(
						AlternativeAudio::AADSPSection::eDS_PerSource_BC,
						sourceFrameType,
						(float*)srcframes,
						framesPerBuffer,
						&playingsource->flags
					);

				//convert the audio source's number of channels to port audio's number of channels.
				EBUS_EVENT(
					AlternativeAudio::AlternativeAudioRequestBus,
					ConvertAudioFrame,
					srcframes,
					convertedSrcFrames,
					sourceFrameType,
					this->m_audioFormat,
					framesRead
				);
				delete[] srcframes;

				//apply after convert dsp
				if (m_isMaster)
					EBUS_EVENT(
						AlternativeAudio::AlternativeAudioDSPBus,
						ProcessEffects,
						AlternativeAudio::AADSPSection::eDS_PerSource_AC,
						this->m_audioFormat,
						(float*)convertedSrcFrames,
						framesPerBuffer,
						&playingsource->flags
					);
				else
					this->ProcessEffects(
						AlternativeAudio::AADSPSection::eDS_PerSource_AC,
						this->m_audioFormat,
						(float*)convertedSrcFrames,
						framesPerBuffer,
						&playingsource->flags
					);

				//convert samplerate.
				SRC_DATA src_data;
				src_data.data_in = (float *)convertedSrcFrames;
				src_data.data_out = (float *)framesOut;
				src_data.input_frames = (long)framesRead;
				src_data.output_frames = framesPerBuffer;
				src_data.src_ratio = ratio;

				//tell the sample rate converter if we are at the end or not.
				src_data.end_of_input = (playingsource->currentFrame + src_data.input_frames >= playingsource->endFrame);

				//reset the sample rate conversion state
				src_reset(this->m_pSrcState);

				//convert the sample rate
				int src_err = src_process(this->m_pSrcState, &src_data);

				if (src_err != 0) this->pushError(src_err, src_strerror(src_err)); //if we have an error, push it back.

				delete[] convertedSrcFrames; //free up the frame in buffer

				frameLength = src_data.output_frames_gen;
				playingsource->currentFrame += src_data.input_frames_used;
			}

			//apply after resampling dsp
			if (m_isMaster)
				EBUS_EVENT(
					AlternativeAudio::AlternativeAudioDSPBus,
					ProcessEffects,
					AlternativeAudio::AADSPSection::eDS_PerSource_ARS,
					this->m_audioFormat,
					(float*)framesOut,
					framesPerBuffer,
					&playingsource->flags
				);
			else
				this->ProcessEffects(
					AlternativeAudio::AADSPSection::eDS_PerSource_ARS,
					this->m_audioFormat,
					(float*)framesOut,
					framesPerBuffer,
					&playingsource->flags
				);

			playingsource->flags.SetFlags(prevFlags);

			//mix audio
			EBUS_EVENT(
				AlternativeAudio::AlternativeAudioRequestBus,
				MixAudioFrames,
				(float*)outputBuffer,
				(float*)framesOut,
				this->m_audioFormat,
				frameLength
			);

			if (playingsource->currentFrame >= playingsource->endFrame) { //if we are finished playing.
				if (playingsource->loop) { //if we are to loop
					playingsource->currentFrame = playingsource->startFrame; //set the current frame to the beginning.
				} else { //otherwise
					m_stoppedAudioFiles.push_back(entry.first); //mark for removal.
				}
			}
		}

		delete[] framesOut; //free up memory

		//Apply Output DSP
		if (m_isMaster)
			EBUS_EVENT(
				AlternativeAudio::AlternativeAudioDSPBus,
				ProcessEffects,
				AlternativeAudio::AADSPSection::eDS_Output,
				this->m_audioFormat,
				(float*)outputBuffer,
				framesPerBuffer,
				&this->m_flags
			);
		else
			this->ProcessEffects(
				AlternativeAudio::AADSPSection::eDS_Output,
				this->m_audioFormat,
				(float*)outputBuffer,
				framesPerBuffer,
				&this->m_flags
			);

		this->m_flags.SetFlags(dflags);

		//removed stopped audio files.
		while (m_stoppedAudioFiles.size() > 0) {
			this->m_playingAudioSource.erase(m_stoppedAudioFiles.back());
			m_stoppedAudioFiles.pop_back();
		}
		this->m_callbackMutex.unlock();

		if (allPaused) return paComplete; //if all the sources are paused, why are we running?
		if (this->m_playingAudioSource.size() == 0) {
			//this->m_nextPlayID = 0;
			return paComplete; //if we have no audio sources, why are we running?
		}
		return paContinue;
	}

	AlternativeAudio::AudioFrame::Frame * PortAudioDevice::CreateBuffer(AlternativeAudio::AudioFrame::Type type, long long length) {
		switch (type) {
		case AlternativeAudio::AudioFrame::Type::eT_af1:
			return new AlternativeAudio::AudioFrame::af1[length];
		case AlternativeAudio::AudioFrame::Type::eT_af2:
			return new AlternativeAudio::AudioFrame::af2[length];
		case AlternativeAudio::AudioFrame::Type::eT_af21:
			return new AlternativeAudio::AudioFrame::af21[length];
		case AlternativeAudio::AudioFrame::Type::eT_af3:
			return new AlternativeAudio::AudioFrame::af3[length];
		case AlternativeAudio::AudioFrame::Type::eT_af31:
			return new AlternativeAudio::AudioFrame::af31[length];
		case AlternativeAudio::AudioFrame::Type::eT_af4:
			return new AlternativeAudio::AudioFrame::af4[length];
		case AlternativeAudio::AudioFrame::Type::eT_af41:
			return new AlternativeAudio::AudioFrame::af41[length];
		case AlternativeAudio::AudioFrame::Type::eT_af5:
			return new AlternativeAudio::AudioFrame::af5[length];
		case AlternativeAudio::AudioFrame::Type::eT_af51:
			return new AlternativeAudio::AudioFrame::af51[length];
		case AlternativeAudio::AudioFrame::Type::eT_af7:
			return new AlternativeAudio::AudioFrame::af7[length];
		case AlternativeAudio::AudioFrame::Type::eT_af71:
			return new AlternativeAudio::AudioFrame::af71[length];
		}
		return nullptr;
	}

	int PortAudioDevice::getNumberOfChannels(AlternativeAudio::AudioFrame::Type type) {
		if (type == AlternativeAudio::AudioFrame::Type::eT_af1) return 1;
		else if (type == AlternativeAudio::AudioFrame::Type::eT_af2) return 2;
		else if (type == AlternativeAudio::AudioFrame::Type::eT_af21) return 3;
		else if (type == AlternativeAudio::AudioFrame::Type::eT_af3) return 3;
		else if (type == AlternativeAudio::AudioFrame::Type::eT_af31) return 4;
		else if (type == AlternativeAudio::AudioFrame::Type::eT_af4) return 4;
		else if (type == AlternativeAudio::AudioFrame::Type::eT_af41) return 5;
		else if (type == AlternativeAudio::AudioFrame::Type::eT_af5) return 5;
		else if (type == AlternativeAudio::AudioFrame::Type::eT_af51) return 6;
		else if (type == AlternativeAudio::AudioFrame::Type::eT_af7) return 7;
		else if (type == AlternativeAudio::AudioFrame::Type::eT_af71) return 8;
		return 1;
	}
}