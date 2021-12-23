/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 * All rights reserved.
 */
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <setjmp.h>
#include <cmocka.h>

#include "util.h"
#include "tpm2-header.h"

#include "mock-funcs.h"

#define MAX_BUF 4096
#define READ_SIZE  UTIL_BUF_SIZE
#define WRITE_SIZE 10

#define UTIL_UNIT_ERROR util_unit_error_quark ()

GQuark
util_unit_error_quark (void)
{
    return g_quark_from_static_string ("util-unit-error-quark");
}

static void
g_debug_bytes_max (void **state)
{
    UNUSED_PARAM(state);
    g_debug_bytes (NULL, 0, 100, 40);
}

ssize_t
__wrap_g_output_stream_write (GOutputStream *ostream,
                              const void    *buf,
                              size_t         count,
                              GCancellable  *cancellable,
                              GError       **error)
{
    GError *error_tmp = mock_type (GError*);
    UNUSED_PARAM(ostream);
    UNUSED_PARAM(buf);
    UNUSED_PARAM(count);
    UNUSED_PARAM(cancellable);

    if (error_tmp != NULL && error != NULL) {
        *error = error_tmp;
    }
    return mock_type (ssize_t);
}

void
write_in_one (void **state)
{
    ssize_t written;
    UNUSED_PARAM(state);

    will_return (__wrap_g_output_stream_write, NULL);
    will_return (__wrap_g_output_stream_write, WRITE_SIZE);
    written = write_all (0, NULL, WRITE_SIZE);
    assert_int_equal (written, WRITE_SIZE);
}

void
write_in_two (void **state)
{
    ssize_t written;
    UNUSED_PARAM(state);

    will_return (__wrap_g_output_stream_write, NULL);
    will_return (__wrap_g_output_stream_write, 5);
    will_return (__wrap_g_output_stream_write, NULL);
    will_return (__wrap_g_output_stream_write, 5);
    written = write_all (0, NULL, WRITE_SIZE);
    assert_int_equal (written, WRITE_SIZE);
}

void
write_in_three (void **state)
{
    ssize_t written;
    UNUSED_PARAM(state);

    will_return (__wrap_g_output_stream_write, NULL);
    will_return (__wrap_g_output_stream_write, 3);
    will_return (__wrap_g_output_stream_write, NULL);
    will_return (__wrap_g_output_stream_write, 3);
    will_return (__wrap_g_output_stream_write, NULL);
    will_return (__wrap_g_output_stream_write, 4);
    written = write_all (0, NULL, WRITE_SIZE);
    assert_int_equal (written, WRITE_SIZE);
}

void
write_error (void **state)
{
    ssize_t written;
    GError *error;
    UNUSED_PARAM(state);

    /* this is free'd by the 'write_all' function */
    error = g_error_new (UTIL_UNIT_ERROR,
                         G_IO_ERROR_WOULD_BLOCK,
                         "g-io-error-would-block");
    will_return (__wrap_g_output_stream_write, error);
    will_return (__wrap_g_output_stream_write, -1);
    written = write_all (0, NULL, WRITE_SIZE);
    assert_int_equal (written, -1);
}

void
write_zero (void **state)
{
    ssize_t written;
    UNUSED_PARAM(state);

    will_return (__wrap_g_output_stream_write, NULL);
    will_return (__wrap_g_output_stream_write, 0);
    written = write_all (0, NULL, WRITE_SIZE);
    assert_int_equal (written, 0);
}

void
gerror_code_to_tcti_rc_no_connection (void **state)
{
    UNUSED_PARAM(state);
    assert_int_equal (gerror_code_to_tcti_rc (-1), TSS2_TCTI_RC_NO_CONNECTION);
}

void
gerror_code_to_tcti_rc_io_error (void **state)
{
    UNUSED_PARAM(state);
    TSS2_RC rc = gerror_code_to_tcti_rc (G_IO_ERROR_FAILED);
    assert_int_equal (rc, TSS2_TCTI_RC_IO_ERROR);
}

/*
 * This tests the poll_fd function, ensuring that it returns the
 * expected response code for the POLIN event.
 */
