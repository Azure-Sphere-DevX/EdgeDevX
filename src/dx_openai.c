// #include "dx_openai.h"
// #include "parson.h"
// #include <curl/curl.h>

// // generate a prompt string from the user, system and assistant prompts
// // and the stream and temperature properties
// // Note, the caller is responsible for freeing the returned string with json_free_serialized_string
// char *generate_prompt(struct DX_OPENAI_CHAT_CTX *ctx)
// {
//     // create a new json object with parson and add the prompts, stream and temperature properties to it
//     // then convert it to a string and return it

//     // create a new parson object
//     JSON_Value *root_value = json_value_init_object();

//     // add the user prompts to the json object
//     JSON_Object *root_object = json_value_get_object(root_value);
//     for (int i = 0; i < ctx->user_prompt_count; i++)
//     {
//         json_object_set_string(root_object, "user", ctx->user_prompt[i]);
//     }

//     for (int i = 0; i < ctx->system_prompt_count; i++)
//     {
//         json_object_set_string(root_object, "system", ctx->system_prompt[i]);
//     }

//     for (int i = 0; i < ctx->assistant_prompt_count; i++)
//     {
//         json_object_set_string(root_object, "assistant", ctx->assistant_prompt[i]);
//     }

//     // add stream and temperature to the json object
//     json_object_set_boolean(root_object, "stream", ctx->stream);
//     json_object_set_number(root_object, "temperature", ctx->temperature);

//     // convert the json object to a string
//     char *json_string = json_serialize_to_string(root_value);

//     // free the json object
//     json_value_free(root_value);

//     return json_string;
// }

// int dx_openai_chat(DX_OPENAI_CHAT_CTX *ctx)
// {

//     // generate the prompt string from the user, system and assistant prompts
//     // and the stream and temperature properties
//     char *prompt = generate_prompt(ctx);

//     // .. do work here make the curl request to the openai api



//     // free the prompt string
//     json_free_serialized_string(prompt);

//     return 0;
// }

// int dx_openai_transcribe(DX_OPENAI_WHISPER_CTX *ctx)
// {
//     return 0;
// }