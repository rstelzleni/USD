#include "pxr/usdImaging/hydraPassthrough/textureDescriptor.h"

#include <sstream>

PXR_NAMESPACE_OPEN_SCOPE

std::string HydraPassthroughTextureDescriptor::ToString() const {
    std::stringstream ss;
    ss << "HydraPassthroughTextureDescriptor {" << std::endl;
    ss << "  name: " << name << std::endl;
    ss << "  filePath: " << filePath << std::endl;
    ss << "  type: " << (int)type << std::endl;
    ss << "  samplerParameters: " << std::endl;
    ss << "    wrapS: " << (int)samplerParameters.wrapS << std::endl;
    ss << "    wrapT: " << (int)samplerParameters.wrapT << std::endl;
    ss << "    wrapR: " << (int)samplerParameters.wrapR << std::endl;
    ss << "    minFilter: " << (int)samplerParameters.minFilter << std::endl;
    ss << "    magFilter: " << (int)samplerParameters.magFilter << std::endl;
    ss << "    borderColor: " << (int)samplerParameters.borderColor<< std::endl;
    ss << "    enableCompare: " << (int)samplerParameters.enableCompare << std::endl;
    ss << "    maxAnisotropy: " << samplerParameters.maxAnisotropy << std::endl;
    ss << "  memoryRequest: " << memoryRequest << std::endl;
    ss << "}";
    return ss.str();
}

PXR_NAMESPACE_CLOSE_SCOPE
