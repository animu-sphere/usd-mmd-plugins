//
// Copyright 2017 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef MMDSCHEMA_API_H
#define MMDSCHEMA_API_H

#include "pxr/base/arch/export.h"

#if defined(PXR_STATIC)
#   define MMDSCHEMA_API
#   define MMDSCHEMA_API_TEMPLATE_CLASS(...)
#   define MMDSCHEMA_API_TEMPLATE_STRUCT(...)
#   define MMDSCHEMA_LOCAL
#else
#   if defined(MMDSCHEMA_EXPORTS)
#       define MMDSCHEMA_API ARCH_EXPORT
#       define MMDSCHEMA_API_TEMPLATE_CLASS(...) ARCH_EXPORT_TEMPLATE(class, __VA_ARGS__)
#       define MMDSCHEMA_API_TEMPLATE_STRUCT(...) ARCH_EXPORT_TEMPLATE(struct, __VA_ARGS__)
#   else
#       define MMDSCHEMA_API ARCH_IMPORT
#       define MMDSCHEMA_API_TEMPLATE_CLASS(...) ARCH_IMPORT_TEMPLATE(class, __VA_ARGS__)
#       define MMDSCHEMA_API_TEMPLATE_STRUCT(...) ARCH_IMPORT_TEMPLATE(struct, __VA_ARGS__)
#   endif
#   define MMDSCHEMA_LOCAL ARCH_HIDDEN
#endif

#endif
