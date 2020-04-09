/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2017, Intel Corporation
 * All rights reserved.
 */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <tss2/tss2_tpm2_types.h>

#include "connection.h"
#include "random.h"
#include "util.h"
#include "tpm2-header.h"

#if defined(__FreeBSD__)
#ifndef POLLRDHUP
#define POLLRDHUP 0x0
#endif
#endif

/**
 * This is a wrapper around g_debug to dump a binary buffer in a human
 * readable format. Since g_debug appends a new line to each string that
 * it builds we dump a single line at a time. Each line is indented by
 * 'indent' spaces. The 'width' parameter determines how many bytes are
 * output on each line.
 */
#define MAX_LINE_LENGTH 200
void
g_debug_bytes (uint8_t const *byte_array,
               size_t         array_size,
               size_t         width,
               size_t         indent)
{
    guint byte_ctr;
    guint indent_ctr;
    size_t line_length = indent + width * 3 + 1;
    char  line [MAX_LINE_LENGTH] = { 0 };
    char  *line_position = NULL;

    if (line_length > MAX_LINE_LENGTH) {
        g_warning ("g_debug_bytes: MAX_LINE_LENGTH exceeded");
        return;
    }

    for (byte_ctr = 0; byte_ctr < array_size; ++byte_ctr) {
        /* index into line where next byte is written */
        line_position = line + indent + (byte_ctr % width) * 3;
        /* detect the beginning of a line, pad indent spaces */
        if (byte_ctr % width == 0)
            for (indent_ctr = 0; indent_ctr < indent; ++indent_ctr)
                line [indent_ctr] = ' ';
        sprintf (line_position, "%02x", byte_array [byte_ctr]);
        /**
         *  If we're not width bytes into the array AND we're not at the end
         *  of the byte array: print a space. This is padding between the
         *  current byte and the next.
         */
        if (byte_ctr % width != width - 1 && byte_ctr != array_size - 1) {
            sprintf (line_position + 2, " ");
        } else {
            g_debug ("%s", line);
        }
    }
}
/** Write as many of the size bytes from buf to fd as possible.
 */
ssize_t
write_all (GOutputStream *ostream,
           const uint8_t *buf,
           const size_t   size)
{
    ssize_t written = 0;
    size_t written_total = 0;
    GError *error = NULL;

    do {
        g_debug ("%s: writing %zu bytes to ostream", __func__,
                 size - written_total);
        written = g_output_stream_write (ostream,
                                         (const gchar*)&buf [written_total],
                                         size - written_total,
                                         NULL,
                                         &error);
        switch (written) {
        case -1:
            g_assert (error != NULL);
            g_warning ("%s: failed to write to ostream: %s", __func__, error->message);
            g_error_free (error);
            return written;
        case  0:
            return (ssize_t)written_total;
        default:
            g_debug ("%s: wrote %zd bytes to ostream", __func__, written);
        }
        written_total += (size_t)written;
    } while (written_total < size);
    g_debug ("returning %zu", written_total);

    return (ssize_t)written_total;
}
/*
 * This function maps GError code values to TCTI RCs.
 */
TSS2_RC
gerror_code_to_tcti_rc (int error_number)
{
    switch (error_number) {
    case -1:
        return TSS2_TCTI_RC_NO_CONNECTION;
    case G_IO_ERROR_WOULD_BLOCK:
        return TSS2_TCTI_RC_TRY_AGAIN;
    case G_IO_ERROR_FAILED:
    case G_IO_ERROR_HOST_UNREACHABLE:
    case G_IO_ERROR_NETWORK_UNREACHABLE:
#if G_IO_ERROR_BROKEN_PIPE != G_IO_ERROR_CONNECTION_CLOSED
    case G_IO_ERROR_CONNECTION_CLOSED:
#endif
#if GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION >= 44
    case G_IO_ERROR_NOT_CONNECTED:
#endif
        return TSS2_TCTI_RC_IO_ERROR;
    default:
        g_debug ("mapping errno %d with message \"%s\" to "
                 "TSS2_TCTI_RC_GENERAL_FAILURE",
                 error_number, strerror (error_number));
        return TSS2_TCTI_RC_GENERAL_FAILURE;
    }
}
/*
 * This is a thin wrapper around a call to poll. It packages up the provided
 * file descriptor and timeout and polls on that same FD for data or a hangup.
 * Returns:
 *   -1 on timeout
 *   0 when data is ready
 *   errno on error
 */
