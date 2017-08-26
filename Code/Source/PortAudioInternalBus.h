#pragma once

#include <AzCore\EBus\EBus.h>

namespace PortAudio {
	class PortAudioInternalNotifications : public AZ::EBusTraits {
	public:
		static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
		static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
	public:
		virtual void CloseDevice(long long deviceID) = 0;
	};

	using PortAudioInternalNotificationsBus = AZ::EBus<PortAudioInternalNotifications>;
}