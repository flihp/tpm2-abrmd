/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2020, Intel Corporation
 */
#include <glib-object.h>
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

static void
tpm2_header_from_buffer_tag_fail (void **state)
{
    UNUSED_PARAM (state);
    uint8_t buf [TPM_HEADER_SIZE] = { 0, };
    size_t size = sizeof (buf);

    will_return (__wrap_Tss2_MU_TPM2_ST_Unmarshal, TSS2_MU_RC_BAD_REFERENCE);

    assert_null (tpm2_header_new_from_buffer (buf, size));
}

static void
tpm2_header_from_buffer_size_fail (void **state)
{
    UNUSED_PARAM (state);
    uint8_t buf [TPM_HEADER_SIZE] = { 0, };
    size_t size = sizeof (buf);

    will_return (__wrap_Tss2_MU_TPM2_ST_Unmarshal, TSS2_RC_SUCCESS);
    will_return (__wrap_Tss2_MU_TPM2_ST_Unmarshal, TPM2_ST_NO_SESSIONS);
    will_return (__wrap_Tss2_MU_UINT32_Unmarshal, TSS2_MU_RC_BAD_REFERENCE);

    assert_null (tpm2_header_new_from_buffer (buf, size));
}

static void
tpm2_header_from_buffer_code_fail (void **state)
{
    UNUSED_PARAM (state);
    uint8_t buf [TPM_HEADER_SIZE] = { 0, };
    size_t size = sizeof (buf);

    will_return (__wrap_Tss2_MU_TPM2_ST_Unmarshal, TSS2_RC_SUCCESS);
    will_return (__wrap_Tss2_MU_TPM2_ST_Unmarshal, TPM2_ST_NO_SESSIONS);
    will_return (__wrap_Tss2_MU_UINT32_Unmarshal, TSS2_RC_SUCCESS);
    will_return (__wrap_Tss2_MU_UINT32_Unmarshal, 10);
    will_return (__wrap_Tss2_MU_UINT32_Unmarshal, TSS2_MU_RC_BAD_REFERENCE);

    assert_null (tpm2_header_new_from_buffer (buf, size));
}
static void
tpm2_header_from_buffer_success (void **state)
{
    UNUSED_PARAM (state);
    uint8_t buf [TPM_HEADER_SIZE] = { 0, };
    size_t size = sizeof (buf);

    will_return (__wrap_Tss2_MU_TPM2_ST_Unmarshal, TSS2_RC_SUCCESS);
    will_return (__wrap_Tss2_MU_TPM2_ST_Unmarshal, TPM2_ST_NO_SESSIONS);
    will_return (__wrap_Tss2_MU_UINT32_Unmarshal, TSS2_RC_SUCCESS);
    will_return (__wrap_Tss2_MU_UINT32_Unmarshal, 10);
    will_return (__wrap_Tss2_MU_UINT32_Unmarshal, TSS2_RC_SUCCESS);
    will_return (__wrap_Tss2_MU_UINT32_Unmarshal, TPM2_CC_Startup);

    Tpm2Header* hdr = tpm2_header_new_from_buffer (buf, size);
    assert_non_null (hdr);
    g_clear_object (&hdr);
}
static void
tpm2_header_marshal_tag_fail (void **state)
{
    UNUSED_PARAM (state);
    TSS2_RC rc;
    uint8_t buf [TPM_HEADER_SIZE] = { 0, };
    size_t size = sizeof (buf);

    Tpm2Header* hdr = tpm2_header_new (TPM2_ST_NO_SESSIONS,
                                       10,
                                       TPM2_CC_Startup);

    will_return (__wrap_Tss2_MU_TPM2_ST_Marshal, TSS2_MU_RC_BAD_REFERENCE);
    rc = tpm2_header_marshal (hdr, buf, size);
    assert_int_equal (rc, TSS2_MU_RC_BAD_REFERENCE);
    g_clear_object (&hdr);
}
static void
tpm2_header_marshal_size_fail (void **state)
{
    UNUSED_PARAM (state);
    TSS2_RC rc;
    uint8_t buf [TPM_HEADER_SIZE] = { 0, };
    size_t size = sizeof (buf);

    Tpm2Header* hdr = tpm2_header_new (TPM2_ST_NO_SESSIONS,
                                       10,
                                       TPM2_CC_Startup);

    will_return (__wrap_Tss2_MU_TPM2_ST_Marshal, TSS2_RC_SUCCESS);
    will_return (__wrap_Tss2_MU_UINT32_Marshal, TSS2_MU_RC_BAD_REFERENCE);

    rc = tpm2_header_marshal (hdr, buf, size);
    assert_int_equal (rc, TSS2_MU_RC_BAD_REFERENCE);
    g_clear_object (&hdr);
}
static void
tpm2_header_marshal_code_fail (void **state)
{
    UNUSED_PARAM (state);
    TSS2_RC rc;
    uint8_t buf [TPM_HEADER_SIZE] = { 0, };
    size_t size = sizeof (buf);

    Tpm2Header* hdr = tpm2_header_new (TPM2_ST_NO_SESSIONS,
                                       10,
                                       TPM2_CC_Startup);

    will_return (__wrap_Tss2_MU_TPM2_ST_Marshal, TSS2_RC_SUCCESS);
    will_return (__wrap_Tss2_MU_UINT32_Marshal, TSS2_RC_SUCCESS);
    will_return (__wrap_Tss2_MU_UINT32_Marshal, TSS2_MU_RC_BAD_REFERENCE);

    rc = tpm2_header_marshal (hdr, buf, size);
    assert_int_equal (rc, TSS2_MU_RC_BAD_REFERENCE);
    g_clear_object (&hdr);
}
static void
tpm2_header_get_tag_test (void **state)
{
    UNUSED_PARAM (state);
    TPM2_ST tag = TPM2_ST_NO_SESSIONS;
    UINT32 size = TPM_HEADER_SIZE;
    UINT32 code = TPM2_CC_Startup;

    Tpm2Header* hdr = tpm2_header_new (tag, size, code);
    assert_int_equal (tag, tpm2_header_get_tag (hdr));
    g_clear_object (&hdr);
}
static void
tpm2_header_get_size_test (void **state)
{
    UNUSED_PARAM (state);
    TPM2_ST tag = TPM2_ST_NO_SESSIONS;
    UINT32 size = TPM_HEADER_SIZE;
    UINT32 code = TPM2_CC_Startup;

    Tpm2Header* hdr = tpm2_header_new (tag, size, code);
    assert_int_equal (size, tpm2_header_get_size (hdr));
    g_clear_object (&hdr);
}
static void
tpm2_header_get_command_code_test (void **state)
{
    UNUSED_PARAM (state);
    TPM2_ST tag = TPM2_ST_NO_SESSIONS;
    UINT32 size = TPM_HEADER_SIZE;
    TPM2_CC code = TPM2_CC_Startup;

    Tpm2Header* hdr = tpm2_header_new (tag, size, code);
    assert_int_equal (code, tpm2_header_get_command_code (hdr));
    g_clear_object (&hdr);
}
static void
tpm2_header_get_response_code_test (void **state)
{
    UNUSED_PARAM (state);
    TPM2_ST tag = TPM2_ST_NO_SESSIONS;
    UINT32 size = TPM_HEADER_SIZE;
    TSS2_RC code = TSS2_RC_SUCCESS;

    Tpm2Header* hdr = tpm2_header_new (tag, size, code);
    assert_int_equal (code, tpm2_header_get_response_code (hdr));
    g_clear_object (&hdr);
}
int
main (void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test (tpm2_header_type_test),
        cmocka_unit_test (tpm2_header_from_buffer_tag_fail),
        cmocka_unit_test (tpm2_header_from_buffer_size_fail),
        cmocka_unit_test (tpm2_header_from_buffer_code_fail),
        cmocka_unit_test (tpm2_header_from_buffer_success),
        cmocka_unit_test (tpm2_header_marshal_tag_fail),
        cmocka_unit_test (tpm2_header_marshal_size_fail),
        cmocka_unit_test (tpm2_header_marshal_code_fail),
        cmocka_unit_test (tpm2_header_get_tag_test),
        cmocka_unit_test (tpm2_header_get_size_test),
        cmocka_unit_test (tpm2_header_get_command_code_test),
        cmocka_unit_test (tpm2_header_get_response_code_test),
   };
    return cmocka_run_group_tests (tests, NULL, NULL);
} 