static void
poll_fd_fd_ready_pollin (void **state)
{
    UNUSED_PARAM (state);
    int ret;

    will_return (__wrap_poll, POLLIN);
    will_return (__wrap_poll, 0);
    will_return (__wrap_poll, 1);

    ret = poll_fd (TEST_FD, TSS2_TCTI_TIMEOUT_BLOCK);
    assert_int_equal (ret, 0);
}
/*
 * This tests the poll_fd function, ensuring that it returns the
 * expected response code for the POLLPRI event.
 */
static void
poll_fd_fd_ready_pollpri (void **state)
{
    UNUSED_PARAM (state);
    int ret;

    will_return (__wrap_poll, POLLPRI);
    will_return (__wrap_poll, 0);
    will_return (__wrap_poll, 1);

    ret = poll_fd (TEST_FD, TSS2_TCTI_TIMEOUT_BLOCK);
    assert_int_equal (ret, 0);
}
/*
 * This tests the poll_fd function, ensuring that it returns the
 * expected response code for the POLLRDHUP event.
 */

#if defined(__FreeBSD__)
#ifndef POLLRDHUP
#define POLLRDHUP 0x0
#endif
#endif
static void
poll_fd_fd_ready_pollrdhup (void **state)
{
    UNUSED_PARAM (state);
    int ret;

    will_return (__wrap_poll, POLLRDHUP);
    will_return (__wrap_poll, 0);
    will_return (__wrap_poll, 1);

    ret = poll_fd (TEST_FD, TSS2_TCTI_TIMEOUT_BLOCK);
    assert_int_equal (ret, 0);
}
/*
 * This tests the poll_fd function, ensuring that it returns the
 * expected response code when a timeout occurs.
 */
static void
poll_fd_timeout (void **state)
{
    UNUSED_PARAM (state);
    int ret;

    will_return (__wrap_poll, 0);
    will_return (__wrap_poll, 0);
    will_return (__wrap_poll, 0);

    ret = poll_fd (TEST_FD, TSS2_TCTI_TIMEOUT_BLOCK);
    assert_int_equal (ret, -1);
}
/*
 * This tests the poll_fd function, ensuring that it returns the
 * expected response when an error occurs.
 */
static void
poll_fd_error (void **state)
{
    UNUSED_PARAM (state);
    int ret;

    will_return (__wrap_poll, 0);
    will_return (__wrap_poll, EINVAL);
    will_return (__wrap_poll, -1);

    ret = poll_fd (TEST_FD, TSS2_TCTI_TIMEOUT_BLOCK);
    assert_int_equal (ret, EINVAL);
}
/* global static input array used by read_data* tests */
static uint8_t buf_in [MAX_BUF] = {
    /* header */
    0x80, 0x02, 0x00, 0x00, 0x00, 0x1a, 0x00, 0x00,
    0x00, 0x00,
    /* body */
    0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef
};
/*
 * Data structure to hold data for read tests.
 */
typedef struct {
    GSocketConnection *sock_con;
    size_t  index;
    uint8_t buf_out [MAX_BUF];
    size_t  buf_size;
    int client;
} data_t;

static int
read_data_setup (void **state)
{
    data_t *data;

    data = g_malloc0 (sizeof (data_t));
    data->buf_size = 26;
    data->sock_con = TEST_CONNECTION;

    *state = data;
    return 0;
}

static int
read_data_teardown (void **state)
{
    data_t *data = *state;

    if (data != NULL) {
        free (data);
    }
    return 0;
}
/*
 */
static void
create_socket_pair_success_test (void **state)
{
    int ret, client_fd, server_fd, flags = 0;
    UNUSED_PARAM(state);

#if !defined(__FreeBSD__)
    flags = O_CLOEXEC;
#endif

    will_return (__wrap_socketpair, TEST_FD);
    will_return (__wrap_socketpair, TEST_FD_B);
    will_return (__wrap_socketpair, 0);
    ret = create_socket_pair (&client_fd, &server_fd, flags);
    assert_int_equal (ret, 0);
    assert_int_equal (client_fd, TEST_FD);
    assert_int_equal (server_fd, TEST_FD_B);
}

static void
create_socket_pair_fail (void **state)
{
    int ret, fd_a = 0, fd_b = 0, flags = 0;
    UNUSED_PARAM(state);

#if !defined(__FreeBSD__)
    flags = O_CLOEXEC;
#endif

    will_return (__wrap_socketpair, 0);
    will_return (__wrap_socketpair, 0);
    will_return (__wrap_socketpair, -1);
    ret = create_socket_pair (&fd_a, &fd_b, flags);
    assert_int_equal (ret, -1);
}

