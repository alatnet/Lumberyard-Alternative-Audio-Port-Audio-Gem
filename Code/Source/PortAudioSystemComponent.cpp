#include "StdAfx.h"

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/RTTI/BehaviorContext.h>

#include "PortAudioSystemComponent.h"

#include "PortAudioDevice.h"

#include <AlternativeAudio\AlternativeAudioBus.h>

#include <PortAudio\PortAudioUserData.h>

namespace PortAudio {
	int PortAudioSystemComponent::m_initializeCount = 0;
	bool PortAudioSystemComponent::m_initialized = false;

	PortAudioSystemComponent::PortAudioSystemComponent() {
		m_registered = m_devicesEnumerated = false;

		if (PortAudioSystemComponent::m_initializeCount == 0) {
			PortAudioSystemComponent::m_initializeCount++;
			PortAudioSystemComponent::m_initialized = true;

			int err = Pa_Initialize();
			if (err != paNoError) {
				const char * errStr = Pa_GetErrorText(err);
				AZ_Printf("[PortAudio]", "[PortAudio] Init Error: %s\n", errStr);
				this->pushError(err, errStr);
			}
		}
	}

	PortAudioSystemComponent::~PortAudioSystemComponent() {
		PortAudioSystemComponent::m_initializeCount--;

		if (!this->deviceMap.empty()) {
			for (AZStd::pair<long long, AlternativeAudio::OAudioDevice*> entry : this->deviceMap) //for each device
				while (entry.second->NumRefs() != 0) //while there are still references to an open device.
					entry.second->Release(); //release them.
				//delete entry.second;
			this->deviceMap.clear(); //clear the device map.
		}

		if (PortAudioSystemComponent::m_initializeCount < 0) PortAudioSystemComponent::m_initializeCount = 0;
		if (PortAudioSystemComponent::m_initializeCount == 0) {
			PortAudioSystemComponent::m_initialized = false;
			int err = Pa_Terminate();

			if (err != paNoError) {
				AZStd::string errString("PA Error: ");
				errString += Pa_GetErrorText(err);
				AZ_Printf("[PortAudio]", "[PortAudio] Terminate Error: %s\n", errString.c_str());
			}
		}
	}

	void PortAudioSystemComponent::Reflect(AZ::ReflectContext* context) {
		if (AZ::SerializeContext* serialize = azrtti_cast<AZ::SerializeContext*>(context)) {
			auto classinfo = serialize->Class<PortAudioSystemComponent, AZ::Component>()
				->Version(0)
				->SerializerForEmptyClass();

			PortAudioUserData::Reflect(serialize);

			if (AZ::EditContext* ec = serialize->GetEditContext()) {
				ec->Class<PortAudioSystemComponent>("PortAudio", "Port Audio playback system utilizing Alternative Audio Gem.")
					->ClassElement(AZ::Edit::ClassElements::EditorData, "")
					->Attribute(AZ::Edit::Attributes::Category, "Alternative Audio - Playback")
					->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("System", 0xc94d118b))
					->Attribute(AZ::Edit::Attributes::AutoExpand, true)
					;
			}
		}

		AZ::BehaviorContext* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
		if (behaviorContext) {
			PortAudioUserData::Behavior(behaviorContext);

			behaviorContext->Class<PortAudioSystemComponent>("PortAudio")
				->Attribute(AZ::Script::Attributes::Category, "PortAudio")
				->Constant("NoFlag", []() -> PaStreamFlags { return paNoFlag; })
				->Constant("ClipOff", []() -> PaStreamFlags { return paClipOff; })
				->Constant("DitherOff", []() -> PaStreamFlags { return paDitherOff; })
				;
		}
	}

	void PortAudioSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided) {
		provided.push_back(AZ_CRC("PortAudioService", 0x3553a550));
	}

	void PortAudioSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible) {
		incompatible.push_back(AZ_CRC("PortAudioService", 0x3553a550));
	}

	void PortAudioSystemComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required) {
		required.push_back(AZ_CRC("AlternativeAudioService", 0x2eb4e627));
	}

	void PortAudioSystemComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent) {
		(void)dependent;
	}

	void PortAudioSystemComponent::Init() {
		if (m_devicesEnumerated) return;
		if (this->m_hasError) return;

		AZ_Printf("[PortAudio]", "[PortAudio] Enumerating Devices.\n");
		m_devicesEnumerated = true;

		int numDevices = Pa_GetDeviceCount();

		for (int i = 0; i < numDevices; i++) {
			const PaDeviceInfo * info = Pa_GetDeviceInfo(i);

			AlternativeAudio::OAudioDeviceInfo device;

			device.name = AZStd::string(info->name);
			device.maxChannels = info->maxOutputChannels;
			device.defaultSampleRate = info->defaultSampleRate;

			this->devices.push_back(device);
		}
		AZ_Printf("[PortAudio]", "[PortAudio] Device Enumeration Complete.\n");
	}

	void PortAudioSystemComponent::Activate() {
		if (this->m_hasError) return;
		PortAudioInternalNotificationsBus::Handler::BusConnect();

		if (this->m_registered) return;

		AZ_Printf("[PortAudio]", "[PortAudio] Registering Port Audio.\n");
		EBUS_EVENT(
			AlternativeAudio::AlternativeAudioDeviceBus,
			RegisterPlaybackLibrary,
			"PortAudio",
			AZ_CRC("PortAudio", 0x2a7a2a31),
			this
		);
		
		AZ_Printf("[PortAudio]", "[PortAudio] Port Audio Version: %s\n", Pa_GetVersionText()/*Pa_GetVersionInfo()->versionText*/);
		AZ_Printf("[PortAudio]", "[PortAudio] libsamplerate Version: %s\n", src_get_version());

		AZ_Printf("[PortAudio]", "[PortAudio] Registration Complete.\n");
	}

	void PortAudioSystemComponent::Deactivate() {
		if (this->m_hasError) return;
		PortAudioInternalNotificationsBus::Handler::BusDisconnect();
	}

	AlternativeAudio::OAudioDevice * PortAudioSystemComponent::NewDevice(long long deviceIndex, double samplerate, AlternativeAudio::AudioFrame::Type audioFormat, void* userdata) {
		if (this->m_hasError) return nullptr;
		if (this->deviceMap.count(deviceIndex) == 0) { //if the device isnt open
			//create the device
			PortAudioDevice * d = new PortAudioDevice(deviceIndex, samplerate, audioFormat, userdata);
			this->deviceMap.insert(AZStd::make_pair(deviceIndex, d));
		}

		return this->deviceMap[deviceIndex]; //return the device
	}

	void PortAudioSystemComponent::CloseDevice(long long deviceID) {
		this->deviceMap.erase(deviceID);
	}
}
