// SPDX-FileCopyrightText: 2021 heersin <teablearcher@gmail.com>
// SPDX-License-Identifier: LGPL-3.0-only

#ifndef RZ_IL_CAST_H
#define RZ_IL_CAST_H

#include "value.h"

RZ_API RzILBitVector val_cast_to_bitv(RzILVal val);
RZ_API RzILBool val_cast_to_bool(RzILVal val);

RZ_API RzILVal bitv_cast_to_val(RzILBitVector bv);
RZ_API RzILVal bool_cast_to_val(RzILBool b);

#endif // RZ_IL_CAST_H
