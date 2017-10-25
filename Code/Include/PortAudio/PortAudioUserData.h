#pragma once

#include <AzCore/EBus/EBus.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/RTTI/BehaviorContext.h>

namespace PortAudio {
	struct PortAudioUserData {
		unsigned long streamFlags{ 0 };
		double suggestedLatency{ -1.0 };
		void * hostApiSpecificStreamInfo{ nullptr };

		AZ_RTTI(PortAudioUserData, "{A3FBA43E-DE4C-4327-B9E2-830BCD2AA918}");

		unsigned long getStreamFlags() { return this->streamFlags; }
		double getSuggestedLatency() { return this->suggestedLatency; }
		void * getHostAPISpecificStreamInfo() { return this->hostApiSpecificStreamInfo; }

		void setStreamFlags(unsigned long flags) { this->streamFlags = flags; }
		void setSuggestedLatency(double latency) { this->suggestedLatency = latency; }
		void setHostAPISpecificStreamInfo(void * info) { this->hostApiSpecificStreamInfo = info; }

		static void Reflect(AZ::SerializeContext* serialize) {
			serialize->Class<PortAudioUserData>()
				->Version(0)
				->SerializerForEmptyClass();
		}

		static void Behavior(AZ::BehaviorContext* behaviorContext) {
			behaviorContext->Class<PortAudioUserData>("PortAudioUserData")
				->Attribute(AZ::Script::Attributes::Category, "PortAudio")
				->Method("getStreamFlags", &PortAudioUserData::getStreamFlags)
				->Method("getSuggestedLatency", &PortAudioUserData::getSuggestedLatency)
				->Method("getHostAPISpecificStreamInfo", &PortAudioUserData::getHostAPISpecificStreamInfo)
				->Method("setStreamFlags", &PortAudioUserData::setStreamFlags)
				->Method("setSuggestedLatency", &PortAudioUserData::setSuggestedLatency)
				->Method("setHostAPISpecificStreamInfo", &PortAudioUserData::setHostAPISpecificStreamInfo)

				->Property("streamFlags", &PortAudioUserData::getStreamFlags, &PortAudioUserData::setStreamFlags)
				->Property("suggestedLatency", &PortAudioUserData::getSuggestedLatency, &PortAudioUserData::setSuggestedLatency)
				->Property("hostApiSpecificStreamInfo", &PortAudioUserData::getHostAPISpecificStreamInfo, &PortAudioUserData::setHostAPISpecificStreamInfo)
				;
		}
	};
} // namespace PortAudio
