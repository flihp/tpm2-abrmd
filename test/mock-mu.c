/* SPDX-License-Identifier: BSD-2-Clause */

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>

#include <setjmp.h>
#include <cmocka.h>

#include <tss2/tss2_mu.h>

#include "util.h"

TSS2_RC
__wrap_Tss2_MU_TPM2_ST_Marshal(TPM2_ST src,
                               uint8_t buffer[],
                               size_t  buffer_size,
                               size_t* offset)
{
    assert (buffer);
    assert (offset);

    UNUSED_PARAM (src);
    UNUSED_PARAM (buffer);
    UNUSED_PARAM (buffer_size);

    TSS2_RC rc = mock_type (TSS2_RC);
    if (!rc) {
        *offset += sizeof (TPM2_ST);
    }

    return rc;
}

TSS2_RC
__wrap_Tss2_MU_TPM2_ST_Unmarshal (uint8_t const buffer[],
                                  size_t buffer_size,
                                  size_t* offset,
                                  TPM2_ST* dest)
{
    assert (buffer);
    assert (offset);
    assert (dest);

    UNUSED_PARAM (buffer);
    UNUSED_PARAM (buffer_size);

    TSS2_RC rc = mock_type (TSS2_RC);
    if (!rc) {
        *dest = mock_type (TPM2_ST);
        *offset += sizeof (TPM2_ST);
    }

    return rc;
}
TSS2_RC
__wrap_Tss2_MU_UINT32_Marshal(UINT32 src,
                              uint8_t buffer[],
                              size_t  buffer_size,
                              size_t* offset)
{
    assert (buffer);
    assert (offset);

    UNUSED_PARAM (src);
    UNUSED_PARAM (buffer);
    UNUSED_PARAM (buffer_size);

    TSS2_RC rc = mock_type (TSS2_RC);
    if (!rc) {
        *offset += sizeof (UINT32);
    }

    return rc;
}
TSS2_RC
__wrap_Tss2_MU_UINT32_Unmarshal (uint8_t const buffer[],
                                 size_t buffer_size,
                                 size_t* offset,
                                 UINT32* dest)
{
    assert (buffer);
    assert (offset);
    assert (dest);

    UNUSED_PARAM (buffer);
    UNUSED_PARAM (buffer_size);

    TSS2_RC rc = mock_type (TSS2_RC);
    if (!rc) {
        *dest = mock_type (UINT32);
        *offset += (sizeof (UINT32));
    }

    return rc;
}