static void
errno_to_tcti_rc_no_connection (void **state)
{
    UNUSED_PARAM(state);
    assert_int_equal (errno_to_tcti_rc (-1), TSS2_TCTI_RC_NO_CONNECTION);
}

static void
errno_to_tcti_rc_success (void **state)
{
    UNUSED_PARAM(state);
    assert_int_equal (errno_to_tcti_rc (0), TSS2_RC_SUCCESS);
}

static void
errno_to_tcti_rc_eagain (void **state)
{
    UNUSED_PARAM(state);
    assert_int_equal (errno_to_tcti_rc (EAGAIN), TSS2_TCTI_RC_TRY_AGAIN);
}

static void
errno_to_tcti_rc_eio (void **state)
{
    UNUSED_PARAM(state);
    assert_int_equal (errno_to_tcti_rc (EIO), TSS2_TCTI_RC_IO_ERROR);
}

/*
 * This test ensures that a call to read_with_timeout that causes poll to
 * timeout will return the appropriate RC.
 */
static void
read_with_timeout_poll_timeout (void **state)
{
    TSS2_RC rc;
    uint8_t resp [TPM2_MAX_RESPONSE_SIZE] = { 0, };
    size_t resp_size = sizeof (resp), index = 0;
    uint32_t timeout = TSS2_TCTI_TIMEOUT_BLOCK;
    data_t *data = *state;

    will_return (__wrap_g_socket_connection_get_socket, TEST_SOCKET);
    will_return (__wrap_g_socket_get_fd, TEST_FD);

    /* prime mock stack for poll, will return 0 indicating timeout */
    will_return (__wrap_poll, 0);
    will_return (__wrap_poll, 0);
    will_return (__wrap_poll, 0);

    rc = read_with_timeout (data->sock_con, resp, resp_size, &index, timeout);
    assert_int_equal (rc, TSS2_TCTI_RC_TRY_AGAIN);
}

/*
 * This test ensures that a call to read_with_timeout that causes poll to
 * fail / return an error that it will return the appropriate RC.
 */
static void
read_with_timeout_poll_fail (void **state)
{
    TSS2_RC rc;
    uint8_t resp [TPM2_MAX_RESPONSE_SIZE] = { 0, };
    size_t resp_size = sizeof (resp), index = 0;
    uint32_t timeout = TSS2_TCTI_TIMEOUT_BLOCK;
    data_t *data = *state;

    will_return (__wrap_g_socket_connection_get_socket, TEST_SOCKET);
    will_return (__wrap_g_socket_get_fd, TEST_FD);

    will_return (__wrap_poll, 0);
    will_return (__wrap_poll, EINVAL);
    will_return (__wrap_poll, -1);

    rc = read_with_timeout (data->sock_con, resp, resp_size, &index, timeout);
    assert_int_equal (rc, TSS2_TCTI_RC_GENERAL_FAILURE);
}
/*
 * This test ensures that a call to read_with_timeout that causes
 * g_input_stream_read to return EOF will return the appropriate RC.
 */
static void
read_with_timeout_eof (void **state)
{
    int ret;
    uint8_t resp [TPM2_MAX_RESPONSE_SIZE] = { 0, };
    size_t resp_size = sizeof (resp), index = 0;
    uint32_t timeout = TSS2_TCTI_TIMEOUT_BLOCK;
    data_t *data = *state;

    /* mock stack required to extract the fd from the GSocketConnection */
    will_return (__wrap_g_socket_connection_get_socket, TEST_SOCKET);
    will_return (__wrap_g_socket_get_fd, TEST_FD);

    /* prime mock stack for poll to indicate data is ready */
    will_return (__wrap_poll, POLLIN);
    will_return (__wrap_poll, 0);
    will_return (__wrap_poll, 1);

    /* mock stack required to extract GIStream */
    will_return (__wrap_g_io_stream_get_input_stream, TEST_CONNECTION);
    /* cause g_input_stream_read to return 0 indicating EOF */
    will_return (__wrap_g_input_stream_read, 0);

    ret = read_with_timeout (data->sock_con, resp, resp_size, &index, timeout);
    assert_int_equal (ret, TSS2_TCTI_RC_NO_CONNECTION);
}
/*
 * This test ensures that a call to read_with_timeout that causes
 * g_input_stream_read to indicate that it would block, returns the
 * appropriate RC.
 */