int
poll_fd (int        fd,
         int32_t    timeout)
{
    struct pollfd pollfds [] = {
        {
            .fd = fd,
             .events = POLLIN | POLLPRI | POLLRDHUP,
        }
    };
    int ret;
    int errno_tmp;

    ret = TABRMD_ERRNO_EINTR_RETRY (poll (pollfds,
                                    sizeof (pollfds) / sizeof (struct pollfd),
                                    timeout));
    errno_tmp = errno;
    switch (ret) {
    case -1:
        g_debug ("poll produced error: %d, %s",
                 errno_tmp, strerror (errno_tmp));
        return errno_tmp;
    case 0:
        g_debug ("poll timed out after %" PRId32 " milliseconds", timeout);
        return -1;
    default:
        g_debug ("poll has %d fds ready", ret);
        if (pollfds[0].revents & POLLIN) {
            g_debug ("  POLLIN");
        }
        if (pollfds[0].revents & POLLPRI) {
            g_debug ("  POLLPRI");
        }
        if (pollfds[0].revents & POLLRDHUP) {
            g_debug ("  POLLRDHUP");
        }
        return 0;
    }
}
/*
 * This function maps errno values to TCTI RCs.
 */
TSS2_RC
errno_to_tcti_rc (int error_number)
{
    switch (error_number) {
    case -1:
        return TSS2_TCTI_RC_NO_CONNECTION;
    case 0:
        return TSS2_RC_SUCCESS;
    case EAGAIN:
#if EAGAIN != EWOULDBLOCK
    case EWOULDBLOCK:
#endif
        return TSS2_TCTI_RC_TRY_AGAIN;
    case EIO:
        return TSS2_TCTI_RC_IO_ERROR;
    default:
        g_debug ("mapping errno %d with message \"%s\" to "
                 "TSS2_TCTI_RC_GENERAL_FAILURE",
                 error_number, strerror (error_number));
        return TSS2_TCTI_RC_GENERAL_FAILURE;
    }
}
/* poll for data with timeout, then read as much as you can up to size */
TSS2_RC
read_with_timeout (GSocketConnection *connection,
                   uint8_t *buf,
                   size_t size,
                   size_t *index,
                   int32_t timeout)
{
    GError *error;
    ssize_t num_read;
    int ret;

    ret = poll_fd (g_socket_get_fd (g_socket_connection_get_socket (connection)), timeout);
    switch (ret) {
    case -1:
        return TSS2_TCTI_RC_TRY_AGAIN;
    case 0:
        break;
    default:
        return errno_to_tcti_rc (ret);
    }

    num_read = g_input_stream_read (g_io_stream_get_input_stream (G_IO_STREAM (connection)),
                                    (gchar*)&buf [*index],
                                    size,
                                    NULL,
                                    &error);
    switch (num_read) {
    case 0:
        g_debug ("read produced EOF");
        return TSS2_TCTI_RC_NO_CONNECTION;
    case -1:
        g_assert (error != NULL);
        g_warning ("%s: read on istream produced error: %s", __func__,
                   error->message);
        ret = error->code;
        g_error_free (error);
        return gerror_code_to_tcti_rc (ret);
    default:
        g_debug ("successfully read %zd bytes", num_read);
        g_debug_bytes (&buf [*index], num_read, 16, 4);
        /* Advance index by the number of bytes read. */
        *index += num_read;
        /* short read means try again */
        if (size - num_read != 0) {
            return TSS2_TCTI_RC_TRY_AGAIN;
        } else {
            return TSS2_RC_SUCCESS;
        }
    }
}
/*
 * This function attempts to read a TPM2 command or response into the provided
 * buffer. It specifically handles the details around reading the command /
 * response header, determining the size of the data that it needs to read and
 * keeping track of past / partial reads.
 * Returns:
 *   -1: If the underlying syscall results in an EOF
 *   0: If data is successfully read.
 *      NOTE: The index will be updated to the size of the command buffer.
 *   errno: In the event of an error from the underlying 'read' syscall.
 *   EPROTO: If buf_size is less than the size from the command buffer.
 */
