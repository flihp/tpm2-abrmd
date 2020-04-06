/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 * All rights reserved.
 */
#ifndef TPM2_HEADER_H
#define TPM2_HEADER_H

#include <sys/types.h>
#include <tss2/tss2_tcti.h>

/* A convenience macro to get us the size of the TPM header. */
#define TPM_HEADER_SIZE (UINT32)(sizeof (TPM2_ST) + sizeof (UINT32) + sizeof (TPM2_CC))

/*
 * A generic tpm header structure, could be command or response.
 * NOTE: Do not expect sizeof (tpm2_header_t) to get your the size of the
 * header. The compiler pads this structure. Use the macro above.
 */
typedef struct {
    TPM2_ST   tag;
    UINT32   size;
    UINT32   code;
} tpm2_header_t;

TPM2_ST tpm2_header_get_tag (uint8_t* hdr);
void tpm2_header_set_tag (uint8_t* hdr,
                         TPM2_ST tag);
UINT32 tpm2_header_get_size (uint8_t* hdr);
void tpm2_header_set_size (uint8_t* hdr,
                          UINT32 size);
TSS2_RC tpm2_header_get_response_code (uint8_t* hdr);
void tpm2_header_set_response_code (uint8_t* hdr, TSS2_RC rc);
TPM2_CC tpm2_header_get_command_code (uint8_t* hdr);
void tpm2_header_set_command_code (uint8_t *hdr, TPM2_ST cc);
TSS2_RC tpm2_header_init (uint8_t* buf,
                          size_t buf_size,
                          TPM2_ST tag,
                          UINT32 size,
                          TSS2_RC code);

#endif /* TPM2_HEADER_H */