static void
read_with_timeout_block_error (void **state)
{
    int ret;
    uint8_t resp [TPM2_MAX_RESPONSE_SIZE] = { 0, };
    size_t resp_size = sizeof (resp), index = 0;
    uint32_t timeout = TSS2_TCTI_TIMEOUT_BLOCK;
    data_t *data = *state;
    GError* error;

    /* mock stack required to extract the fd from the GSocketConnection */
    will_return (__wrap_g_socket_connection_get_socket, TEST_SOCKET);
    will_return (__wrap_g_socket_get_fd, TEST_FD);

    /* prime mock stack for poll to indicate data is ready */
    will_return (__wrap_poll, POLLIN);
    will_return (__wrap_poll, 0);
    will_return (__wrap_poll, 1);

    /* mock stack required to extract GIStream & read data */
    will_return (__wrap_g_io_stream_get_input_stream, TEST_CONNECTION);
    error = g_error_new (1, G_IO_ERROR_WOULD_BLOCK, __func__);
    will_return (__wrap_g_input_stream_read, -1);
    will_return (__wrap_g_input_stream_read, error);

    ret = read_with_timeout (data->sock_con, resp, resp_size, &index, timeout);
    assert_int_equal (ret, TSS2_TCTI_RC_TRY_AGAIN);
}
/*
 * This test forces the call to 'g_input_stream_read' to read fewer bytes
 * than requested by the caller (the 'read_with_timeout' in this case). This
 * is a "short read" and should return an RC telling the caller to retry.
 */
static void
read_with_timeout_short (void **state)
{
    int ret;
    uint8_t resp [TPM2_MAX_RESPONSE_SIZE] = { 0, };
    size_t resp_size = sizeof (resp), read_size = resp_size / 2, index = 0;
    uint32_t timeout = TSS2_TCTI_TIMEOUT_BLOCK;
    data_t *data = *state;
    uint8_t buf [sizeof (resp)] = { 0, };

    /* mock stack required to extract the fd from the GSocketConnection */
    will_return (__wrap_g_socket_connection_get_socket, TEST_SOCKET);
    will_return (__wrap_g_socket_get_fd, TEST_FD);

    /* prime mock stack for poll to indicate data is ready */
    will_return (__wrap_poll, POLLIN);
    will_return (__wrap_poll, 0);
    will_return (__wrap_poll, 1);

    /* mock stack required to extract GIStream & read data */
    will_return (__wrap_g_io_stream_get_input_stream, TEST_CONNECTION);
    will_return (__wrap_g_input_stream_read, read_size);
    will_return (__wrap_g_input_stream_read, buf);

    ret = read_with_timeout (data->sock_con, resp, resp_size, &index, timeout);
    assert_int_equal (ret, TSS2_TCTI_RC_TRY_AGAIN);
}
static void
read_with_timeout_success (void **state)
{
    int ret;
    uint8_t resp [TPM2_MAX_RESPONSE_SIZE] = { 0, };
    uint8_t buf [TPM2_MAX_RESPONSE_SIZE] = { 0, };
    size_t resp_size = sizeof (resp), index = 0;
    uint32_t timeout = TSS2_TCTI_TIMEOUT_BLOCK;
    data_t *data = *state;

    /* prime mock stack for poll to indicate data is ready */
    will_return (__wrap_poll, POLLIN);
    will_return (__wrap_poll, 0);
    will_return (__wrap_poll, 1);
    /* mock stack required to extract the fd from the GSocketConnection */
    will_return (__wrap_g_socket_connection_get_socket, TEST_SOCKET);
    will_return (__wrap_g_socket_get_fd, TEST_FD);

    /* mock stack required to extract GIStream & read data */
    will_return (__wrap_g_io_stream_get_input_stream, TEST_CONNECTION);
    will_return (__wrap_g_input_stream_read, resp_size);
    will_return (__wrap_g_input_stream_read, buf);

    ret = read_with_timeout (data->sock_con, resp, resp_size, &index, timeout);
    assert_int_equal (ret, TSS2_RC_SUCCESS);
}
static void
read_tpm_buf_alloc_success_test (void **state)
{
    data_t *data = *state;
    uint8_t *buf;
    size_t   buf_size;

    /* do not mock g_malloc0: if glib cannot allocate memory it aborts */
    /* prime g_socket_connection_get_socket */
    will_return (__wrap_g_socket_connection_get_socket, TEST_SOCKET);
    /* prime g_socket_get_fd */
    will_return (__wrap_g_socket_get_fd, TEST_FD);
    /* prime poll to successfully */
    will_return (__wrap_poll, POLLIN); // data ready
    will_return (__wrap_poll, 0); // errno
    will_return (__wrap_poll, 1); // return value
    /* prime g_io_stream_get_input_stream */
    will_return (__wrap_g_io_stream_get_input_stream, TEST_CONNECTION);
    /* prime read to successfully produce the header */
    will_return (__wrap_g_input_stream_read, 10);
    will_return (__wrap_g_input_stream_read, buf_in);

    /* prime g_socket_connection_get_socket */
    will_return (__wrap_g_socket_connection_get_socket, TEST_SOCKET);
    /* prime g_socket_get_fd */
    will_return (__wrap_g_socket_get_fd, TEST_FD);
    /* prime poll to successfully */
    will_return (__wrap_poll, POLLIN); // data ready
    will_return (__wrap_poll, 0); // errno
    will_return (__wrap_poll, 1); // return value
    /* prime g_io_stream_get_input_stream */
    will_return (__wrap_g_io_stream_get_input_stream, TEST_CONNECTION);
    /* prime read to successfully produce the rest of the buffer */
    will_return (__wrap_g_input_stream_read, data->buf_size - 10);
    will_return (__wrap_g_input_stream_read, buf_in + 10);

    buf = read_tpm_buffer_alloc (data->sock_con, &buf_size);
    assert_non_null (buf);
    assert_int_equal (buf_size, data->buf_size);
    assert_memory_equal (buf, buf_in, data->buf_size);
    g_free (buf);
}

