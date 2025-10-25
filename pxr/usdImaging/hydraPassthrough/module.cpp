#include "pxr/pxr.h"
#include "pxr/base/tf/pyModule.h"

PXR_NAMESPACE_USING_DIRECTIVE

TF_WRAP_MODULE
{
    TF_WRAP(MaterialParam);
    TF_WRAP(RenderData);
    TF_WRAP(RenderManager);
    TF_WRAP(TextureDescriptor);
    TF_WRAP(ValueDescriptor);
}
