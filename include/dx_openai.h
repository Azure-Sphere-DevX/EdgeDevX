// #pragma once

// #include <stdbool.h>
// #include <stddef.h>
// #include <stdint.h>

// typedef struct DX_OPENAI_FUNCTION_PROPERTY_T
// {
//     const char *name;
//     const char *description;
//     const char *enum_value;
// } DX_OPENAI_FUNCTION_PROPERTY_T;

// typedef struct
// {
//     const char *name;
//     const char *description;
//     struct DX_OPENAI_FUNCTION_PROPERTY_T **properties;
// } DX_OPENAI_FUNCTION_T;

// typedef struct DX_OPENAI_CHAT_CTX
// {
//     const char *openai_api_key;
//     const char *azure_openai_key;
//     const char **user_prompt;
//     size_t user_prompt_count;
//     const char **system_prompt;
//     size_t system_prompt_count;
//     const char **assistant_prompt;
//     size_t assistant_prompt_count;
//     DX_OPENAI_FUNCTION_T **functions;
//     size_t function_count;
//     const char **required_function;
//     size_t required_function_count;
//     bool stream;
//     int temperature;
//     int tokens;
//     int (*response_callback)(const char *response, size_t response_len);
// } DX_OPENAI_CHAT_CTX;

// typedef struct DX_OPENAI_WHISPER_CTX
// {
//     const char *openai_api_key;
//     const char *azure_openai_key;
//     uint8_t *audio;
//     int audio_len;
//     int (*response_callback)(const char *response, size_t response_len);
// } DX_OPENAI_WHISPER_CTX;

// int dx_openai_chat(DX_OPENAI_CHAT_CTX *ctx);
// int dx_openai_transcribe(DX_OPENAI_WHISPER_CTX *ctx);
