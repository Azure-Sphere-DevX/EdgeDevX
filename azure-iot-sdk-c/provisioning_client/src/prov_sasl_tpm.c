// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <string.h>
#include "azure_c_shared_utility/optimize_size.h"
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/crt_abstractions.h"
#include "azure_c_shared_utility/strings.h"

#include "azure_prov_client/prov_transport.h"
#include "azure_prov_client/internal/prov_sasl_tpm.h"
#include "azure_uamqp_c/sasl_mechanism.h"

typedef struct SASL_TPM_INSTANCE_TAG
{
    BUFFER_HANDLE endorsement_key;
    BUFFER_HANDLE storage_root_key;
    char* registration_id;
    char* hostname;
    char* scope_id;
    TPM_CHALLENGE_CALLBACK challenge_cb;
    void* user_ctx;
    unsigned char ctrl_seq_num;

    unsigned char* data_buffer;
    size_t data_offset;

    BUFFER_HANDLE nonce_handle;
} SASL_TPM_INSTANCE;

typedef enum CTRL_BYTE_STAGE_TAG
{
    CTRL_BYTE_STAGE_CONTINUE = 0x0,
    CTRL_BYTE_STAGE_LAST_SEQUENCE = 0x40
} CTRL_BYTE_STAGE;

typedef enum CTRL_BYTE_TYPE_TAG
{
    CTRL_BYTE_TYPE_EK = 0x00,
    CTRL_BYTE_TYPE_SRK = 0x10
} CTRL_BYTE_TYPE;

#define VALID_CTRL_BYTE     0x80
#define SEQUENCE_BYTE       0x3
#define MAX_FRAME_DATA_LEN  470

static bool is_last_sequence(unsigned char ctrl_byte)
{
    return (ctrl_byte & CTRL_BYTE_STAGE_LAST_SEQUENCE) ? true : false;
}

static bool is_valid_ctrl_byte(unsigned char ctrl_byte)
{
    return (ctrl_byte & VALID_CTRL_BYTE) ? true : false;
}

static unsigned char get_seq_byte(unsigned char ctrl_byte)
{
    return ctrl_byte & SEQUENCE_BYTE;
}

static unsigned char construct_prov_ctrl_byte(CTRL_BYTE_STAGE stage, CTRL_BYTE_TYPE type, unsigned char seq_num)
{
    unsigned char result = 0;
    result |= VALID_CTRL_BYTE | stage | type | seq_num;
    return result;
}

static bool validate_sequence_num(SASL_TPM_INSTANCE* sasl_tpm_info, unsigned char ctrl_byte)
{
    bool result;
    unsigned char seq_num = get_seq_byte(ctrl_byte);
    if (sasl_tpm_info->ctrl_seq_num != seq_num)
    {
        result = false;
    }
    else
    {
        result = true;
    }
    return result;
}

