#include "StdAfx.h"

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/RTTI/BehaviorContext.h>

#include "PortAudioSystemComponent.h"

#include <AlternativeAudio\AlternativeAudioBus.h>

namespace PortAudio {
	PortAudioSystemComponent::PortAudioSystemComponent() {
		m_nextPlayID = 0;
		m_masterVol = 0;
		m_vols = AZStd::vector<float>(eAS_Count, 1.0f);
		//for (int i = 0; i < eAS_Count; i++) this->m_vols[i] = 1.0f;
		m_sampleRate = 0.0;
		m_pSrcState = nullptr;
		m_hasError = false;
		m_pAudioStream = nullptr;
		m_audioFormat = AlternativeAudio::AudioFrame::af1::RTTI_Type();
		m_device = Pa_GetDefaultOutputDevice();
		m_hostApiSpecificStreamInfo = 0;
	}

	void PortAudioSystemComponent::Reflect(AZ::ReflectContext* context) {
		if (AZ::SerializeContext* serialize = azrtti_cast<AZ::SerializeContext*>(context)) {
			auto classinfo = serialize->Class<PortAudioSystemComponent, AZ::Component>()
				->Version(1)
				->Field("masterVol", &PortAudioSystemComponent::m_masterVol)
				->Field("volumes", &PortAudioSystemComponent::m_vols)
				->Field("samplerate", &PortAudioSystemComponent::m_sampleRate)
				->Field("audioFormat", &PortAudioSystemComponent::m_audioFormat)
				->Field("device", &PortAudioSystemComponent::m_device)
				->Field("hostSpecificStreamInfo", &PortAudioSystemComponent::m_hostApiSpecificStreamInfo);

			if (AZ::EditContext* ec = serialize->GetEditContext()) {
				ec->Class<PortAudioSystemComponent>("PortAudio", "Port Audio playback system utilizing Alternative Audio Gem.")
					->ClassElement(AZ::Edit::ClassElements::EditorData, "")
					->Attribute(AZ::Edit::Attributes::Category, "Alternative Audio - Playback")
					->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("System"))
					->Attribute(AZ::Edit::Attributes::AutoExpand, true)
					;
			}
		}

