#pragma once

#include <AzCore/EBus/EBus.h>
#include <AlternativeAudio\Device\OAudioDevice.h>

namespace PortAudio {
	class PortAudioRequests
		: public AZ::EBusTraits {
	public:
		static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
		static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
	/*public:
		virtual bool HasError() = 0;
		virtual AlternativeAudio::AAError GetError() = 0;*/
	};
	using PortAudioRequestBus = AZ::EBus<PortAudioRequests>;
} // namespace PortAudio