static int construct_send_data(SASL_TPM_INSTANCE* sasl_tpm_info, const char* scope_id, const char* reg_id, unsigned char* send_data, size_t data_len, uint32_t* send_len)
{
    int result;
    uint32_t payload_data_length;
    size_t scope_id_len = 0;
    size_t registration_id_len = 0;
    size_t buff_offset = 0;

    if (scope_id != NULL)
    {
        scope_id_len = strlen(scope_id) + 1;
    }
    if (reg_id != NULL)
    {
        registration_id_len = strlen(reg_id) + 1;
    }

    if ((data_len + scope_id_len + registration_id_len) > MAX_FRAME_DATA_LEN)
    {
        CTRL_BYTE_STAGE curr_stage;
        if ((data_len + scope_id_len + registration_id_len) - sasl_tpm_info->data_offset <= MAX_FRAME_DATA_LEN)
        {
            curr_stage = CTRL_BYTE_STAGE_LAST_SEQUENCE;
            payload_data_length = (uint32_t)(data_len + scope_id_len + registration_id_len + 1) - (uint32_t)sasl_tpm_info->data_offset;

        }
        else
        {
            curr_stage = CTRL_BYTE_STAGE_CONTINUE;
            payload_data_length = MAX_FRAME_DATA_LEN;
        }

        sasl_tpm_info->data_buffer = (unsigned char*)malloc((size_t)payload_data_length+1);
        if (sasl_tpm_info->data_buffer == NULL)
        {
            LogError("Failed allocating initial data");
            result = MU_FAILURE;
        }
        else
        {
            unsigned char control_byte = construct_prov_ctrl_byte(curr_stage, CTRL_BYTE_TYPE_EK, sasl_tpm_info->ctrl_seq_num);
            sasl_tpm_info->data_buffer[0] = control_byte;
            buff_offset++;
            if (scope_id != NULL && reg_id != NULL)
            {
                if (sasl_tpm_info->data_offset < (scope_id_len + registration_id_len))
                {
                    (void)memcpy(sasl_tpm_info->data_buffer + buff_offset + sasl_tpm_info->data_offset, scope_id, scope_id_len - sasl_tpm_info->data_offset);
                    buff_offset += scope_id_len;

                    (void)memcpy(sasl_tpm_info->data_buffer + buff_offset, reg_id, registration_id_len);
                    buff_offset += registration_id_len;
                }
                else
                {
                    scope_id_len  = registration_id_len = 0;
                }
            }

            (void)memcpy(sasl_tpm_info->data_buffer + buff_offset, send_data + sasl_tpm_info->data_offset, payload_data_length-buff_offset);

            //(void)memcpy(sasl_tpm_info->data_buffer + 1, send_data + sasl_tpm_info->data_offset, payload_data_length);
            sasl_tpm_info->data_offset += payload_data_length - buff_offset;
            sasl_tpm_info->ctrl_seq_num++;
            *send_len = payload_data_length;
            result = 0;
        }
    }
    else
    {
        payload_data_length = (uint32_t)(data_len + scope_id_len + registration_id_len + 1);
        sasl_tpm_info->data_buffer = (unsigned char*)malloc((size_t)payload_data_length+1);
        if (sasl_tpm_info->data_buffer == NULL)
        {
            LogError("Failed allocating initial data");
            result = MU_FAILURE;
        }
        else
        {
            unsigned char control_byte = 0;
            sasl_tpm_info->data_buffer[0] = control_byte;
            buff_offset++;
            if (scope_id != NULL || reg_id != NULL)
            {
                (void)memcpy(sasl_tpm_info->data_buffer + buff_offset, scope_id, scope_id_len);
                buff_offset += scope_id_len;
                (void)memcpy(sasl_tpm_info->data_buffer + buff_offset, reg_id, registration_id_len);
                buff_offset += registration_id_len;
            }
            (void)memcpy(sasl_tpm_info->data_buffer + buff_offset, send_data, data_len);
            *send_len = payload_data_length;
            result = 0;
        }
    }
    return result;
}

static void clear_security_buffers(BUFFER_HANDLE handle)
{
    unsigned char* sec_buff = BUFFER_u_char(handle);
    size_t buff_length = BUFFER_length(handle);
    (void)memset(sec_buff, 0, buff_length);
    BUFFER_delete(handle);
}

static void cleanup_sasl_data(SASL_TPM_INSTANCE* sasl_tpm_info)
{
    clear_security_buffers(sasl_tpm_info->endorsement_key);
    clear_security_buffers(sasl_tpm_info->storage_root_key);
    BUFFER_delete(sasl_tpm_info->nonce_handle);
    free(sasl_tpm_info->hostname);
    free(sasl_tpm_info->registration_id);
    free(sasl_tpm_info->scope_id);
    free(sasl_tpm_info->data_buffer);
    free(sasl_tpm_info);
}

