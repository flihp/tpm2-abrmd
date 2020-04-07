/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef TPM2_HEADER_H
#define TPM2_HEADER_H

#include <glib-object.h>
#include <tss2/tss2_tpm2_types.h>

/* A convenience macro to get us the size of the TPM header. */
#define TPM_HEADER_SIZE (UINT32)(sizeof (TPM2_ST) + sizeof (UINT32) + sizeof (TPM2_CC))

G_BEGIN_DECLS

typedef struct _Tpm2HeaderClass {
    GObjectClass parent;
} Tpm2HeaderClass;

typedef struct _Tpm2Header {
    GObject parent_instance;
    TPM2_ST tag;
    UINT32 size;
    UINT32 code;
} Tpm2Header;

#define TYPE_TPM2_HEADER            (tpm2_header_get_type      ())
#define TPM2_HEADER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj),   TYPE_TPM2_HEADER, Tpm2Header))
#define TPM2_HEADER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST    ((klass), TYPE_TPM2_HEADER, Tpm2HeaderClass))
#define IS_TPM2_HEADER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj),   TYPE_TPM2_HEADER))
#define IS_TPM2_HEADER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE    ((klass), TYPE_TPM2_HEADER))
#define TPM2_HEADER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS  ((obj),   TYPE_TPM2_HEADER, Tpm2HeaderClass))

GType tpm2_header_get_type (void);
Tpm2Header* tpm2_header_new (TPM2_ST tag, UINT32 size, UINT32 code);
/* may return NULL if buffer fails to parse
 * buf / size *must* be at least TPM_HEADER_SIZE */
Tpm2Header* tpm2_header_new_from_buffer (uint8_t* buf,
                                         size_t size,
                                         TSS2_RC* rc);
TSS2_RC tpm2_header_marshal (Tpm2Header* hdr, uint8_t* buf, size_t size);

TPM2_ST tpm2_header_get_tag (Tpm2Header* hdr);
UINT32 tpm2_header_get_size (Tpm2Header* hdr);
TSS2_RC tpm2_header_get_response_code (Tpm2Header* hdr);
TPM2_CC tpm2_header_get_command_code (Tpm2Header* hdr);

G_END_DECLS

#endif /* TPM2_HEADER_H */
