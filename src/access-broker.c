/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 * All rights reserved.
 */
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include "tabrmd.h"

#include "access-broker.h"
#include "tcti.h"
#include "tpm2-command.h"
#include "tpm2-response.h"
#include "util.h"

G_DEFINE_TYPE (AccessBroker, access_broker, G_TYPE_OBJECT);

enum {
    PROP_0,
    PROP_SAPI_CTX,
    PROP_TCTI,
    N_PROPERTIES
};
static GParamSpec *obj_properties [N_PROPERTIES] = { NULL, };
/**
 * GObject property setter.
 */
static void
access_broker_set_property (GObject        *object,
                            guint           property_id,
                            GValue const   *value,
                            GParamSpec     *pspec)
{
    AccessBroker *self = ACCESS_BROKER (object);

    g_debug (__func__);
    switch (property_id) {
    case PROP_SAPI_CTX:
        self->sapi_context = g_value_get_pointer (value);
        break;
    case PROP_TCTI:
        self->tcti = g_value_get_object (value);
        g_object_ref (self->tcti);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}
/**
 * GObject property getter.
 */
static void
access_broker_get_property (GObject     *object,
                            guint        property_id,
                            GValue      *value,
                            GParamSpec  *pspec)
{
    AccessBroker *self = ACCESS_BROKER (object);

    g_debug (__func__);
    switch (property_id) {
    case PROP_SAPI_CTX:
        g_value_set_pointer (value, self->sapi_context);
        break;
    case PROP_TCTI:
        g_value_set_object (value, self->tcti);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}
/*
 * Dispose function: release references to gobjects. We also free the SAPI
 * context here as well. Typically freeing this data would be done in the
 * finalize function but it can't be used w/o the underlying TCTI.
 */
static void
access_broker_dispose (GObject *obj)
{
    AccessBroker *self = ACCESS_BROKER (obj);

    if (self->sapi_context != NULL) {
        Tss2_Sys_Finalize (self->sapi_context);
    }
    g_clear_pointer (&self->sapi_context, g_free);
    g_clear_object (&self->tcti);
    G_OBJECT_CLASS (access_broker_parent_class)->dispose (obj);
}
/*
 * G_DEFINE_TYPE requires an instance init even though we don't use it.
 */
static void
access_broker_init (AccessBroker *broker)
{
    UNUSED_PARAM(broker);
    /* noop */
}
/**
 * GObject class initialization function. This function boils down to:
 * - Setting up the parent class.
 * - Set dispose, property get/set.
 * - Install properties.
 */
static void
access_broker_class_init (AccessBrokerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    if (access_broker_parent_class == NULL)
        access_broker_parent_class = g_type_class_peek_parent (klass);
    object_class->dispose      = access_broker_dispose;
    object_class->get_property = access_broker_get_property;
    object_class->set_property = access_broker_set_property;

    obj_properties [PROP_SAPI_CTX] =
        g_param_spec_pointer ("sapi-ctx",
                              "SAPI context",
                              "TSS2_SYS_CONTEXT instance",
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
    obj_properties [PROP_TCTI] =
        g_param_spec_object ("tcti",
                             "Tcti object",
                             "Tcti for communication with TPM",
                             TYPE_TCTI,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
    g_object_class_install_properties (object_class,
                                       N_PROPERTIES,
                                       obj_properties);
}

#define SUPPORTED_ABI_VERSION \
{ \
    .tssCreator = 1, \
    .tssFamily = 2, \
    .tssLevel = 1, \
    .tssVersion = 108, \
}

TSS2_SYS_CONTEXT*
sapi_context_init (Tcti *tcti)
{
    TSS2_SYS_CONTEXT *sapi_context;
    TSS2_TCTI_CONTEXT *tcti_context;
    TSS2_RC rc;
    size_t size;
    TSS2_ABI_VERSION abi_version = SUPPORTED_ABI_VERSION;

    assert (tcti != NULL);
    tcti_context = tcti_peek_context (tcti);
    assert (tcti_context != NULL);

    size = Tss2_Sys_GetContextSize (0);
    g_debug ("Allocating 0x%zx bytes for SAPI context", size);
    /* NOTE: g_malloc0 will terminate the program if allocation fails */
    sapi_context = (TSS2_SYS_CONTEXT*)g_malloc0 (size);

    rc = Tss2_Sys_Initialize (sapi_context, size, tcti_context, &abi_version);
    if (rc != TSS2_RC_SUCCESS) {
        g_free (sapi_context);
        g_warning ("Failed to initialize SAPI context: 0x%x", rc);
        return NULL;
    }
    return sapi_context;
}

TSS2_RC
access_broker_send_tpm_startup (AccessBroker *broker)
{
    TSS2_RC rc;

    assert (broker != NULL);

    rc = Tss2_Sys_Startup (broker->sapi_context, TPM2_SU_CLEAR);
    if (rc != TSS2_RC_SUCCESS && rc != TPM2_RC_INITIALIZE)
        g_warning ("Tss2_Sys_Startup returned unexpected RC: 0x%" PRIx32, rc);
    else
        rc = TSS2_RC_SUCCESS;

    return rc;
}
/**
 * This is a very thin wrapper around the mutex mediating access to the
 * TSS2_SYS_CONTEXT. It locks the mutex.
 *
 * NOTE: Any errors locking the mutex are fatal and will cause the program
 * to halt.
 */
void
access_broker_lock (AccessBroker *broker)
{
    gint error;

    assert (broker != NULL);

    error = pthread_mutex_lock (&broker->sapi_mutex);
    if (error != 0) {
        switch (error) {
        case EINVAL:
            g_error ("AccessBroker: attempted to lock uninitialized mutex");
            break;
        default:
            g_error ("AccessBroker: unknown error attempting to lock SAPI "
                     "mutex: 0x%x", error);
            break;
        }
    }
}
/**
 * This is a very thin wrapper around the mutex mediating access to the
 * TSS2_SYS_CONTEXT. It unlocks the mutex.
 *
 * NOTE: Any errors locking the mutex are fatal and will cause the program
 * to halt.
 */
void
access_broker_unlock (AccessBroker *broker)
{
    gint error;

    assert (broker != NULL);

    error= pthread_mutex_unlock (&broker->sapi_mutex);
    if (error != 0) {
        switch (error) {
        case EINVAL:
            g_error ("AccessBroker: attempted to unlock uninitialized mutex");
            break;
        default:
            g_error ("AccessBroker: unknown error attempting to unlock SAPI "
                     "mutex: 0x%x", error);
            break;
        }
    }
}
/**
 * Query the TPM for fixed (TPM2_PT_FIXED) TPM properties.
 * This function is intended for internal use only. The caller MUST
 * hold the sapi_mutex lock before calling.
 */
TSS2_RC
access_broker_get_tpm_properties_fixed (TSS2_SYS_CONTEXT     *sapi_context,
                                        TPMS_CAPABILITY_DATA *capability_data)
{
    TSS2_RC          rc;
    TPMI_YES_NO      more_data;

    assert (sapi_context != NULL);
    assert (capability_data != NULL);

    g_debug ("access_broker_get_tpm_properties_fixed");
    rc = Tss2_Sys_GetCapability (sapi_context,
                                 NULL,
                                 TPM2_CAP_TPM_PROPERTIES,
                                 /* get TPM2_PT_FIXED TPM property group */
                                 TPM2_PT_FIXED,
                                 /* get all properties in the group */
                                 TPM2_MAX_TPM_PROPERTIES,
                                 &more_data,
                                 capability_data,
                                 NULL);
    if (rc != TSS2_RC_SUCCESS) {
        g_warning ("Failed to GetCapability: TPM2_CAP_TPM_PROPERTIES, "
                 "TPM2_PT_FIXED: 0x%x", rc);
        return rc;
    }
    /* sanity check the property returned */
    if (capability_data->capability != TPM2_CAP_TPM_PROPERTIES)
        g_warning ("GetCapability returned wrong capability: 0x%x",
                   capability_data->capability);
    return rc;
}
/**
 * Query the TM for a specific tagged property from the collection of
 * fixed TPM properties. If the requested property is found then the
 * 'value' parameter will be set accordingly. If no such property exists
 * then TSS2_TABRMD_BAD_VALUE will be returned.
 */
TSS2_RC
access_broker_get_fixed_property (AccessBroker           *broker,
                                  TPM2_PT                  property,
                                  guint32                *value)
{
    unsigned int i;

    assert (broker != NULL);
    assert (value != NULL);

    if (broker->properties_fixed.data.tpmProperties.count == 0) {
        return TSS2_RESMGR_RC_INTERNAL_ERROR;
    }
    for (i = 0; i < broker->properties_fixed.data.tpmProperties.count; ++i) {
        if (broker->properties_fixed.data.tpmProperties.tpmProperty[i].property == property) {
            *value = broker->properties_fixed.data.tpmProperties.tpmProperty[i].value;
            return TSS2_RC_SUCCESS;
        }
    }
    return TSS2_RESMGR_RC_BAD_VALUE;
}
/*
 * This function exposes the underlying SAPI context in the AccessBroker.
 * It locks the AccessBroker object and returns the SAPI context for use
 * by the caller. Do not call this function if you already hold the
 * AccessBroker lock. If you do you'll deadlock.
 * When done with the context the caller must unlock the AccessBroker.
 */
TSS2_SYS_CONTEXT*
access_broker_lock_sapi (AccessBroker *broker)
{
    assert (broker != NULL);
    assert (broker->sapi_context != NULL);

    access_broker_lock (broker);

    return broker->sapi_context;
}
/*
 * Return the TPM2_PT_TPM2_MAX_COMMAND_SIZE fixed TPM property.
 */
TSS2_RC
access_broker_get_max_command (AccessBroker *broker,
                               guint32      *value)
{
    return access_broker_get_fixed_property (broker,
                                             TPM2_PT_MAX_COMMAND_SIZE,
                                             value);
}
/**
 * Return the TPM2_PT_TPM2_MAX_RESPONSE_SIZE fixed TPM property.
 */
TSS2_RC
access_broker_get_max_response (AccessBroker *broker,
                                guint32      *value)
{
    return access_broker_get_fixed_property (broker,
                                             TPM2_PT_MAX_RESPONSE_SIZE,
                                             value);
}
/* Send the parameter Tpm2Command to the TPM. Return the TSS2_RC. */
static TSS2_RC
access_broker_send_cmd (AccessBroker *broker,
                        Tpm2Command  *command)
{
    TSS2_RC rc;

    rc = tcti_transmit (broker->tcti,
                        tpm2_command_get_size (command),
                        tpm2_command_get_buffer (command));
    if (rc != TSS2_RC_SUCCESS)
        g_warning ("%s: AccessBroker failed to transmit Tpm2Command: 0x%"
                   PRIx32, __func__, rc);
    return rc;
}
/*
 * Get a response buffer from the TPM. Return the TSS2_RC through the
 * 'rc' parameter. Returns a buffer (that must be freed by the caller)
 * containing the response from the TPM. Determine the size of the buffer
 * by reading the size field from the TPM command header.
 */
static TSS2_RC
access_broker_get_response (AccessBroker *broker,
                            uint8_t     **buffer,
                            size_t       *buffer_size)
{
    TSS2_RC rc;
    guint32 max_size;

    assert (broker != NULL);
    assert (buffer != NULL);
    assert (buffer_size != NULL);

    rc = access_broker_get_max_response (broker, &max_size);
    if (rc != TSS2_RC_SUCCESS)
        return rc;

    *buffer = calloc (1, max_size);
    if (*buffer == NULL) {
        g_warning ("failed to allocate buffer for Tpm2Response: %s",
                   strerror (errno));
        return RM_RC (TPM2_RC_MEMORY);
    }
    *buffer_size = max_size;
    rc = tcti_receive (broker->tcti, buffer_size, *buffer, TSS2_TCTI_TIMEOUT_BLOCK);
    if (rc != TSS2_RC_SUCCESS) {
        g_warning ("%s: tcti_receive failed with RC 0x%" PRIx32, __func__, rc);
        free (*buffer);
        return rc;
    }
    *buffer = realloc (*buffer, *buffer_size);

    return rc;
}
/**
 * In the most simple case the caller will want to send just a single
 * command represented by a Tpm2Command object. The response is passed
 * back as the return value. The response code from the TCTI (not the TPM)
 * is returned through the 'rc' out parameter.
 * The caller MUST NOT hold the lock when calling. This function will take
 * the lock for itself.
 * Additionally this function *WILL ONLY* return a NULL Tpm2Response
 * pointer if it's unable to allocate memory for the object. In all other
 * error cases this function will create a Tpm2Response object with the
 * appropriate RC populated.
 */
Tpm2Response*
access_broker_send_command (AccessBroker  *broker,
                            Tpm2Command   *command,
                            TSS2_RC       *rc)
{
    Tpm2Response   *response = NULL;
    Connection     *connection = NULL;
    guint8         *buffer = NULL;
    size_t          buffer_size = 0;

    g_debug (__func__);
    assert (broker != NULL);
    assert (command != NULL);
    assert (rc != NULL);

    access_broker_lock (broker);
    *rc = access_broker_send_cmd (broker, command);
    if (*rc != TSS2_RC_SUCCESS)
        goto unlock_out;
    *rc = access_broker_get_response (broker, &buffer, &buffer_size);
    if (*rc != TSS2_RC_SUCCESS) {
        goto unlock_out;
    }
    access_broker_unlock (broker);
    connection = tpm2_command_get_connection (command);
    response = tpm2_response_new (connection,
                                  buffer,
                                  buffer_size,
                                  tpm2_command_get_attributes (command));
    g_clear_object (&connection);
    return response;

unlock_out:
    access_broker_unlock (broker);
    if (!connection)
        connection = tpm2_command_get_connection (command);
    response = tpm2_response_new_rc (connection, *rc);
    g_object_unref (connection);
    return response;
}
/**
 * Create new TPM access broker (ACCESS_BROKER) object. This includes
 * using the provided TCTI to send the TPM the startup command and
 * creating the TCTI mutex.
 */
AccessBroker*
access_broker_new (Tcti *tcti)
{
    AccessBroker       *broker;
    TSS2_SYS_CONTEXT   *sapi_context;

    assert (tcti != NULL);

    sapi_context = sapi_context_init (tcti);
    broker = ACCESS_BROKER (g_object_new (TYPE_ACCESS_BROKER,
                                          "sapi-ctx", sapi_context,
                                          "tcti", tcti,
                                          NULL));
    return broker;
}
/*
 * Initialize the AccessBroker. This is all about initializing internal data
 * that normally we would want to do in a constructor. But since this
 * initialization requires reaching out to the TPM and could fail we don't
 * want to do it in the constructor / _new function. So we put it here in
 * an explicit _init function that must be executed after the object has
 * been instantiated.
 */
TSS2_RC
access_broker_init_tpm (AccessBroker *broker)
{
    TSS2_RC rc;

    g_debug (__func__);
    assert (broker != NULL);

    if (broker->initialized)
        return TSS2_RC_SUCCESS;
    pthread_mutex_init (&broker->sapi_mutex, NULL);
    rc = access_broker_send_tpm_startup (broker);
    if (rc != TSS2_RC_SUCCESS)
        goto out;
    rc = access_broker_get_tpm_properties_fixed (broker->sapi_context,
                                                 &broker->properties_fixed);
    if (rc != TSS2_RC_SUCCESS)
        goto out;
    broker->initialized = true;
out:
    return rc;
}
/*
 * Query the TPM for the current number of loaded transient objects.
 */
 TSS2_RC
access_broker_get_trans_object_count (AccessBroker *broker,
                                      uint32_t     *count)
{
    TSS2_RC rc = TSS2_RC_SUCCESS;
    TSS2_SYS_CONTEXT *sapi_context;
    TPMI_YES_NO more_data;
    TPMS_CAPABILITY_DATA capability_data = { 0, };

    assert (broker != NULL);
    assert (count != NULL);

    sapi_context = access_broker_lock_sapi (broker);
    /*
     * GCC gets confused by the TPM2_TRANSIENT_FIRST constant being used for
     * the 4th parameter. It assumes that it's a signed type which causes
     * -Wsign-conversion to complain. Casting to UINT32 is all we can do.
     */
    rc = Tss2_Sys_GetCapability (sapi_context,
                                 NULL,
                                 TPM2_CAP_HANDLES,
                                 (UINT32)TPM2_TRANSIENT_FIRST,
                                 TPM2_TRANSIENT_LAST - TPM2_TRANSIENT_FIRST,
                                 &more_data,
                                 &capability_data,
                                 NULL);
    if (rc != TSS2_RC_SUCCESS) {
        g_warning ("%s: Tss2_Sys_GetCapability failed with RC 0x%" PRIx32,
                   __func__, rc);
        goto out;
    }
    *count = capability_data.data.handles.count;
out:
    access_broker_unlock (broker);
    return rc;
}
TSS2_RC
access_broker_context_load (AccessBroker *broker,
                            TPMS_CONTEXT *context,
                            TPM2_HANDLE   *handle)
{
    TSS2_RC           rc;
    TSS2_SYS_CONTEXT *sapi_context;

    assert (broker == NULL);
    assert (context == NULL);
    assert (handle == NULL);

    sapi_context = access_broker_lock_sapi (broker);
    rc = Tss2_Sys_ContextLoad (sapi_context, context, handle);
    access_broker_unlock (broker);
    if (rc == TSS2_RC_SUCCESS) {
        g_debug ("%s: successfully load context, got handle 0x%" PRIx32,
                 __func__, *handle);
    } else {
        g_warning ("%s: failed to load context, TSS2_RC: 0x%" PRIx32,
                   __func__, rc);
    }

    return rc;
}
/*
 * This function is a simple wrapper around the TPM2_ContextSave command.
 * It will save the context associated with the provided handle, returning
 * the TPMS_CONTEXT to the caller. The response code returned will be
 * TSS2_RC_SUCCESS or an RC indicating failure from the TPM.
 */
TSS2_RC
access_broker_context_save (AccessBroker *broker,
                            TPM2_HANDLE    handle,
                            TPMS_CONTEXT *context)
{
    TSS2_RC rc;
    TSS2_SYS_CONTEXT *sapi_context;

    assert (broker == NULL);
    assert (context == NULL);

    g_debug ("access_broker_context_save: handle 0x%08" PRIx32, handle);
    sapi_context = access_broker_lock_sapi (broker);
    rc = Tss2_Sys_ContextSave (sapi_context, handle, context);
    if (rc != TSS2_RC_SUCCESS) {
        g_warning ("%s returned an error: 0x%" PRIx32, __func__, rc);
    }
    access_broker_unlock (broker);

    return rc;
}
/*
 * This function is a simple wrapper around the TPM2_FlushContext command.
 */
TSS2_RC
access_broker_context_flush (AccessBroker *broker,
                             TPM2_HANDLE    handle)
{
    TSS2_RC rc;
    TSS2_SYS_CONTEXT *sapi_context;

    assert (broker == NULL);

    g_debug ("access_broker_context_flush: handle 0x%08" PRIx32, handle);
    sapi_context = access_broker_lock_sapi (broker);
    rc = Tss2_Sys_FlushContext (sapi_context, handle);
    if (rc != TSS2_RC_SUCCESS) {
        g_warning ("Failed to flush context for handle 0x%08" PRIx32
                   " RC: 0x%" PRIx32, handle, rc);
    }
    access_broker_unlock (broker);

    return rc;
}
TSS2_RC
access_broker_context_saveflush (AccessBroker *broker,
                                 TPM2_HANDLE    handle,
                                 TPMS_CONTEXT *context)
{
    TSS2_RC           rc;
    TSS2_SYS_CONTEXT *sapi_context;

    assert (broker == NULL);
    assert (context == NULL);

    g_debug ("access_broker_context_saveflush: handle 0x%" PRIx32, handle);
    sapi_context = access_broker_lock_sapi (broker);
    rc = Tss2_Sys_ContextSave (sapi_context, handle, context);
    if (rc != TSS2_RC_SUCCESS) {
        g_warning ("%s: Tss2_Sys_ContextSave failed to save context for "
                   "handle: 0x%" PRIx32 " TSS2_RC: 0x%" PRIx32, __func__,
                   handle, rc);
        goto out;
    }
    g_debug ("access_broker_context_saveflush: handle 0x%" PRIx32, handle);
    rc = Tss2_Sys_FlushContext (sapi_context, handle);
    if (rc != TSS2_RC_SUCCESS) {
        g_warning("%s: Tss2_Sys_FlushContext failed for handle: 0x%" PRIx32
                  ", TSS2_RC: 0x%" PRIx32, __func__, handle, rc);
    }
out:
    access_broker_unlock (broker);
    return rc;
}
/*
 * Flush all handles in a given range. This function will return an error if
 * we're unable to query for handles within the requested range. Failures to
 * flush handles returned will be ignored since our goal here is to flush as
 * many as possible.
 */
TSS2_RC
access_broker_flush_all_unlocked (AccessBroker     *broker,
                                  TSS2_SYS_CONTEXT *sapi_context,
                                  TPM2_RH            first,
                                  TPM2_RH            last)
{
    TSS2_RC rc = TSS2_RC_SUCCESS;
    TPMI_YES_NO more_data;
    TPMS_CAPABILITY_DATA capability_data = { 0, };
    TPM2_HANDLE handle;
    size_t i;

    g_debug ("%s: first: 0x%08" PRIx32 ", last: 0x%08" PRIx32,
             __func__, first, last);
    assert (broker != NULL);
    assert (sapi_context != NULL);

    rc = Tss2_Sys_GetCapability (sapi_context,
                                 NULL,
                                 TPM2_CAP_HANDLES,
                                 first,
                                 last - first,
                                 &more_data,
                                 &capability_data,
                                 NULL);
    if (rc != TSS2_RC_SUCCESS) {
        g_warning ("Failed to get capability TPM2_CAP_HANDLES");
        return rc;
    }
    g_debug ("%s: got %u handles", __func__, capability_data.data.handles.count);
    for (i = 0; i < capability_data.data.handles.count; ++i) {
        handle = capability_data.data.handles.handle [i];
        g_debug ("%s: flushing context with handle: 0x%08" PRIx32, __func__,
                 handle);
        rc = Tss2_Sys_FlushContext (sapi_context, handle);
        if (rc != TSS2_RC_SUCCESS) {
            g_warning ("Failed to flush context for handle 0x%08" PRIx32
                       " RC: 0x%" PRIx32, handle, rc);
        }
    }

    return TSS2_RC_SUCCESS;
}
void
access_broker_flush_all_context (AccessBroker *broker)
{
    TSS2_SYS_CONTEXT *sapi_context;

    g_debug (__func__);
    assert (broker != NULL);

    sapi_context = access_broker_lock_sapi (broker);
    access_broker_flush_all_unlocked (broker,
                                      sapi_context,
                                      TPM2_ACTIVE_SESSION_FIRST,
                                      TPM2_ACTIVE_SESSION_LAST);
    access_broker_flush_all_unlocked (broker,
                                      sapi_context,
                                      TPM2_LOADED_SESSION_FIRST,
                                      TPM2_LOADED_SESSION_LAST);
    access_broker_flush_all_unlocked (broker,
                                      sapi_context,
                                      TPM2_TRANSIENT_FIRST,
                                      TPM2_TRANSIENT_LAST);
    access_broker_unlock (broker);
}
