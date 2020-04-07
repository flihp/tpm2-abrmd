/* SPDX-License-Identifier: BSD-2-Clause */
#include <assert.h>
#include <glib-object.h>

#include <tss2/tss2_mu.h>
#include <tss2/tss2_tpm2_types.h>

#include "tpm2-header.h"
#include "util.h"

G_DEFINE_TYPE (Tpm2Header, tpm2_header, G_TYPE_OBJECT);

enum {
    PROP_0,
    PROP_TAG,
    PROP_SIZE,
    PROP_CODE,
    N_PROPERTIES
};
static GParamSpec *obj_properties [N_PROPERTIES] = { NULL, };

static void
tpm2_header_get_property (GObject* object,
                          guint property_id,
                          GValue* value,
                          GParamSpec* pspec)
{
    Tpm2Header* self = TPM2_HEADER (object);

    switch (property_id) {
    case PROP_TAG:
        g_value_set_uint (value, self->tag);
        break;
    case PROP_SIZE:
        g_value_set_uint (value, self->size);
        break;
    case PROP_CODE:
        g_value_set_uint (value, self->code);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}
static void
tpm2_header_set_property (GObject* object,
                          guint property_id,
                          GValue const* value,
                          GParamSpec* pspec)
{
    Tpm2Header* self = TPM2_HEADER (object);

    switch (property_id) {
    case PROP_TAG:
        self->tag = g_value_get_uint (value);
        break;
    case PROP_SIZE:
        self->size = g_value_get_uint (value);
        break;
    case PROP_CODE:
        self->code = g_value_get_uint (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}
/* are these necessary if we don't dispose or finalize anything? */
static void
tpm2_header_dispose (GObject *obj)
{
    G_OBJECT_CLASS (tpm2_header_parent_class)->dispose (obj);
}
static void
tpm2_header_finalize (GObject *obj)
{
    G_OBJECT_CLASS (tpm2_header_parent_class)->finalize(obj);
}
static void
tpm2_header_init (Tpm2Header* hdr)
{
    UNUSED_PARAM (hdr);
    /* noop */
}
static void
tpm2_header_class_init (Tpm2HeaderClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    if (tpm2_header_parent_class == NULL)
        tpm2_header_parent_class = g_type_class_peek_parent (klass);
    object_class->dispose      = tpm2_header_dispose;
    object_class->finalize     = tpm2_header_finalize;
    object_class->get_property = tpm2_header_get_property;
    object_class->set_property = tpm2_header_set_property;

    obj_properties [PROP_TAG] =
        g_param_spec_uint ("tag",
                           "TPM2_ST",
                           "TPM command / response buffer tag",
                           0,
                           UINT16_MAX,
                           TPM2_ST_NO_SESSIONS,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

    UINT32 max_size;
    if (TPM2_MAX_COMMAND_SIZE >= TPM2_MAX_RESPONSE_SIZE)
        max_size = TPM2_MAX_COMMAND_SIZE;
    else
        max_size = TPM2_MAX_RESPONSE_SIZE;

    obj_properties [PROP_SIZE] =
        g_param_spec_uint ("size",
                           "UINT32",
                           "TPM command / response buffer size",
                           0,
                           UINT32_MAX,
                           max_size,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
    obj_properties [PROP_CODE] =
        g_param_spec_uint ("code",
                           "UINT32",
                           "TPM command / response code",
                           0,
                           UINT32_MAX,
                           0,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
    g_object_class_install_properties (object_class,
                                       N_PROPERTIES,
                                       obj_properties);
}
Tpm2Header*
tpm2_header_new (uint16_t tag, uint32_t size, uint32_t code)
{
    return TPM2_HEADER (g_object_new (TYPE_TPM2_HEADER,
                                      "tag", tag,
                                      "size", size,
                                      "code", code,
                                      NULL));
}
/*
 * Cheaper than a factory object.
 */
Tpm2Header*
tpm2_header_new_from_buffer (uint8_t* buf, size_t buf_size, TSS2_RC* rc)
{
    assert (buf != NULL);
    assert (size < TPM_HEADER_SIZE);
    assert (rc);

    size_t offset = 0;
    TPM2_ST tag = 0;
    UINT32 size = 0;
    UINT32 code = 0;

    *rc = Tss2_MU_TPM2_ST_Unmarshal (buf, buf_size, &offset, &tag);
    if (*rc) {
        RC_WARN ("Tss2_MU_TPM2_ST_Unmarshal", *rc);
        return NULL;
    }

    *rc = Tss2_MU_UINT32_Unmarshal (buf, buf_size, &offset, &size);
    if (*rc) {
        RC_WARN ("Tss2_MU_UINT32_Unmarshal", *rc);
        return NULL;
    }

    *rc = Tss2_MU_UINT32_Unmarshal (buf, buf_size, &offset, &code);
    if (*rc) {
        RC_WARN ("Tss2_MU_UINT32_Unmarshal", *rc);
        return NULL;
    }

    return tpm2_header_new (tag, size, code);
}
TSS2_RC
tpm2_header_marshal (Tpm2Header* hdr, uint8_t* buf, size_t size)
{
    assert (buf != NULL);
    assert (size >= TPM_HEADER_SIZE);

    size_t offset = 0;
    TSS2_RC rc;

    rc = Tss2_MU_TPM2_ST_Marshal (hdr->tag, buf, size, &offset);
    if (rc) {
        RC_WARN ("Tss2_MU_TPM2_ST_Marshal", rc);
        return rc;
    }

    rc = Tss2_MU_UINT32_Marshal (hdr->size, buf, size, &offset);
    if (rc) {
        RC_WARN ("Tss2_MU_UINT32_Marshal", rc);
        return rc;
    }

    rc = Tss2_MU_UINT32_Marshal (hdr->code, buf, size, &offset);
    if (rc) {
        RC_WARN ("Tss2_MU_UINT32_Marshal", rc);
    }

    return rc;
}
TPM2_ST
tpm2_header_get_tag (Tpm2Header* hdr)
{
    assert (hdr);
    return hdr->tag;
}
UINT32
tpm2_header_get_size (Tpm2Header* hdr)
{
    assert (hdr);
    return hdr->size;
}
TPM2_CC
tpm2_header_get_command_code (Tpm2Header* hdr)
{
    assert (hdr);
    return hdr->code;
}
TSS2_RC
tpm2_header_get_response_code (Tpm2Header* hdr)
{
    assert (hdr);
    return hdr->code;
}