int
read_tpm_buffer (GSocketConnection        *socket_con,
                 uint8_t                  *buf,
                 size_t                    buf_size,
                 size_t                   *index)
{
    ssize_t ret = 0;
    uint32_t size = 0;

    /* if the buf_size isn't at least large enough to hold the header */
    if (buf_size < TPM_HEADER_SIZE) {
        return EPROTO;
    }
    /* If we don't have the whole header yet try to get it. */
    if (*index < TPM_HEADER_SIZE) {
        ret = read_with_timeout (socket_con,
                                 buf,
                                 TPM_HEADER_SIZE - *index,
                                 index,
                                 TSS2_TCTI_TIMEOUT_BLOCK);
        if (ret != 0) {
            /* Pass errors up to the caller. */
            return ret;
        }
    }

    /* Once we have the header we can get the size of the whole blob. */
    size = get_command_size (buf);
    /* If size from header is size of header, there's nothing more to read. */
    if (size == TPM_HEADER_SIZE) {
        return ret;
    }
    /* Not enough space in buf to for data in the buffer (header.size). */
    if (size > buf_size) {
        return EPROTO;
    }
    /* Now that we have the header, we know the whole buffer size. Get it. */
    return read_with_timeout (socket_con,
                              buf,
                              size - *index,
                              index,
                              TSS2_TCTI_TIMEOUT_BLOCK);
}
/*
 * This fucntion is a wrapper around the read_tpm_buffer function above. It
 * adds the memory allocation logic necessary to create the buffer to hold
 * the TPM command / response buffer.
 * Returns NULL on error, and a pointer to the allocated buffer on success.
 *   The size of the allocated buffer is returned through the *buf_size
 *   parameter on success.
 */
uint8_t*
read_tpm_buffer_alloc (Connection *connection,
                       size_t       *buf_size)
{
    uint8_t *buf = NULL;
    size_t   size_tmp = TPM_HEADER_SIZE, index = 0;
    int ret = 0;
    GSocketConnection *sock_con;

    assert (connection);
    assert (buf_size);

    sock_con = connection_get_sockcon (connection);
    do {
        buf = g_realloc (buf, size_tmp);
        ret = read_tpm_buffer (sock_con,
                               buf,
                               size_tmp,
                               &index);
        switch (ret) {
        case EPROTO:
            size_tmp = get_command_size (buf);
            if (size_tmp < TPM_HEADER_SIZE || size_tmp > UTIL_BUF_MAX) {
                g_warning ("%s: tpm buffer size is ouside of acceptable bounds: %zd",
                           __func__, size_tmp);
                goto err_out;
            }
            break;
        case 0:
            /* done */
            break;
        default:
            goto err_out;
        }
    } while (ret == EPROTO);
    g_debug ("%s: read TPM buffer of size: %zd", __func__, index);
    g_debug_bytes (buf, index, 16, 4);
    *buf_size = size_tmp;
    return buf;
err_out:
    g_debug ("%s: err_out freeing buffer", __func__);
    if (buf != NULL) {
        g_free (buf);
    }
    return NULL;
}
/*
 * The client end of the socket is returned through the client_fd
 * parameter.
 */