static CONCRETE_SASL_MECHANISM_HANDLE prov_sasltpm_create(void* config)
{
    SASL_TPM_INSTANCE* result;
    if (config == NULL)
    {
        LogError("Bad argument config is NULL");
        result = NULL;
    }
    else
    {
        const SASL_TPM_CONFIG_INFO* config_info = (SASL_TPM_CONFIG_INFO*)config;
        if (config_info->challenge_cb == NULL)
        {
            LogError("challenge callback is NULL.");
            result = NULL;
        }
        else
        {
            result = (SASL_TPM_INSTANCE*)malloc(sizeof(SASL_TPM_INSTANCE));
            if (result == NULL)
            {
                LogError("Cannot allocate memory for SASL anonymous instance");
            }
            else
            {
                (void)memset(result, 0, sizeof(SASL_TPM_INSTANCE));
                result->challenge_cb = config_info->challenge_cb;
                result->user_ctx = config_info->user_ctx;
                if (config_info->endorsement_key == NULL || (result->endorsement_key = BUFFER_clone(config_info->endorsement_key)) == NULL)
                {
                    LogError("Failure copying endorsement key");
                    free(result);
                    result = NULL;
                }
                else if (config_info->storage_root_key == NULL || (result->storage_root_key = BUFFER_clone(config_info->storage_root_key)) == NULL)
                {
                    LogError("Failure copying storage root key");
                    cleanup_sasl_data(result);
                    result = NULL;
                }
                else if (config_info->hostname == NULL || mallocAndStrcpy_s(&result->hostname, config_info->hostname) != 0)
                {
                    LogError("Failure copying hostname");
                    cleanup_sasl_data(result);
                    result = NULL;
                }
                else if (config_info->registration_id == NULL || mallocAndStrcpy_s(&result->registration_id, config_info->registration_id) != 0)
                {
                    LogError("Failure copying registration_id");
                    cleanup_sasl_data(result);
                    result = NULL;
                }
                else if (config_info->scope_id == NULL || mallocAndStrcpy_s(&result->scope_id, config_info->scope_id) != 0)
                {
                    LogError("Failure copying scope Id");
                    cleanup_sasl_data(result);
                    result = NULL;
                }
                else if ((result->nonce_handle = BUFFER_new()) == NULL)
                {
                    LogError("Failure creating nonce buff");
                    cleanup_sasl_data(result);
                    result = NULL;
                }
            }
        }
    }
    return (CONCRETE_SASL_MECHANISM_HANDLE)result;
}

static void prov_sasltpm_destroy(CONCRETE_SASL_MECHANISM_HANDLE handle)
{
    if (handle == NULL)
    {
        LogError("NULL sasl_mechanism_concrete_handle");
    }
    else
    {
        SASL_TPM_INSTANCE* sasl_tpm_info = (SASL_TPM_INSTANCE*)handle;
        cleanup_sasl_data(sasl_tpm_info);
    }
}

static const char* prov_sasltpm_get_mechanism_name(CONCRETE_SASL_MECHANISM_HANDLE handle)
{
    const char* result;
    if (handle == NULL)
    {
        LogError("NULL sasl_mechanism");
        result = NULL;
    }
    else
    {
        result = "TPM";
    }
    return result;
}

static int prov_sasltpm_get_init_bytes(CONCRETE_SASL_MECHANISM_HANDLE handle, SASL_MECHANISM_BYTES* init_bytes)
{
    int result;
    if (handle == NULL || init_bytes == NULL)
    {
        LogError("Bad arguments: sasl_mechanism_concrete_handle = %p, init_bytes = %p", handle, init_bytes);
        result = MU_FAILURE;
    }
    else
    {
        SASL_TPM_INSTANCE* sasl_tpm_info = (SASL_TPM_INSTANCE*)handle;

        if (sasl_tpm_info->data_buffer != NULL)
        {
            free(sasl_tpm_info->data_buffer);
            sasl_tpm_info->data_buffer = NULL;
        }

        unsigned char* ek = BUFFER_u_char(sasl_tpm_info->endorsement_key);
        size_t ek_len = BUFFER_length(sasl_tpm_info->endorsement_key);
        if (construct_send_data(sasl_tpm_info, sasl_tpm_info->scope_id, sasl_tpm_info->registration_id, ek, ek_len, &init_bytes->length) != 0)
        {
            LogError("Failed constructing send data");
            result = MU_FAILURE;
        }
        else
        {
            init_bytes->bytes = sasl_tpm_info->data_buffer;
            result = 0;
        }
    }
    return result;
}

