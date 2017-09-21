#pragma once

#include <AzCore/EBus/EBus.h>
#include <AlternativeAudio\Device\OAudioDevice.h>

namespace PortAudio {
	class PortAudioRequests
		: public AZ::EBusTraits {
	public:
		static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
		static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
	};
	using PortAudioRequestBus = AZ::EBus<PortAudioRequests>;
} // namespace PortAudio
