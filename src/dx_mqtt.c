/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#include "dx_mqtt.h"

#include "dx_utilities.h"
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// MQTT-C includes
#include "mqtt.h"
#include "mqtt_pal.h"

// System includes for socket operations
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

// Internal state management
static struct mqtt_client _client;
static int _sockfd = -1;
static pthread_t _client_daemon = 0;
static bool _is_initialized = false;
static bool _is_connected = false;
static bool _daemon_running = false;

// Buffers for MQTT client
static uint8_t _send_buffer[2048];
static uint8_t _recv_buffer[1024];

// Message handling
static DX_MQTT_MESSAGE_RECEIVED_HANDLER _message_handler = NULL;
static void *_user_context = NULL;

// Error tracking
static char _last_error[256] = {0};

// Function prototypes
static void publish_callback(void **unused, struct mqtt_response_publish *published);
static void *client_refresher(void *client);
static bool cleanup_connection(void);
static void set_last_error(const char *format, ...);
static int open_nb_socket(const char *addr, const char *port);

/// <summary>
/// MQTT publish callback - called when a message is received
/// </summary>
/// <param name="unused">Unused parameter</param>
/// <param name="published">Published message details</param>
static void publish_callback(void **unused, struct mqtt_response_publish *published)
{
    if (_message_handler == NULL || published == NULL) {
        return;
    }

    // Create null-terminated topic string
    char *topic = malloc(published->topic_name_size + 1);
    if (topic == NULL) {
        set_last_error("Failed to allocate memory for topic");
        return;
    }

    memcpy(topic, published->topic_name, published->topic_name_size);
    topic[published->topic_name_size] = '\0';

    // Call user's message handler
    _message_handler(topic, published->application_message, published->application_message_size, _user_context);

    free(topic);
}

/// <summary>
/// MQTT client refresher thread - handles message processing and connection monitoring
/// </summary>
/// <param name="client">MQTT client pointer</param>
/// <returns>NULL</returns>
static void *client_refresher(void *client)
{
    _daemon_running = true;
    
    dx_Log_Debug("DX MQTT: Background processing thread started\n");
    
    while (_daemon_running && _is_connected) {
        // Process MQTT operations (send/receive messages, handle keepalive, etc.)
        int result = mqtt_sync((struct mqtt_client *)client);
        
        if (result != MQTT_OK) {
            set_last_error("MQTT sync failed: %s", mqtt_error_str(_client.error));
            _is_connected = false;
            dx_Log_Debug("DX MQTT: Connection lost in background thread\n");
            break;
        }
        
        // Check for any client errors that might have occurred
        if (_client.error != MQTT_OK) {
            set_last_error("MQTT client error: %s", mqtt_error_str(_client.error));
            _is_connected = false;
            dx_Log_Debug("DX MQTT: Client error detected in background thread\n");
            break;
        }
        
        // Sleep for 100ms between processing cycles
        // This provides responsive message handling while not consuming too much CPU
        usleep(100000U);
    }
    
    _daemon_running = false;
    dx_Log_Debug("DX MQTT: Background processing thread stopped\n");
    return NULL;
}

/// <summary>
/// Set the last error message
/// </summary>
/// <param name="format">Format string</param>
/// <param name="...">Format arguments</param>
static void set_last_error(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(_last_error, sizeof(_last_error), format, args);
    va_end(args);
    
    dx_Log_Debug("DX MQTT Error: %s\n", _last_error);
}

/// <summary>
/// Clean up MQTT connection resources
/// </summary>
/// <returns>True on success</returns>
static bool cleanup_connection(void)
{
    bool success = true;

    // Stop the client daemon
    if (_daemon_running) {
        _daemon_running = false;
        if (_client_daemon != 0) {
            pthread_cancel(_client_daemon);
            pthread_join(_client_daemon, NULL);
            _client_daemon = 0;
        }
    }

    // Close socket
    if (_sockfd != -1) {
        close(_sockfd);
        _sockfd = -1;
    }

    _is_connected = false;
    _is_initialized = false;

    return success;
}

