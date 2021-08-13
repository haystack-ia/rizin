// SPDX-FileCopyrightText: 2021 heersin <teablearcher@gmail.com>
// SPDX-License-Identifier: LGPL-3.0-only

#ifndef RZ_IL_BOOL_H
#define RZ_IL_BOOL_H
#include "common.h"

struct bool_t {
	bool b;
};
typedef struct bool_t *RzILBool;

RZ_API RzILBool rz_il_new_bool(bool true_or_false);
RZ_API RzILBool rz_il_bool_and_(RzILBool a, RzILBool b);
RZ_API RzILBool rz_il_bool_or_(RzILBool a, RzILBool b);
RZ_API RzILBool rz_il_bool_xor_(RzILBool a, RzILBool b);
RZ_API RzILBool rz_il_bool_not_(RzILBool a);
RZ_API void rz_il_free_bool(RzILBool bool_var);

#endif // RZ_IL_BOOL_H