static void
read_tpm_buf_alloc_header_only_test (void **state)
{
    data_t *data = *state;
    uint8_t buf [10] = {
        0x80, 0x02, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x00
    };
    uint8_t *buf_out = NULL;

    /* prime read to successfully produce the header */
    data->buf_size = 10;
    /* prime g_socket_connection_get_socket */
    will_return (__wrap_g_socket_connection_get_socket, TEST_SOCKET);
    /* prime g_socket_get_fd */
    will_return (__wrap_g_socket_get_fd, TEST_FD);
    /* prime poll to successfully */
    will_return (__wrap_poll, POLLIN); // data ready
    will_return (__wrap_poll, 0); // errno
    will_return (__wrap_poll, 1); // return value
    /* prime g_io_stream_get_input_stream */
    will_return (__wrap_g_io_stream_get_input_stream, TEST_CONNECTION);
    /* prime read to successfully produce the header */
    will_return (__wrap_g_input_stream_read, 10);
    will_return (__wrap_g_input_stream_read, buf);

    buf_out = read_tpm_buffer_alloc (data->sock_con, &data->buf_size);
    assert_non_null (buf_out);
    assert_int_equal (data->buf_size, TPM_HEADER_SIZE);
    assert_memory_equal (buf_out, buf, data->buf_size);
    g_free (buf_out);
}
static void
read_tpm_buf_alloc_eof_test (void **state)
{
    data_t *data = *state;
    uint8_t *buf;
    size_t   buf_size;
    UNUSED_PARAM(state);

    /* prime g_socket_connection_get_socket */
    will_return (__wrap_g_socket_connection_get_socket, TEST_SOCKET);
    /* prime g_socket_get_fd */
    will_return (__wrap_g_socket_get_fd, TEST_FD);
    /* prime poll to successfully */
    will_return (__wrap_poll, POLLIN); // data ready
    will_return (__wrap_poll, 0); // errno
    will_return (__wrap_poll, 1); // return value
    /* prime g_io_stream_get_input_stream */
    will_return (__wrap_g_io_stream_get_input_stream, TEST_CONNECTION);
    /* prime read to successfully produce the header */
    will_return (__wrap_g_input_stream_read, 0);

    buf = read_tpm_buffer_alloc (data->sock_con, &buf_size);
    assert_null (buf);
}

