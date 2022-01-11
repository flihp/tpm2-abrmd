/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2019, Intel Corporation
 */
#include <errno.h>
#include <gio/gio.h>
#include <glib.h>
#include <inttypes.h>
#include <poll.h>
#include <sys/socket.h>

#include "mock-funcs.h"

gboolean __real_g_socket_condition_timed_wait (GSocket *socket,
                                               GIOCondition condition,
                                               gint64 timeout,
                                               GCancellable *cancellable,
                                               GError **error);
gboolean
__wrap_g_socket_condition_timed_wait (GSocket *socket,
                                      GIOCondition condition,
                                      gint64 timeout,
                                      GCancellable *cancellable,
                                      GError **error)
{
    gboolean ret = mock_type (gboolean);

    g_debug ("%s", __func__);
    if (socket != TEST_SOCKET) {
        g_debug ("%s: not TEST_SOCKET, calling real func", __func__);
        return __real_g_socket_condition_timed_wait (socket,
                                                     condition,
                                                     timeout,
                                                     cancellable,
                                                     error);
    }

    if (!ret) {
        g_debug ("%s: simulating error with new GError", __func__);
        *error = g_error_new (G_IO_ERROR,
                              mock_type (gint),
                              __func__);
    }

    return ret;
}
/*
 * The mock functions for the g_socket_* stuff are required to test the
 * tcti_tabrmd_read function. They are used to extract the GInputStream
 * from the GSocketConnection stored in the TCTI context structure.
 *
 * These mock functions are written to mock the implementation only when
 * passed known values used in this module. It will call the 'real'
 * if passed a real glib object.
 */
GSocket* __real_g_socket_connection_get_socket (GSocketConnection *connection);
GSocket*
__wrap_g_socket_connection_get_socket (GSocketConnection *connection)
{
    g_debug ("%s: 0x%" PRIxPTR, __func__, (uintptr_t)connection);
    if (connection != TEST_CONNECTION) {
        return __real_g_socket_connection_get_socket (connection);
    } else {
        return mock_type (GSocket*);
    }
}
int __real_g_socket_get_fd (GSocket *socket);
int
__wrap_g_socket_get_fd (GSocket *socket)
{
    g_debug ("%s: 0x%" PRIxPTR, __func__, (uintptr_t)socket);
    if (socket != TEST_SOCKET) {
        return __real_g_socket_get_fd (socket);
    } else {
        return mock_type (int);
    }
}
GInputStream* __real_g_io_stream_get_input_stream (GIOStream *stream);
GInputStream*
__wrap_g_io_stream_get_input_stream (GIOStream *stream)
{
    g_debug ("%s iostream: 0x%" PRIxPTR, __func__, (uintptr_t)stream);
    if (stream != (GIOStream*)TEST_CONNECTION) {
        return __real_g_io_stream_get_input_stream (stream);
    } else {
        return mock_type (GInputStream*);
    }
}
/*
 * The mock functions for the GInputStream reading function is required to
 * test the  * tcti_tabrmd_read function. This is almost exactly like a
 * mock function the 'read' system call but handles the glib stuff too.
 *
 * This mock function is  written to mock the implementation only when
 * passed known values used in this module. It will call the 'real'
 * if passed a real glib object.
 */
gssize
__real_g_input_stream_read (GInputStream *stream,
                            void *buffer,
                            gsize count,
                            GCancellable *cancellable,
                            GError **error);
gssize
__wrap_g_input_stream_read (GInputStream *stream,
                            void *buffer,
                            gsize count,
                            GCancellable *cancellable,
                            GError **error)
{
    gssize resp_size;
    uint8_t *resp;

    g_debug ("%s stream: 0x%" PRIxPTR, __func__, (uintptr_t)stream);
    if (stream != (GInputStream*)TEST_CONNECTION) {
        return __real_g_input_stream_read (stream,
                                           buffer,
                                           count,
                                           cancellable,
                                           error);
    } else {
        resp_size = mock_type (gssize);
        if (resp_size > 0) {
            resp = mock_type (uint8_t*);
            memcpy (buffer, resp, resp_size);
        } else if (resp_size == -1) {
            *error = mock_type (GError*);
        }
        return resp_size;
    }
}
