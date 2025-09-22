/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#pragma once

#include "dx_config.h"
#include "dx_exit_codes.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mqtt.h"
#include "mqtt_pal.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /// <summary>
    /// MQTT connection configuration structure
    /// </summary>
    typedef struct DX_MQTT_CONFIG
    {
        const char *hostname;
        const char *port;
        const char *client_id;
        const char *username;
        const char *password;
        uint16_t keep_alive_seconds;
        bool clean_session;
    } DX_MQTT_CONFIG;

    /// <summary>
    /// MQTT message structure for publishing
    /// </summary>
    typedef struct DX_MQTT_MESSAGE
    {
        const char *topic;
        const void *payload;
        size_t payload_length;
        uint8_t qos;
        bool retain;
    } DX_MQTT_MESSAGE;

    /// <summary>
    /// Callback function prototype for handling received messages
    /// </summary>
    /// <param name="topic">Topic on which the message was received</param>
    /// <param name="payload">Message payload</param>
    /// <param name="payload_length">Length of the payload</param>
    /// <param name="context">User-defined context passed during initialization</param>
    typedef void (*DX_MQTT_MESSAGE_RECEIVED_HANDLER)(const char *topic, const void *payload, size_t payload_length, void *context);

    /// <summary>
    /// Initialize and connect to an MQTT broker
    /// </summary>
    /// <param name="config">MQTT connection configuration</param>
    /// <param name="message_handler">Callback function for received messages (can be NULL)</param>
    /// <param name="context">User context to pass to the message handler</param>
    /// <returns>True on success, false on failure</returns>
    bool dx_mqttConnect(const DX_MQTT_CONFIG *config, DX_MQTT_MESSAGE_RECEIVED_HANDLER message_handler, void *context);

    /// <summary>
    /// Publish a message to an MQTT topic
    /// </summary>
    /// <param name="message">Message to publish</param>
    /// <returns>True on success, false on failure</returns>
    bool dx_mqttPublish(const DX_MQTT_MESSAGE *message);

    /// <summary>
    /// Subscribe to an MQTT topic
    /// </summary>
    /// <param name="topic">Topic to subscribe to</param>
    /// <param name="qos">Quality of Service level (0, 1, or 2)</param>
    /// <returns>True on success, false on failure</returns>
    bool dx_mqttSubscribe(const char *topic, uint8_t qos);

    /// <summary>
    /// Unsubscribe from an MQTT topic
    /// </summary>
    /// <param name="topic">Topic to unsubscribe from</param>
    /// <returns>True on success, false on failure</returns>
    bool dx_mqttUnsubscribe(const char *topic);

    /// <summary>
    /// Check if MQTT client is connected to the broker
    /// </summary>
    /// <returns>True if connected, false otherwise</returns>
    bool dx_isMqttConnected(void);

    /// <summary>
    /// Get the last MQTT error message
    /// </summary>
    /// <returns>String description of the last error, or NULL if no error</returns>
    const char *dx_mqttGetLastError(void);

    /// <summary>
    /// Disconnect from MQTT broker and cleanup resources
    /// </summary>
    void dx_mqttDisconnect(void);

#ifdef __cplusplus
}
#endif