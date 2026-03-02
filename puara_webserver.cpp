#include "puara.h"

// Includes for webserver
#include <stdio.h>
#include <inttypes.h>

#include <zephyr/kernel.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/net/http/server.h>
#include <zephyr/net/http/service.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include "zephyr/device.h"
#include "zephyr/sys/util.h"
#include <zephyr/data/json.h>
#include <zephyr/sys/util_macro.h>
#include <zephyr/net/net_config.h>
LOG_MODULE_DECLARE(PUARA_MODULE, LOG_LEVEL_DBG);


// Websocket server settings
#define MAX_CLIENT_QUEUE 1
#define NUM_WEBSOCKET_HANDLERS 2
#define MAX_BUFFER_SIZE 1280
#define DEVICE_CONFIG_SLOT 0
#define SENSOR_SETTINGS_SLOT 1

// // Settings structuree for sensor settings

// #define PUARA_MAX_NESTED_SETTINGS 10
// struct puara_child_settings {
//     std::string name;
//     std::string description;
//     std::string type;
//     puara_sensor_value sensor_val[PUARA_MAX_ARRAY_SIZE];
//     size_t val_len;
// };

// struct puara_parent_settings {
//     std::string name;
//     std::string description;
//     std::vector<puara_child_settings> nested_settings;
// };


// Update ARRAY MAcros to play nicely with

// struct puara_sensor_value {
// 	std::string str_value;
// 	int int_value;
// 	float float_value;
// };

// JSON templates
static const struct json_obj_descr settingsVariables_descr[] = {
	JSON_OBJ_DESCR_PRIM(settingsVariables, name, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(settingsVariables, description, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(settingsVariables, type, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(settingsVariables, textValue, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(settingsVariables, numberValue, JSON_TOK_DOUBLE_FP),
};


static const struct json_obj_descr device_setting_descr[] = {
	JSON_OBJ_DESCR_OBJ_ARRAY(puara_device_settings, settings, PUARA_MAX_ARRAY_SIZE, settings_len, settingsVariables_descr, ARRAY_SIZE(settingsVariables_descr)),
};

static const struct json_obj_descr child_settings_descr[] = {
	JSON_OBJ_DESCR_PRIM(puara_child_settings, name, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(puara_child_settings, description, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(puara_child_settings, type, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(puara_child_settings, str_value, JSON_TOK_STRING),
};

static const struct json_obj_descr parent_settings_descr[] = {
	JSON_OBJ_DESCR_PRIM(puara_parent_settings, name, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(puara_parent_settings, description, JSON_TOK_STRING),
	JSON_OBJ_DESCR_OBJ_ARRAY(puara_parent_settings, nested_settings, PUARA_MAX_NESTED_SETTINGS, count, child_settings_descr, ARRAY_SIZE(child_settings_descr)),
};

// Webserver handlers
int Puara::deviceconfig_handler(struct http_client_ctx *client, enum http_transaction_status status,
			const struct http_request_ctx *request_ctx,
			struct http_response_ctx *response_ctx, void *user_data)
{
	enum http_method method = client->method;
	static size_t processed;
	int ret;

	if (status == HTTP_SERVER_TRANSACTION_ABORTED || status == HTTP_SERVER_TRANSACTION_COMPLETE) {
		if (status == HTTP_SERVER_TRANSACTION_ABORTED) {
			LOG_DBG("Transaction aborted after %zd bytes.", processed);
		}
		processed = 0;
		return 0;
	}

	__ASSERT_NO_MSG(request_ctx->data != NULL);

	processed += request_ctx->data_len;


	if (status == HTTP_SERVER_REQUEST_DATA_FINAL) {
		LOG_DBG("All data received (%zd bytes).", processed);
		processed = 0;
	}

	/* Send data to client if the client asked for it */
	char buffer[MAX_BUFFER_SIZE];
	if (method == HTTP_GET) {
		// Encode the setting sinto a buffer
		ret = json_obj_encode_buf(device_setting_descr, ARRAY_SIZE(device_setting_descr), &device_config, buffer, sizeof(buffer));
		
		// Error in encoding data to buffer
		if (ret < 0) {
			LOG_ERR("Error in encoding data to buffer: %d", ret);
			return ret;
		} else {
			response_ctx->body = (const uint8_t *)buffer;
			response_ctx->body_len = sizeof(buffer);
			response_ctx->final_chunk = (status == HTTP_SERVER_REQUEST_DATA_FINAL);
		}
	}

	/* If the client sent us device configuration retrieve it*/
	if (method == HTTP_POST) {
		puara_device_settings data;
		// Decode received data from client
		ret = json_obj_parse((char *)request_ctx->data, request_ctx->data_len, device_setting_descr, ARRAY_SIZE(device_setting_descr), &data);

		// Save data to device
		if (ret < 0) {
			LOG_ERR("Error in parsing received data from client: %d", ret);
			return ret;
		}
	}

	return 0;
}
