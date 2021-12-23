/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2019, Intel Corporation
 */
#include <gio/gio.h>
#include <glib.h>
#include <poll.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <setjmp.h>
#include <cmocka.h>

#define TEST_FD 5551555
#define TEST_SOCKET (GSocket*)8675309
#define TEST_CONNECTION ((GSocketConnection*)9035768)
#define TEST_ISTREAM (GInputStream*)583
#define TEST_FD_B 1115111

int
__wrap_socketpair (int domain,
                   int type,
                   int protocol,
                   int sv[2]);
int
__wrap_poll (struct pollfd *fds,
             nfds_t nfds,
             int timeout);
GSocket*
__wrap_g_socket_connection_get_socket (GSocketConnection *connection);
int
__wrap_g_socket_get_fd (GSocket *socket);
GInputStream*
__wrap_g_io_stream_get_input_stream (GIOStream *stream);
gssize
__wrap_g_input_stream_read (GInputStream *stream,
                            void *buffer,
                            gsize count,
                            GCancellable *cancellable,
                            GError **error);
GTypeInstance*
__real_g_type_check_instance_cast (GTypeInstance *type_instance,
                                    GType          iface_type);
GTypeInstance*
__wrap_g_type_check_instance_cast (GTypeInstance *type_instance,
                                    GType          iface_type);