/// <summary>
/// Initialize and connect to an MQTT broker
/// </summary>
/// <param name="config">MQTT connection configuration</param>
/// <param name="message_handler">Callback function for received messages (can be NULL)</param>
/// <param name="context">User context to pass to the message handler</param>
/// <returns>True on success, false on failure</returns>
bool dx_mqttConnect(const DX_MQTT_CONFIG *config, DX_MQTT_MESSAGE_RECEIVED_HANDLER message_handler, void *context)
{
    if (config == NULL || config->hostname == NULL) {
        set_last_error("Invalid configuration parameters");
        return false;
    }

    // Clean up any existing connection
    if (_is_initialized) {
        cleanup_connection();
    }

    // Store message handler and context
    _message_handler = message_handler;
    _user_context = context;

    // Set defaults
    const char *port = config->port ? config->port : "1883";
    const char *client_id = config->client_id; // Can be NULL for anonymous client
    uint16_t keep_alive = config->keep_alive_seconds > 0 ? config->keep_alive_seconds : 400;

    dx_Log_Debug("DX MQTT: Connecting to %s:%s\n", config->hostname, port);

    // Open socket connection
    _sockfd = open_nb_socket(config->hostname, port);
    if (_sockfd == -1) {
        set_last_error("Failed to open socket to %s:%s", config->hostname, port);
        return false;
    }

    // Initialize MQTT client
    mqtt_init(&_client, _sockfd, _send_buffer, sizeof(_send_buffer), _recv_buffer, sizeof(_recv_buffer), publish_callback);

    // Prepare connection flags
    uint8_t connect_flags = 0;
    if (config->clean_session) {
        connect_flags |= MQTT_CONNECT_CLEAN_SESSION;
    }

    // Connect to broker
    if (mqtt_connect(&_client, client_id, config->username, config->password,
                     config->password ? strlen(config->password) : 0,
                     NULL, NULL, connect_flags, keep_alive) != MQTT_OK) {
        set_last_error("MQTT connect failed: %s", mqtt_error_str(_client.error));
        cleanup_connection();
        return false;
    }

    // Check for connection errors
    if (_client.error != MQTT_OK) {
        set_last_error("MQTT connection error: %s", mqtt_error_str(_client.error));
        cleanup_connection();
        return false;
    }

    // Start client daemon thread for automatic background processing
    if (pthread_create(&_client_daemon, NULL, client_refresher, &_client) != 0) {
        set_last_error("Failed to start MQTT background processing thread");
        cleanup_connection();
        return false;
    }

    _is_initialized = true;
    _is_connected = true;

    dx_Log_Debug("DX MQTT: Successfully connected to %s:%s with automatic background processing\n", config->hostname, port);
    return true;
}

/// <summary>
/// Publish a message to an MQTT topic
/// </summary>
/// <param name="message">Message to publish</param>
/// <returns>True on success, false on failure</returns>
bool dx_mqttPublish(const DX_MQTT_MESSAGE *message)
{
    // Check if dx_mqttConnect was called first
    if (!_is_initialized) {
        return false;
    }
    
    // Check if still connected
    if (!_is_connected) {
        return false;
    }
    
    // Validate message parameters
    if (message == NULL || message->topic == NULL) {
        set_last_error("Invalid message parameters - message and topic cannot be NULL");
        return false;
    }

    // Default QoS to 0 if not specified or invalid
    uint8_t qos = MQTT_PUBLISH_QOS_0;
    if (message->qos == 1) {
        qos = MQTT_PUBLISH_QOS_1;
    } else if (message->qos == 2) {
        qos = MQTT_PUBLISH_QOS_2;
    }

    // Publish the message
    if (mqtt_publish(&_client, message->topic, message->payload, message->payload_length, qos) != MQTT_OK) {
        set_last_error("MQTT publish failed: %s", mqtt_error_str(_client.error));
        if (_client.error != MQTT_OK) {
            _is_connected = false;
        }
        return false;
    }
    return true;
}

/// <summary>
/// Subscribe to an MQTT topic
/// </summary>
/// <param name="topic">Topic to subscribe to</param>
/// <param name="qos">Quality of Service level (0, 1, or 2)</param>
/// <returns>True on success, false on failure</returns>
bool dx_mqttSubscribe(const char *topic, uint8_t qos)
{
    // Check if dx_mqttConnect was called first
    if (!_is_initialized) {
        set_last_error("MQTT client not initialized - dx_mqttConnect must be called first");
        return false;
    }
    
    // Check if still connected
    if (!_is_connected) {
        set_last_error("MQTT client not connected - connection may have been lost");
        return false;
    }
    
    // Validate topic parameter
    if (topic == NULL) {
        set_last_error("Invalid topic - topic cannot be NULL");
        return false;
    }

    // Validate QoS
    if (qos > 2) {
        qos = 0;
    }

    if (mqtt_subscribe(&_client, topic, qos) != MQTT_OK) {
        set_last_error("MQTT subscribe failed: %s", mqtt_error_str(_client.error));
        if (_client.error != MQTT_OK) {
            _is_connected = false;
        }
        return false;
    }

    dx_Log_Debug("DX MQTT: Subscribed to topic '%s' with QoS %d\n", topic, qos);
    return true;
}

