/* SPDX-License-Identifier: BSD-2-Clause */

#include <stdlib.h>
#include <tss2/tss2_tpm2_types>

TSS2_RC
__wrap_Tss2_MU_TPM2_ST_Marshal(TPM2_ST src,
                               uint8_t buffer[],
                               size_t  buffer_size,
                               size_t* offset);
TSS2_RC
__wrap_Tss2_MU_TPM2_ST_Unmarshal (uint8_t const buffer[],
                                  size_t buffer_size,
                                  size_t* offset,
                                  TPM2_ST* dest);
TSS2_RC
__wrap_Tss2_MU_UINT32_Marshal(UINT32 src,
                              uint8_t buffer[],
                              size_t buffer_size,
                              size_t* offset);
TSS2_RC
_wrap_Tss2_MU_UINT32_Unmarshal (uint8_t const buffer[],
                                size_t buffer_size,
                                size_t* offset,
                                UINT32* dest);
