//
// Copyright 2017 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef PXR_USD_IMAGING_HYDRA_PASSTHROUGH_API_H
#define PXR_USD_IMAGING_HYDRA_PASSTHROUGH_API_H

#include "pxr/base/arch/export.h"

#if defined(PXR_STATIC)
#   define HYDRA_PASSTHROUGH_API
#   define HYDRA_PASSTHROUGH_API_TEMPLATE_CLASS(...)
#   define HYDRA_PASSTHROUGH_API_TEMPLATE_STRUCT(...)
#   define HYDRA_PASSTHROUGH_LOCAL
#else
#   if defined(HYDRA_PASSTHROUGH_EXPORTS)
#       define HYDRA_PASSTHROUGH_API ARCH_EXPORT
#       define HYDRA_PASSTHROUGH_API_TEMPLATE_CLASS(...) ARCH_EXPORT_TEMPLATE(class, __VA_ARGS__)
#       define HYDRA_PASSTHROUGH_API_TEMPLATE_STRUCT(...) ARCH_EXPORT_TEMPLATE(struct, __VA_ARGS__)
#   else
#       define HYDRA_PASSTHROUGH_API ARCH_IMPORT
#       define HYDRA_PASSTHROUGH_API_TEMPLATE_CLASS(...) ARCH_IMPORT_TEMPLATE(class, __VA_ARGS__)
#       define HYDRA_PASSTHROUGH_API_TEMPLATE_STRUCT(...) ARCH_IMPORT_TEMPLATE(struct, __VA_ARGS__)
#   endif
#   define HYDRA_PASSTHROUGH_LOCAL ARCH_HIDDEN
#endif

#endif // PXR_USD_IMAGING_HYDRA_PASSTHROUGH_API_H