static void
read_tpm_buf_alloc_short_read_header (void **state)
{
    data_t *data = *state;
    size_t  buf_size;
    uint8_t buf [TPM_HEADER_SIZE] = {
        0x80, 0x02,
        0x00, 0x00, 0x00, 0x0a, /* TPM_HEADER_SIZE -> resp is just header */
        0x00, 0x00, 0x00, 0x00,
    };

    /* first read */
    /* setup getter to return our mock socket */
    will_return (__wrap_g_socket_connection_get_socket, TEST_SOCKET);
    /* setup getter to return our mock fd */
    will_return (__wrap_g_socket_get_fd, TEST_FD);
    /* setup poll to succeed w/ data ready */
    will_return (__wrap_poll, POLLIN); // data ready
    will_return (__wrap_poll, 0); // errno
    will_return (__wrap_poll, 1); // return value
    /* setup getter to return our mock connection */
    will_return (__wrap_g_io_stream_get_input_stream, TEST_CONNECTION);
    /* setup read to produce the FIRST HALF of the header */
    will_return (__wrap_g_input_stream_read, TPM_HEADER_SIZE / 2);
    will_return (__wrap_g_input_stream_read, buf);

    /* second read */
    /* setup getter to return our mock socket */
    will_return (__wrap_g_socket_connection_get_socket, TEST_SOCKET);
    /* setup getter to return our mock fd */
    will_return (__wrap_g_socket_get_fd, TEST_FD);
    /* setup poll to succeed w/ data ready */
    will_return (__wrap_poll, POLLIN); // data ready
    will_return (__wrap_poll, 0); // errno
    will_return (__wrap_poll, 1); // return value
    /* setup getter to return our mock connection */
    will_return (__wrap_g_io_stream_get_input_stream, TEST_CONNECTION);
    /* setup read to produce the FIRST HALF of the header */
    will_return (__wrap_g_input_stream_read, TPM_HEADER_SIZE / 2);
    will_return (__wrap_g_input_stream_read, &buf[TPM_HEADER_SIZE / 2]);

    /* read_tpm_buffer_alloc: This test tries to read TPM_HEADER_SIZE bytes
       from a mock GSocketConnection using the read_tpm_buffer_alloc
       function. We mocked up the plumbing above to return
       TPM_HEADER_SIZE /2 bytes on the first read and the rest of the
       header on the next one. We expect the function under test to deal
       with these two reads transparently. It should return the expected
       bytes from the input 'buf'.
     */
    uint8_t *buf_out = NULL;
    buf_out = read_tpm_buffer_alloc (data->sock_con, &buf_size);
    assert_non_null (buf_out);
    assert_int_equal (TPM_HEADER_SIZE, buf_size);
    g_free (buf_out);
}

static void
read_tpm_buf_alloc_short_read_body (void **state)
{
    data_t *data = *state;
    size_t  buf_size = 0, short_index = 0;

    /* first read */
    /* setup getter to return our mock socket */
    will_return (__wrap_g_socket_connection_get_socket, TEST_SOCKET);
    /* setup getter to return our mock fd */
    will_return (__wrap_g_socket_get_fd, TEST_FD);
    /* setup poll to succeed w/ data ready */
    will_return (__wrap_poll, POLLIN); // data ready
    will_return (__wrap_poll, 0); // errno
    will_return (__wrap_poll, 1); // return value
    /* setup getter to return our mock connection */
    will_return (__wrap_g_io_stream_get_input_stream, TEST_CONNECTION);
    /* setup read to produce the header */
    will_return (__wrap_g_input_stream_read, TPM_HEADER_SIZE);
    will_return (__wrap_g_input_stream_read, buf_in);

    /* index into buf_in where we cause the short read to happen */
    short_index = get_command_size (buf_in) - 5;

    /* second read */
    /* setup getter to return our mock socket */
    will_return (__wrap_g_socket_connection_get_socket, TEST_SOCKET);
    /* setup getter to return our mock fd */
    will_return (__wrap_g_socket_get_fd, TEST_FD);
    /* setup poll to succeed w/ data ready */
    will_return (__wrap_poll, POLLIN); // data ready
    will_return (__wrap_poll, 0); // errno
    will_return (__wrap_poll, 1); // return value
    /* setup getter to return our mock connection */
    will_return (__wrap_g_io_stream_get_input_stream, TEST_CONNECTION);
    /* setup read to produce the FIRST HALF of the header */
    will_return (__wrap_g_input_stream_read, short_index - TPM_HEADER_SIZE);
    will_return (__wrap_g_input_stream_read, &buf_in [TPM_HEADER_SIZE]);

    /* third read */
    /* setup getter to return our mock socket */
    will_return (__wrap_g_socket_connection_get_socket, TEST_SOCKET);
    /* setup getter to return our mock fd */
    will_return (__wrap_g_socket_get_fd, TEST_FD);
    /* setup poll to succeed w/ data ready */
    will_return (__wrap_poll, POLLIN); // data ready
    will_return (__wrap_poll, 0); // errno
    will_return (__wrap_poll, 1); // return value
    /* setup getter to return our mock connection */
    will_return (__wrap_g_io_stream_get_input_stream, TEST_CONNECTION);
    /* setup read to produce the FIRST HALF of the header */
    will_return (__wrap_g_input_stream_read, 5);
    will_return (__wrap_g_input_stream_read, &buf_in [short_index]);

    /* read_tpm_buffer_alloc: This test tries to read sizeof (buf_in) bytes
       from a mock GSocketConnection using the read_tpm_buffer_alloc
       function. We mocked up the plumbing above to cause a short read in
       the body of the TPM command we're trying to read, with the rest of
       the body read on the second read from the socket. We expect the
       function under test to deal with these two reads transparently. It
       should return the expected bytes from buf_in.
     */
    uint8_t *buf_out = NULL;
    buf_out = read_tpm_buffer_alloc (data->sock_con, &buf_size);
    assert_non_null (buf_out);
    assert_int_equal (get_command_size (buf_in), buf_size);
}

