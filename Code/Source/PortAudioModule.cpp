
#include "StdAfx.h"
#include <platform_impl.h>

#include "PortAudioSystemComponent.h"

#include <IGem.h>

namespace PortAudio
{
    class PortAudioModule
        : public CryHooksModule
    {
    public:
        AZ_RTTI(PortAudioModule, "{4504EA5F-9C29-4310-A1F1-E357FE9E0F7C}", CryHooksModule);

        PortAudioModule()
            : CryHooksModule()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            m_descriptors.insert(m_descriptors.end(), {
                PortAudioSystemComponent::CreateDescriptor(),
            });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList{
                azrtti_typeid<PortAudioSystemComponent>(),
            };
        }
    };
}

// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
AZ_DECLARE_MODULE_CLASS(PortAudio_96a96b530fa24e0ba3a51daa485e9b16, PortAudio::PortAudioModule)
