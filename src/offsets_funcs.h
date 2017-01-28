#ifndef OFFSETS_FUNCS_H
#define OFFSETS_FUNCS_H

#include "types.h"

#include "patch/func.h"

void InitBwFuncs_1161(Common::PatchManager *exec_heap, uintptr_t current_base_address);
void InitStormFuncs_1161(Common::PatchManager *exec_heap, uintptr_t current_base_address);

#include "offsets_funcs_1161.h"

#endif /* OFFSETS_FUNCS_H */