gint
main (void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test (g_debug_bytes_max),
        cmocka_unit_test (write_in_one),
        cmocka_unit_test (write_in_two),
        cmocka_unit_test (write_in_three),
        cmocka_unit_test (write_error),
        cmocka_unit_test (write_zero),
        cmocka_unit_test (gerror_code_to_tcti_rc_no_connection),
        cmocka_unit_test (gerror_code_to_tcti_rc_io_error),
        cmocka_unit_test (poll_fd_fd_ready_pollin),
        cmocka_unit_test (poll_fd_fd_ready_pollpri),
        cmocka_unit_test (poll_fd_fd_ready_pollrdhup),
        cmocka_unit_test (poll_fd_timeout),
        cmocka_unit_test (poll_fd_error),
        cmocka_unit_test (create_socket_pair_success_test),
        cmocka_unit_test (create_socket_pair_fail),
        cmocka_unit_test (errno_to_tcti_rc_no_connection),
        cmocka_unit_test (errno_to_tcti_rc_success),
        cmocka_unit_test (errno_to_tcti_rc_eagain),
        cmocka_unit_test (errno_to_tcti_rc_eio),
        /* read_with_timeout */
        cmocka_unit_test_setup_teardown (read_with_timeout_poll_timeout,
                                         read_data_setup,
                                         read_data_teardown),
        cmocka_unit_test_setup_teardown (read_with_timeout_poll_fail,
                                         read_data_setup,
                                         read_data_teardown),
        cmocka_unit_test_setup_teardown (read_with_timeout_eof,
                                         read_data_setup,
                                         read_data_teardown),
        cmocka_unit_test_setup_teardown (read_with_timeout_block_error,
                                         read_data_setup,
                                         read_data_teardown),
        cmocka_unit_test_setup_teardown (read_with_timeout_short,
                                         read_data_setup,
                                         read_data_teardown),
        cmocka_unit_test_setup_teardown (read_with_timeout_success,
                                         read_data_setup,
                                         read_data_teardown),
        /* read_tpm_buffer_alloc*/
        cmocka_unit_test_setup_teardown (read_tpm_buf_alloc_success_test,
                                         read_data_setup,
                                         read_data_teardown),
        cmocka_unit_test_setup_teardown (read_tpm_buf_alloc_header_only_test,
                                         read_data_setup,
                                         read_data_teardown),
        cmocka_unit_test_setup_teardown (read_tpm_buf_alloc_eof_test,
                                         read_data_setup,
                                         read_data_teardown),
        cmocka_unit_test_setup_teardown (read_tpm_buf_alloc_short_read_header,
                                         read_data_setup,
                                         read_data_teardown),
        cmocka_unit_test_setup_teardown (read_tpm_buf_alloc_short_read_body,
                                         read_data_setup,
                                         read_data_teardown),
    };
    return cmocka_run_group_tests (tests, NULL, NULL);
}
