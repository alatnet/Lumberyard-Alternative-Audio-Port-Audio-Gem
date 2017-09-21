#pragma once

namespace PortAudio {
	struct PortAudioUserData {
		unsigned long streamFlags{ 0 };
		double suggestedLatency{ -1.0 };
		void * hostApiSpecificStreamInfo{ nullptr };
	};
} // namespace PortAudio
