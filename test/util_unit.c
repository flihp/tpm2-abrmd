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

    ret = create_socket_pair (&client_fd, &server_fd, flags);
    if (ret == -1)
        g_error ("create_pipe_pair failed: %s", strerror (errno));
    close (client_fd);
}

static void
read_tpm_buf_alloc_success_test (void **state)
{
    data_t *data = *state;
    uint8_t *buf;
    size_t   buf_size;

    g_warning("before wrap shit");
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

    g_warning("before read_tpm_buffer_alloc shit");
    buf = read_tpm_buffer_alloc (data->sock_con, &buf_size);
    g_warning("after read_tpm_buffer_alloc shit");
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
    g_warning("before wrap shit");
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

gint
main (void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test (write_in_one),
        cmocka_unit_test (write_in_two),
        cmocka_unit_test (write_in_three),
        cmocka_unit_test (write_error),
        cmocka_unit_test (write_zero),
        cmocka_unit_test (create_socket_pair_success_test),
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
    };
    return cmocka_run_group_tests (tests, NULL, NULL);
}
