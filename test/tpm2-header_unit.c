/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2020, Intel Corporation
 */
#include <glib.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include <setjmp.h>
#include <cmocka.h>

#include "tpm2-header-gobject.h"
#include "util.h"

static void
tpm2_header_type_test (void **state)
{
    UNUSED_PARAM (state);

    Tpm2Header *header = tpm2_header_new (TPM2_ST_NO_SESSIONS,
                                          0,
                                          TPM2_CC_Clear);

    assert_true (G_IS_OBJECT (header));
    assert_true (IS_TPM2_HEADER (header));
}

int
main (void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test (tpm2_header_type_test),
    };
    return cmocka_run_group_tests (tests, NULL, NULL);
} 