static int prov_sasltpm_challenge(CONCRETE_SASL_MECHANISM_HANDLE handle, const SASL_MECHANISM_BYTES* challenge_bytes, SASL_MECHANISM_BYTES* resp_bytes)
{
    int result;

    if (handle == NULL || resp_bytes == NULL)
    {
        LogError("Bad arguments: concrete_sasl_mechanism: %p, response_bytes: %p", handle, resp_bytes);
        result = MU_FAILURE;
    }
    else
    {
        SASL_TPM_INSTANCE* sasl_tpm_info = (SASL_TPM_INSTANCE*)handle;
        if (sasl_tpm_info->data_buffer != NULL)
        {
            free(sasl_tpm_info->data_buffer);
            sasl_tpm_info->data_buffer = NULL;
        }

        unsigned char ctrl_byte = *(unsigned char*)(challenge_bytes->bytes);
        if (!is_valid_ctrl_byte(ctrl_byte) )
        {
            unsigned char* srk = BUFFER_u_char(sasl_tpm_info->storage_root_key);
            size_t srk_len = BUFFER_length(sasl_tpm_info->storage_root_key);
            if (construct_send_data(sasl_tpm_info, NULL, NULL, srk, srk_len, &resp_bytes->length) != 0)
            {
                LogError("Failed constructing send data");
                result = MU_FAILURE;
            }
            else
            {
                resp_bytes->bytes = sasl_tpm_info->data_buffer;
                result = 0;
            }
        }
        else
        {
            if (BUFFER_append_build(sasl_tpm_info->nonce_handle, ((unsigned char*)challenge_bytes->bytes)+1, challenge_bytes->length-1) != 0)
            {
                LogError("Failed building nonce value");
                result = MU_FAILURE;
            }
            else
            {
                if (!validate_sequence_num(sasl_tpm_info, ctrl_byte))
                {
                    LogError("Invalid sequence number received from sasl challenge");
                    result = MU_FAILURE;
                }
                else if (is_last_sequence(ctrl_byte))
                {
                    char* sas_token = sasl_tpm_info->challenge_cb(sasl_tpm_info->nonce_handle, sasl_tpm_info->user_ctx);
                    if (sas_token == NULL)
                    {
                        LogError("Failed creating sas token from challenge callback");
                        result = MU_FAILURE;
                    }
                    else
                    {
                        size_t sas_token_len = strlen(sas_token);
                        if (sas_token_len > MAX_FRAME_DATA_LEN)
                        {
                            LogError("Sas Token is too large for sasl frame");
                            result = MU_FAILURE;
                        }
                        else
                        {
                            resp_bytes->length = (uint32_t)sas_token_len + 1;
                            if ((sasl_tpm_info->data_buffer = (unsigned char*)malloc(resp_bytes->length)) == NULL)
                            {
                                LogError("Failed allocating initial data value");
                                result = MU_FAILURE;
                            }
                            else
                            {
                                sasl_tpm_info->ctrl_seq_num = 0;
                                unsigned char control_byte = 0;
                                sasl_tpm_info->data_buffer[0] = control_byte;
                                (void)memcpy(sasl_tpm_info->data_buffer + 1, sas_token, sas_token_len);
                                resp_bytes->bytes = sasl_tpm_info->data_buffer;
                                result = 0;
                            }
                        }
                        free(sas_token);
                    }
                }
                else
                {
                    if ((sasl_tpm_info->data_buffer = (unsigned char*)malloc(1)) == NULL)
                    {
                        LogError("Failed allocating initial data value");
                        result = MU_FAILURE;
                    }
                    else
                    {
                        sasl_tpm_info->ctrl_seq_num++;
                        sasl_tpm_info->data_buffer[0] = 0;
                        resp_bytes->bytes = sasl_tpm_info->data_buffer;
                        resp_bytes->length = 1;
                        result = 0;
                    }
                }
            }
        }
    }
    return result;
}

static const SASL_MECHANISM_INTERFACE_DESCRIPTION prov_sasltpm_interface =
{
    prov_sasltpm_create,
    prov_sasltpm_destroy,
    prov_sasltpm_get_init_bytes,
    prov_sasltpm_get_mechanism_name,
    prov_sasltpm_challenge
};

const SASL_MECHANISM_INTERFACE_DESCRIPTION* prov_sasltpm_get_interface(void)
{
    return &prov_sasltpm_interface;
}