GSocketConnection*
create_socket_connection (int *client_fd)
{
    GSocketConnection *socket_con;
    GSocket *socket;
    int server_fd, ret;

    ret = create_socket_pair (client_fd,
                              &server_fd,
                              SOCK_CLOEXEC | SOCK_NONBLOCK);
    if (ret == -1) {
        g_error ("CreateConnection failed to make fd pair %s", strerror (errno));
    }
    socket = g_socket_new_from_fd (server_fd, NULL);
    socket_con = g_socket_connection_factory_create_connection (socket);
    g_object_unref (socket);
    return socket_con;
}
/*
 * Create a socket and return the fds for both ends of the communication
 * channel.
 */
int
create_socket_pair (int *fd_a,
                    int *fd_b,
                    int  flags)
{
    int ret, fds[2] = { 0, };

    ret = socketpair (PF_LOCAL, SOCK_STREAM | flags, 0, fds);
    if (ret == -1) {
        g_warning ("%s: failed to create socket pair with errno: %d",
                   __func__, errno);
        return ret;
    }
    *fd_a = fds [0];
    *fd_b = fds [1];
    return 0;
}
/* pretty print */
void
g_debug_tpma_cc (TPMA_CC tpma_cc)
{
    g_debug ("TPMA_CC: 0x%08" PRIx32, tpma_cc);
    g_debug ("  commandIndex: 0x%" PRIx16, (tpma_cc & TPMA_CC_COMMANDINDEX_MASK) >> TPMA_CC_COMMANDINDEX_SHIFT);
    g_debug ("  reserved1:    0x%" PRIx8, (tpma_cc & TPMA_CC_RESERVED1_MASK));
    g_debug ("  nv:           %s", prop_str (tpma_cc & TPMA_CC_NV));
    g_debug ("  extensive:    %s", prop_str (tpma_cc & TPMA_CC_EXTENSIVE));
    g_debug ("  flushed:      %s", prop_str (tpma_cc & TPMA_CC_FLUSHED));
    g_debug ("  cHandles:     0x%" PRIx8, (tpma_cc & TPMA_CC_CHANDLES_MASK) >> TPMA_CC_CHANDLES_SHIFT);
    g_debug ("  rHandle:      %s", prop_str (tpma_cc & TPMA_CC_RHANDLE));
    g_debug ("  V:            %s", prop_str (tpma_cc & TPMA_CC_V));
    g_debug ("  Res:          0x%" PRIx8, (tpma_cc & TPMA_CC_RES_MASK) >> TPMA_CC_RES_SHIFT);
}
/*
 * Parse the provided string containing a key / value pair separated by the
 * '=' character.
 * NOTE: The 'kv_str' parameter is not 'const' and this function will modify
 * it as part of the parsing process.
 */
gboolean
parse_key_value (char *key_value_str,
                 key_value_t *key_value)
{
    const char *delim = "=";
    char *tok, *state;

    tok = strtok_r (key_value_str, delim, &state);
    if (tok == NULL) {
        g_warning ("key / value string is null.");
        return FALSE;
    }
    key_value->key = tok;

    tok = strtok_r (NULL, delim, &state);
    if (tok == NULL) {
        g_warning ("key / value string is invalid");
        return FALSE;
    }
    key_value->value = tok;

    return TRUE;
}
/*
 * This function parses the provided configuration string extracting the
 * key/value pairs, and then passing the provided provided callback function
 * for processing.
 * NOTE: The 'kv_str' parameter is not 'const' and this function will modify
 * it as part of the parsing process.
 */
TSS2_RC
parse_key_value_string (char *kv_str,
                        KeyValueFunc callback,
                        gpointer user_data)
{
    const char *delim = ",";
    char *state, *tok;
    key_value_t key_value = { .key = 0, .value = 0 };
    gboolean ret;
    TSS2_RC rc = TSS2_RC_SUCCESS;

    for (tok = strtok_r (kv_str, delim, &state);
         tok;
         tok = strtok_r (NULL, delim, &state)) {
        ret = parse_key_value (tok, &key_value);
        if (ret != TRUE) {
            return TSS2_TCTI_RC_BAD_VALUE;
        }
        rc = callback (&key_value, user_data);
        if (rc != TSS2_RC_SUCCESS) {
            goto out;
        }
    }
out:
    return rc;
}