/// <summary>
/// Unsubscribe from an MQTT topic
/// </summary>
/// <param name="topic">Topic to unsubscribe from</param>
/// <returns>True on success, false on failure</returns>
bool dx_mqttUnsubscribe(const char *topic)
{
    // Check if dx_mqttConnect was called first
    if (!_is_initialized) {
        set_last_error("MQTT client not initialized - dx_mqttConnect must be called first");
        return false;
    }
    
    // Check if still connected
    if (!_is_connected) {
        set_last_error("MQTT client not connected - connection may have been lost");
        return false;
    }
    
    // Validate topic parameter
    if (topic == NULL) {
        set_last_error("Invalid topic - topic cannot be NULL");
        return false;
    }

    if (mqtt_unsubscribe(&_client, topic) != MQTT_OK) {
        set_last_error("MQTT unsubscribe failed: %s", mqtt_error_str(_client.error));
        if (_client.error != MQTT_OK) {
            _is_connected = false;
        }
        return false;
    }

    dx_Log_Debug("DX MQTT: Unsubscribed from topic '%s'\n", topic);
    return true;
}



/// <summary>
/// Check if MQTT client is connected to the broker
/// </summary>
/// <returns>True if connected, false otherwise</returns>
bool dx_isMqttConnected(void)
{
    return _is_connected && _is_initialized && (_client.error == MQTT_OK);
}

/// <summary>
/// Get the last MQTT error message
/// </summary>
/// <returns>String description of the last error, or NULL if no error</returns>
const char *dx_mqttGetLastError(void)
{
    return strlen(_last_error) > 0 ? _last_error : NULL;
}

/// <summary>
/// Disconnect from MQTT broker and cleanup resources
/// </summary>
void dx_mqttDisconnect(void)
{
    if (!_is_initialized) {
        return;
    }

    dx_Log_Debug("DX MQTT: Disconnecting from broker\n");

    // Send disconnect message if still connected
    if (_is_connected && _client.error == MQTT_OK) {
        mqtt_disconnect(&_client);
    }

    // Cleanup all resources
    cleanup_connection();

    // Clear error state
    _last_error[0] = '\0';

    dx_Log_Debug("DX MQTT: Disconnected and cleaned up\n");
}

/// <summary>
/// Open a non-blocking socket connection to the specified host and port
/// </summary>
/// <param name="addr">Host address</param>
/// <param name="port">Port number</param>
/// <returns>Socket file descriptor on success, -1 on failure</returns>
static int open_nb_socket(const char *addr, const char *port)
{
    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC; /* IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* Must be TCP */
    
    int sockfd = -1;
    int rv;
    struct addrinfo *p, *servinfo;

    /* get address information */
    rv = getaddrinfo(addr, port, &hints, &servinfo);
    if (rv != 0) {
        set_last_error("getaddrinfo failed: %s", gai_strerror(rv));
        return -1;
    }

    /* open the first valid socket */
    for (p = servinfo; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) continue;

        /* set to non-blocking */
        int flags = fcntl(sockfd, F_GETFL, 0);
        if (flags == -1) {
            close(sockfd);
            sockfd = -1;
            continue;
        }
        if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
            close(sockfd);
            sockfd = -1;
            continue;
        }

        /* connect to server */
        rv = connect(sockfd, p->ai_addr, p->ai_addrlen);
        if (rv == -1) {
            if (errno != EINPROGRESS) {
                close(sockfd);
                sockfd = -1;
                continue;
            }
            
            /* Connection in progress - wait for completion */
            fd_set write_fds, error_fds;
            struct timeval timeout;
            
            FD_ZERO(&write_fds);
            FD_ZERO(&error_fds);
            FD_SET(sockfd, &write_fds);
            FD_SET(sockfd, &error_fds);
            
            timeout.tv_sec = 10;  /* 10 second timeout */
            timeout.tv_usec = 0;
            
            rv = select(sockfd + 1, NULL, &write_fds, &error_fds, &timeout);
            if (rv <= 0 || FD_ISSET(sockfd, &error_fds)) {
                close(sockfd);
                sockfd = -1;
                continue;
            }
            
            /* Check if connection actually succeeded */
            int error = 0;
            socklen_t len = sizeof(error);
            if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) == -1 || error != 0) {
                close(sockfd);
                sockfd = -1;
                continue;
            }
        }
        break;
    }

    freeaddrinfo(servinfo);

    if (sockfd == -1) {
        set_last_error("Failed to open socket to %s:%s", addr, port);
        return -1;
    }

    return sockfd;
}