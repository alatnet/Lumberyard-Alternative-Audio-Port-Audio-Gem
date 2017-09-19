#pragma once

#include <AzCore/Component/Component.h>

#include <PortAudio/PortAudioBus.h>
#include "PortAudioInternalBus.h"

#include "PortAudioCommon.h"

namespace PortAudio
{
    class PortAudioSystemComponent
        : public AZ::Component
        , protected PortAudioRequestBus::Handler
		, protected PortAudioInternalNotificationsBus::Handler
		, public AlternativeAudio::OAudioDeviceProvider
    {
	public:
		PortAudioSystemComponent();
		~PortAudioSystemComponent();
    public:
        AZ_COMPONENT(PortAudioSystemComponent, "{1E19EC45-499C-47EE-8FC7-915F6916FF48}", AlternativeAudio::OAudioDeviceProvider);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

    protected:
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
		bool m_registered;

		AZStd::vector<AlternativeAudio::OAudioDeviceInfo> devices;
	public:
		AlternativeAudio::OAudioDevice * NewDevice(long long device, double samplerate, AlternativeAudio::AudioFrame::Type audioFormat, void* userdata);
		virtual AZStd::vector<AlternativeAudio::OAudioDeviceInfo>& GetDevices() { return this->devices; }
		long long GetDefaultDevice() { return Pa_GetDefaultOutputDevice(); }
	private:
		AZStd::unordered_map<long long, AlternativeAudio::OAudioDevice*> deviceMap;
		////////////////////////////////////////////////////////////////////////
		// PortAudioInternalNotificationsBus interface implementation
	protected:
		void CloseDevice(long long deviceID);
		////////////////////////////////////////////////////////////////////////
    };
}