		AZ::BehaviorContext* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
		if (behaviorContext) {
			behaviorContext->Class<PortAudioSystemComponent>("PortAudio")
				->Enum<EAudioSection::eAS_Music>("eAS_Music")
				->Enum<EAudioSection::eAS_SFX>("eAS_SFX")
				->Enum<EAudioSection::eAS_Voice>("eAS_Voice")
				->Enum<EAudioSection::eAS_Environment>("eAS_Environment")
				->Enum<EAudioSection::eAS_UI>("eAS_UI")
				;

			#define EBUS_METHOD(name) ->Event(#name, &PortAudioRequestBus::Events::##name##)
			behaviorContext->EBus<PortAudioRequestBus>("PortAudioBus")
				EBUS_METHOD(SetStream)
				EBUS_METHOD(PlaySource)
				EBUS_METHOD(PauseSource)
				EBUS_METHOD(ResumeSource)
				EBUS_METHOD(StopSource)
				EBUS_METHOD(IsPlaying)
				EBUS_METHOD(GetTime)
				EBUS_METHOD(SetTime)
				EBUS_METHOD(SetMasterVol)
				EBUS_METHOD(GetMasterVol)
				EBUS_METHOD(SetVolume)
				EBUS_METHOD(GetVolume)
				EBUS_METHOD(SetSourceVolume)
				EBUS_METHOD(GetSourceVolume)
				;
			#undef EBUS_METHOD
		}
	}

	void PortAudioSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided) {
		provided.push_back(AZ_CRC("PortAudioService"));
	}

	void PortAudioSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible) {
		incompatible.push_back(AZ_CRC("PortAudioService"));
	}

	void PortAudioSystemComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required) {
		(void)required;
	}

	void PortAudioSystemComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent) {
		(void)dependent;
	}

	void PortAudioSystemComponent::Init() {
	}

	void PortAudioSystemComponent::Activate() {
		this->m_initialized = true;
		int err = Pa_Initialize();

		if (err != paNoError) {
			AZStd::string errString("PA Error: ");
			errString += Pa_GetErrorText(err);
			AZ_Printf("[PortAudio]","[PortAudio] Music Init Error: %s", errString.c_str());
			this->m_initialized = false;
		}

		AZ_Printf("[PortAudio]", "[PortAudio] Port Audio Version: %s", Pa_GetVersionText());
		AZ_Printf("[PortAudio]", "[PortAudio] libsamplerate Version: %s", src_get_version());

		SetStream(this->m_sampleRate, this->m_audioFormat, this->m_device, this->m_hostApiSpecificStreamInfo);

		PortAudioRequestBus::Handler::BusConnect();
	}

	void PortAudioSystemComponent::Deactivate() {
		if (this->m_pAudioStream) {
			Pa_StopStream(this->m_pAudioStream);
			Pa_CloseStream(this->m_pAudioStream);
			this->m_pAudioStream = nullptr;
		}

		if (this->m_pSrcState) {
			src_delete(this->m_pSrcState);
			this->m_pSrcState = nullptr;
		}

		this->m_initialized = false;
		PortAudioRequestBus::Handler::BusDisconnect();
		int err = Pa_Terminate();

		if (err != paNoError) {
			AZStd::string errString("PA Error: ");
			errString += Pa_GetErrorText(err);
			AZ_Printf("[PortAudio]", "[PortAudio] Music Terminate Error: %s", errString.c_str());
		}
	}

	int getNumberOfChannels(AZ::Uuid type) {
		if (type == AlternativeAudio::AudioFrame::af1::RTTI_Type()) return 1;
		else if (type == AlternativeAudio::AudioFrame::af2::RTTI_Type()) return 2;
		else if (type == AlternativeAudio::AudioFrame::af21::RTTI_Type()) return 3;
		else if (type == AlternativeAudio::AudioFrame::af3::RTTI_Type()) return 3;
		else if (type == AlternativeAudio::AudioFrame::af31::RTTI_Type()) return 4;
		else if (type == AlternativeAudio::AudioFrame::af5::RTTI_Type()) return 5;
		else if (type == AlternativeAudio::AudioFrame::af51::RTTI_Type()) return 6;
		else if (type == AlternativeAudio::AudioFrame::af7::RTTI_Type()) return 7;
		else if (type == AlternativeAudio::AudioFrame::af71::RTTI_Type()) return 8;
		return 1;
	}

	void PortAudioSystemComponent::SetStream(double samplerate, AZ::Uuid& audioFormat, int device, void * hostApiSpecificStreamInfo) {
		int streamActive = 0;
		if (this->m_pAudioStream) {
			streamActive = Pa_IsStreamActive(this->m_pAudioStream);

			//stop stream if it isnt stopped
			if (Pa_IsStreamStopped(this->m_pAudioStream) != 1) Pa_StopStream(this->m_pAudioStream);

			//close stream
			Pa_CloseStream(this->m_pAudioStream);
			this->m_pAudioStream = nullptr;
		}

		if (this->m_pSrcState) {
			src_delete(this->m_pSrcState);
			this->m_pSrcState = nullptr;
		}

		this->m_sampleRate = samplerate;
		this->m_hostApiSpecificStreamInfo = hostApiSpecificStreamInfo;

		PaStreamParameters streamParams;
		streamParams.device = device; //set device to use
		streamParams.hostApiSpecificStreamInfo = hostApiSpecificStreamInfo;
		streamParams.sampleFormat = paFloat32; // 32bit float format
		streamParams.suggestedLatency = PAS_Millsecond(200); //200 ms ought to satisfy even the worst sound card - effects delta time (higher ms - higher delta)

		m_audioFormat = audioFormat;

		streamParams.channelCount = getNumberOfChannels(audioFormat);

		int err = Pa_OpenStream(
			&this->m_pAudioStream,
			0, // no input
			&streamParams,
			this->m_sampleRate,
			paFramesPerBufferUnspecified, // let portaudio choose the buffersize
			paNoFlag,/* no special modes (clip off, dither off) */
			PortAudioSystemComponent::paCallbackCommon, //set the callback to be the function that calls the per class callback
			this
		);

		if (err != paNoError) {
			pushError(err, Pa_GetErrorText(err));
			this->m_pAudioStream = nullptr;
		}

		int src_err;
		this->m_pSrcState = src_new(SRC_SINC_MEDIUM_QUALITY, streamParams.channelCount, &src_err); //create a new sample rate converter

		if (this->m_pSrcState == NULL) this->pushError(src_err, src_strerror(src_err));

		//restart stream if it was active
		if (streamActive == 1) Pa_StartStream(this->m_pAudioStream);
	}

	//audio source control
	long long PortAudioSystemComponent::PlaySource(AlternativeAudio::IAudioSource * source, EAudioSection section) {
		//push new audio source to playing sources
		if (source == nullptr || source == NULL) return -1;

		long long uID = this->m_nextPlayID++;

		int flags = source->GetFlags();

		PlayingAudioSource *playingSource = new PlayingAudioSource();
		playingSource->audioSource = source;
		playingSource->section = section;
		playingSource->startFrame = 0;
		playingSource->currentFrame = 0;
		playingSource->endFrame = source->GetFrameLength();
		playingSource->loop = ((flags & AlternativeAudio::eAF_Loop) == 1);
		playingSource->paused = ((flags & AlternativeAudio::eAF_PausedOnStart) == 1);

		this->m_callbackMutex.lock();
		this->m_playingAudioSource.insert(AZStd::make_pair<>(uID, playingSource));
		this->m_callbackMutex.unlock();

		//check if stream is stopped, if it is, start it up.
		if (Pa_IsStreamActive(this->m_pAudioStream) != 1) {
			Pa_StopStream(this->m_pAudioStream); //resets stream time.
			Pa_StartStream(this->m_pAudioStream);
		}

		return uID;
	}
	void PortAudioSystemComponent::PauseSource(long long id) {
		this->m_callbackMutex.lock();
		if (this->m_playingAudioSource.find(id) == this->m_playingAudioSource.end()) {
			m_callbackMutex.unlock();
			return;
		}
		this->m_playingAudioSource[id]->paused = true;
		this->m_callbackMutex.unlock();
	}
	void PortAudioSystemComponent::ResumeSource(long long id) {
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
	void PortAudioSystemComponent::StopSource(long long id) {
		this->m_callbackMutex.lock();
		if (this->m_playingAudioSource.find(id) != this->m_playingAudioSource.end()) this->m_playingAudioSource.erase(id);
		this->m_callbackMutex.unlock();
	}
	bool PortAudioSystemComponent::IsPlaying(long long id) {
		this->m_callbackMutex.lock();
		if (this->m_playingAudioSource.find(id) != this->m_playingAudioSource.end()) {
			bool paused = this->m_playingAudioSource[id]->paused;
			this->m_callbackMutex.unlock();
			return !paused;
		}
		this->m_callbackMutex.unlock();

		return false;
	}
	AlternativeAudio::AudioSourceTime PortAudioSystemComponent::GetTime(long long id) {
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
	void PortAudioSystemComponent::SetTime(long long id, double time) {
		long long setFrame = (long long)(time * this->m_playingAudioSource[id]->audioSource->GetSampleRate());
		this->m_callbackMutex.lock();
		if (this->m_playingAudioSource.find(id) != this->m_playingAudioSource.end()) {
			if (setFrame < this->m_playingAudioSource[id]->startFrame) setFrame = this->m_playingAudioSource[id]->startFrame;
			if (setFrame > this->m_playingAudioSource[id]->endFrame) setFrame = this->m_playingAudioSource[id]->endFrame;
			this->m_playingAudioSource[id]->currentFrame = setFrame;
		}
		this->m_callbackMutex.unlock();
	}

	//volume control
	void PortAudioSystemComponent::SetMasterVol(float vol) {
		this->m_callbackMutex.lock();
		if (vol >= 1.0f) vol = 1.0f;
		if (vol <= 0.0f) vol = 0.0f;
		this->m_masterVol = vol;
		this->m_callbackMutex.unlock();
	}
	float PortAudioSystemComponent::GetMasterVol() { return this->m_masterVol; }
	void PortAudioSystemComponent::SetVolume(float vol, EAudioSection section) {
		this->m_callbackMutex.lock();
		if (section < 0 || section > eAS_Count) return;
		if (vol >= 1.0f) vol = 1.0f;
		if (vol <= 0.0f) vol = 0.0f;
		this->m_vols[section];
		this->m_callbackMutex.unlock();
	}
	float PortAudioSystemComponent::GetVolume(EAudioSection section) {
		if (section < 0 || section > eAS_Count) return 0.0f;
		return this->m_vols[section];
	}
	void PortAudioSystemComponent::SetSourceVolume(float vol, long long id) {
		if (vol >= 1.0f) vol = 1.0f;
		if (vol <= 0.0f) vol = 0.0f;

		this->m_callbackMutex.lock();
		if (this->m_playingAudioSource.find(id) != this->m_playingAudioSource.end()) {
			this->m_playingAudioSource[id]->vol = vol;
		}
		this->m_callbackMutex.unlock();
	}
	float PortAudioSystemComponent::GetSourceVolume(long long id) {
		float ret = 1.0f;

		this->m_callbackMutex.lock();
		if (this->m_playingAudioSource.find(id) != this->m_playingAudioSource.end())
			ret = this->m_playingAudioSource[id]->vol;
		this->m_callbackMutex.unlock();

		return ret;
	}

	//error checking
	bool PortAudioSystemComponent::HasError() {
		this->m_errorMutex.lock();
		bool ret = this->m_hasError;
		this->m_errorMutex.unlock();
		return ret;
	}
	PASError PortAudioSystemComponent::GetError() {
		if (this->m_errors.size() == 0) {
			PASError noErr;
			this->m_hasError = false;
			return noErr;
		}

		this->m_errorMutex.lock();
		PASError ret = this->m_errors.back();
		this->m_errors.pop_back();

		if (this->m_errors.size() == 0) this->m_hasError = false;
		this->m_errorMutex.unlock();
		return ret;
	}

	void PortAudioSystemComponent::pushError(int errorCode, const char * errorStr) {
		PASError err;
		err.code = errorCode;
		err.str = errorStr;
		this->m_errorMutex.lock();
		this->m_errors.push_back(err);
		this->m_hasError = true;
		this->m_errorMutex.unlock();
	}

	int PortAudioSystemComponent::paCallbackCommon(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData) {
		//redirect to the individual callback. (gives us individualized and possibly multiple streams at the same time).
		return ((PortAudioSystemComponent *)userData)->paCallback(inputBuffer, outputBuffer, framesPerBuffer, timeInfo, statusFlags, userData);
	}
	
	AlternativeAudio::AudioFrame::Frame * ConvertBuffer(AZ::Uuid type, void * in) {
		if (type == AlternativeAudio::AudioFrame::af1::RTTI_Type())
			return azdynamic_cast<AlternativeAudio::AudioFrame::af1*>(in);
		else if (type == AlternativeAudio::AudioFrame::af2::RTTI_Type())
			return azdynamic_cast<AlternativeAudio::AudioFrame::af2*>(in);
		else if (type == AlternativeAudio::AudioFrame::af21::RTTI_Type())
			return azdynamic_cast<AlternativeAudio::AudioFrame::af21*>(in);
		else if (type == AlternativeAudio::AudioFrame::af3::RTTI_Type())
			return azdynamic_cast<AlternativeAudio::AudioFrame::af3*>(in);
		else if (type == AlternativeAudio::AudioFrame::af31::RTTI_Type())
			return azdynamic_cast<AlternativeAudio::AudioFrame::af31*>(in);
		else if (type == AlternativeAudio::AudioFrame::af5::RTTI_Type())
			return azdynamic_cast<AlternativeAudio::AudioFrame::af5*>(in);
		else if (type == AlternativeAudio::AudioFrame::af51::RTTI_Type())
			return azdynamic_cast<AlternativeAudio::AudioFrame::af51*>(in);
		else if (type == AlternativeAudio::AudioFrame::af7::RTTI_Type())
			return azdynamic_cast<AlternativeAudio::AudioFrame::af7*>(in);
		else if (type == AlternativeAudio::AudioFrame::af71::RTTI_Type())
			return azdynamic_cast<AlternativeAudio::AudioFrame::af71*>(in);
		return nullptr;
	}

	AlternativeAudio::AudioFrame::Frame * CreateBuffer(AZ::Uuid type, long long length) {
		if (type == AlternativeAudio::AudioFrame::af1::RTTI_Type())
			return new AlternativeAudio::AudioFrame::af1[length];
		else if (type == AlternativeAudio::AudioFrame::af2::RTTI_Type())
			return new AlternativeAudio::AudioFrame::af2[length];
		else if (type == AlternativeAudio::AudioFrame::af21::RTTI_Type())
			return new AlternativeAudio::AudioFrame::af21[length];
		else if (type == AlternativeAudio::AudioFrame::af3::RTTI_Type())
			return new AlternativeAudio::AudioFrame::af3[length];
		else if (type == AlternativeAudio::AudioFrame::af31::RTTI_Type())
			return new AlternativeAudio::AudioFrame::af31[length];
		else if (type == AlternativeAudio::AudioFrame::af5::RTTI_Type())
			return new AlternativeAudio::AudioFrame::af5[length];
		else if (type == AlternativeAudio::AudioFrame::af51::RTTI_Type())
			return new AlternativeAudio::AudioFrame::af51[length];
		else if (type == AlternativeAudio::AudioFrame::af7::RTTI_Type())
			return new AlternativeAudio::AudioFrame::af7[length];
		else if (type == AlternativeAudio::AudioFrame::af71::RTTI_Type())
			return new AlternativeAudio::AudioFrame::af71[length];
		return nullptr;
	}

	int PortAudioSystemComponent::paCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData) {
		if (this->m_playingAudioSource.size() == 0) return paComplete; //if we have no audio sources, why are we running?

		int pachannels = getNumberOfChannels(this->m_audioFormat);

		{ //clear the buffer
			float * outputFBuff = (float *)outputBuffer;
			for (int i = 0; i < framesPerBuffer * pachannels; i++) outputFBuff[i] = 0.0f;
		}

		//convert output buffer;
		AlternativeAudio::AudioFrame::Frame * outputFrameBuffer = ConvertBuffer(this->m_audioFormat, outputBuffer);

		//create a buffer to hold the converted channel audio source
		AlternativeAudio::AudioFrame::Frame * framesOut = CreateBuffer(this->m_audioFormat, framesPerBuffer);
		
		//ERROR OUT
		if (outputFrameBuffer == nullptr) {
			this->pushError(-1, "Could not convert output buffer.");
			return paAbort;
		}

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
			AZ::Uuid sourceFrameType = playingsource->audioSource->GetFrameType();

			//get the ratio of the sample rate conversion
			double ratio = this->m_sampleRate / playingsource->audioSource->GetSampleRate();

			long long frameLength = 0;

			//convert to port audio's audio frame format and resample.
			if (ratio == 1.0f || !src_is_valid_ratio(ratio)) { //if the sample rate is the same between the audio source and port audio or if it's not a valid ratio.
				AlternativeAudio::AudioFrame::Frame * srcframes = CreateBuffer(sourceFrameType, framesPerBuffer); //create a buffer to hold the audio sources data
				frameLength = playingsource->audioSource->GetFrames(framesPerBuffer, (float *)srcframes); //get the data.
				playingsource->currentFrame += frameLength;

				//convert the audio source's number of channels to port audio's number of channels.
				for (int i = 0; i < frameLength; i++)
					EBUS_EVENT(
						AlternativeAudio::AlternativeAudioRequestBus,
						ConvertAudioFrame,
						&srcframes[i],
						&framesOut[i],
						sourceFrameType,
						this->m_audioFormat
					);
				delete[] srcframes; //free up the memory.
			} else { //otherwise
				//read frame data.
				long long framesToRead = ((long long)((double)framesPerBuffer / ratio));
				AlternativeAudio::AudioFrame::Frame * srcframes = CreateBuffer(sourceFrameType, framesToRead);
				long long framesRead = playingsource->audioSource->GetFrames(framesToRead, (float *)srcframes); //get the data.

				//convert to port audio's channels.
				AlternativeAudio::AudioFrame::Frame * convertedSrcFrames = CreateBuffer(this->m_audioFormat, framesToRead);

				for (int i = 0; i < framesRead; i++)
					EBUS_EVENT(
						AlternativeAudio::AlternativeAudioRequestBus,
						ConvertAudioFrame,
						&srcframes[i],
						&convertedSrcFrames[i],
						sourceFrameType,
						this->m_audioFormat
					);
				delete[] srcframes;

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

			//adjust volumes
#			define SET_BUFFERS(type) \
				AlternativeAudio::AudioFrame::##type##* out = (AlternativeAudio::AudioFrame::##type##*)outputFrameBuffer; \
				AlternativeAudio::AudioFrame::##type##* src = (AlternativeAudio::AudioFrame::##type##*)framesOut;

#			define SET_CHANNEL(channel) \
				if (out[i].##channel == 0.0f) out[i].##channel = src[i].##channel * (this->m_masterVol * this->m_vols[playingsource->section] * playingsource->vol); \
				else out[i].##channel += src[i].##channel * (this->m_masterVol * this->m_vols[playingsource->section] * playingsource->vol); \
				out[i].##channel = AZ::GetClamp(out[i].##channel, -1.0f, 1.0f);

			if (this->m_audioFormat == AlternativeAudio::AudioFrame::af1::RTTI_Type()) {
				SET_BUFFERS(af1);
				for (int i = 0; i < frameLength; i++) {
					SET_CHANNEL(mono);
				}
			} else if (this->m_audioFormat == AlternativeAudio::AudioFrame::af2::RTTI_Type()) {
				SET_BUFFERS(af2);
				for (int i = 0; i < frameLength; i++) {
					SET_CHANNEL(left);
					SET_CHANNEL(right);
				}
			} else if (this->m_audioFormat == AlternativeAudio::AudioFrame::af21::RTTI_Type()) {
				SET_BUFFERS(af21);
				for (int i = 0; i < frameLength; i++) {
					SET_CHANNEL(left);
					SET_CHANNEL(right);
					SET_CHANNEL(sub);
				}
			} else if (this->m_audioFormat == AlternativeAudio::AudioFrame::af3::RTTI_Type()) {
				SET_BUFFERS(af3);
				for (int i = 0; i < frameLength; i++) {
					SET_CHANNEL(left);
					SET_CHANNEL(right);
					SET_CHANNEL(center);
				}
			} else if (this->m_audioFormat == AlternativeAudio::AudioFrame::af31::RTTI_Type()) {
				SET_BUFFERS(af31);
				for (int i = 0; i < frameLength; i++) {
					SET_CHANNEL(left);
					SET_CHANNEL(right);
					SET_CHANNEL(center);
					SET_CHANNEL(sub);
				}
			} else if (this->m_audioFormat == AlternativeAudio::AudioFrame::af5::RTTI_Type()) {
				SET_BUFFERS(af5);
				for (int i = 0; i < frameLength; i++) {
					SET_CHANNEL(left);
					SET_CHANNEL(right);
					SET_CHANNEL(center);
					SET_CHANNEL(bleft);
					SET_CHANNEL(bright);
				}
			} else if (this->m_audioFormat == AlternativeAudio::AudioFrame::af51::RTTI_Type()) {
				SET_BUFFERS(af51);
				for (int i = 0; i < frameLength; i++) {
					SET_CHANNEL(left);
					SET_CHANNEL(right);
					SET_CHANNEL(center);
					SET_CHANNEL(bleft);
					SET_CHANNEL(bright);
					SET_CHANNEL(sub);
				}
			} else if (this->m_audioFormat == AlternativeAudio::AudioFrame::af7::RTTI_Type()) {
				SET_BUFFERS(af7);
				for (int i = 0; i < frameLength; i++) {
					SET_CHANNEL(left);
					SET_CHANNEL(right);
					SET_CHANNEL(center);
					SET_CHANNEL(sleft);
					SET_CHANNEL(sright);
					SET_CHANNEL(bleft);
					SET_CHANNEL(bright);
				}
			} else if (this->m_audioFormat == AlternativeAudio::AudioFrame::af71::RTTI_Type()) {
				SET_BUFFERS(af71);
				for (int i = 0; i < frameLength; i++) {
					SET_CHANNEL(left);
					SET_CHANNEL(right);
					SET_CHANNEL(center);
					SET_CHANNEL(sleft);
					SET_CHANNEL(sright);
					SET_CHANNEL(bleft);
					SET_CHANNEL(bright);
					SET_CHANNEL(sub);
				}
			}
#			undef SET_BUFFERS
#			undef SET_CHANNEL

			if (playingsource->currentFrame >= playingsource->endFrame) { //if we are finished playing.
				if (playingsource->loop) { //if we are to loop
					playingsource->currentFrame = playingsource->startFrame; //set the current frame to the beginning.
				} else { //otherwise
					m_stoppedAudioFiles.push_back(entry.first); //mark for removal.
				}
			}
		}

		delete[] framesOut; //free up memory

		//removed stopped audio files.
		while (m_stoppedAudioFiles.size() > 0) {
			this->m_playingAudioSource.erase(m_stoppedAudioFiles.back());
			m_stoppedAudioFiles.pop_back();
		}
		this->m_callbackMutex.unlock();

		if (allPaused) return paComplete; //if all the sources are paused, why are we running?
		if (this->m_playingAudioSource.size() == 0) return paComplete; //if we have no audio sources, why are we running?
		return paContinue;
	}
}
