//
// Copyright 2020 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/base/tf/diagnostic.h"

#include "pxr/imaging/hgiVulkan/capabilities.h"
#include "pxr/imaging/hgiVulkan/conversions.h"
#include "pxr/imaging/hgiVulkan/device.h"
#include "pxr/imaging/hgiVulkan/sampler.h"
#include "pxr/imaging/hgiVulkan/diagnostic.h"

#include <float.h>

PXR_NAMESPACE_OPEN_SCOPE


HgiVulkanSampler::HgiVulkanSampler(
    HgiVulkanDevice* device,
    HgiSamplerDesc const& desc)
    : HgiSampler(desc)
    , _vkSampler(nullptr)
    , _device(device)
    , _inflightBits(0)
{
    VkSamplerCreateInfo sampler = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler.magFilter = HgiVulkanConversions::GetMinMagFilter(desc.magFilter);
    sampler.minFilter = HgiVulkanConversions::GetMinMagFilter(desc.minFilter);
    sampler.addressModeU =
        HgiVulkanConversions::GetSamplerAddressMode(desc.addressModeU);
    sampler.addressModeV =
        HgiVulkanConversions::GetSamplerAddressMode(desc.addressModeV);
    sampler.addressModeW =
        HgiVulkanConversions::GetSamplerAddressMode(desc.addressModeW);

    // Eg. Percentage-closer filtering
    sampler.compareEnable = desc.enableCompare ? VK_TRUE : VK_FALSE;
    sampler.compareOp = 
        HgiVulkanConversions::GetDepthCompareFunction(desc.compareFunction);

    sampler.borderColor = 
        HgiVulkanConversions::GetBorderColor(desc.borderColor);
    sampler.mipLodBias = 0.0f;
    sampler.mipmapMode = HgiVulkanConversions::GetMipFilter(desc.mipFilter);
    sampler.minLod = 0.0f;
    sampler.maxLod = desc.mipFilter == HgiMipFilterNotMipmapped
        ? 0.25 : VK_LOD_CLAMP_NONE;
    // 0.25 if not mipmapped, to emulate OpenGL
    // See https://registry.khronos.org/vulkan/specs/latest/man/html/VkSamplerCreateInfo.html#_description

    if ((desc.minFilter != HgiSamplerFilterNearest ||
         desc.mipFilter == HgiMipFilterLinear) &&
         desc.magFilter != HgiSamplerFilterNearest) {
        HgiVulkanCapabilities const& caps = device->GetDeviceCapabilities();
        sampler.anisotropyEnable =
            caps.vkDeviceFeatures2.features.samplerAnisotropy;
        sampler.maxAnisotropy = sampler.anisotropyEnable ?
            std::min<float>({
                caps.vkDeviceProperties2.properties.limits.maxSamplerAnisotropy,
                static_cast<float>(desc.maxAnisotropy),
                static_cast<float>(TfGetEnvSetting(HGI_MAX_ANISOTROPY))}) : 1.0f;
    }

    HGIVULKAN_VERIFY_VK_RESULT(
        vkCreateSampler(
            device->GetVulkanDevice(),
            &sampler,
            HgiVulkanAllocator(),
            &_vkSampler)
    );
}

HgiVulkanSampler::~HgiVulkanSampler()
{
    vkDestroySampler(
        _device->GetVulkanDevice(),
        _vkSampler,
        HgiVulkanAllocator());
}

uint64_t
HgiVulkanSampler::GetRawResource() const
{
    return (uint64_t) _vkSampler;
}

VkSampler
HgiVulkanSampler::GetVulkanSampler() const
{
    return _vkSampler;
}

HgiVulkanDevice*
HgiVulkanSampler::GetDevice() const
{
    return _device;
}

uint64_t &
HgiVulkanSampler::GetInflightBits()
{
    return _inflightBits;
}

PXR_NAMESPACE_CLOSE_SCOPE