/* 

Puara Module Manager                                                     
Metalab - Société des Arts Technologiques (SAT)                          
Input Devices and Music Interaction Laboratory (IDMIL), McGill University
Edu Meneses (2022) - https://www.edumeneses.com                          

- event_handler, wifi_init_sta, and start_wifi were modified from 
  https://github.com/espressif/esp-idf/tree/master/examples/wifi/getting_started/station
- mount_spiffs, and unmount_spiffs were modified from
  https://github.com/espressif/esp-idf/tree/master/examples/storage

*/

#include <puara.h>

LOG_MODULE_REGISTER(puara_module);

#define MACSTR "%02X:%02X:%02X:%02X:%02X:%02X"
#define NET_EVENT_WIFI_MASK                                                                        \
	(NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT |                        \
	 NET_EVENT_WIFI_AP_ENABLE_RESULT | NET_EVENT_WIFI_AP_DISABLE_RESULT |                      \
	 NET_EVENT_WIFI_AP_STA_CONNECTED | NET_EVENT_WIFI_AP_STA_DISCONNECTED)

#define WIFI_AP_IP_ADDRESS "192.168.4.1"
#define WIFI_AP_NETMASK    "255.255.255.0"
// Declare static variables
bool Puara::wifi_enabled = false;
bool Puara::ap_enabled = false;
char Puara::APpasswd[PUARA_MAX_CONFIG_LENGTH] = "mappings";
char Puara::wifiSSID[PUARA_MAX_CONFIG_LENGTH] = "tstick_network";
char Puara::wifiPSK[PUARA_MAX_CONFIG_LENGTH] = "mappings";
char Puara::oscIP1[PUARA_MAX_CONFIG_LENGTH] = "192.168.137.1";
char Puara::oscIP2[PUARA_MAX_CONFIG_LENGTH] = "0.0.0.0";
char Puara::device[PUARA_MAX_CONFIG_LENGTH] = "Puara";
int Puara::oscPORT1 = 8000;
int Puara::oscPORT2 = 8000;
bool Puara::enableLibmapper = false; 
int Puara::id = 1;
std::vector<puara_parent_settings> Puara::parent_variables = {};
std::vector<puara_child_settings> Puara::variables = {};
std::unordered_map<std::string,int> Puara::variables_fields = {};
int Puara::http_service_port = 80;

// Webserver resources
uint8_t Puara::index_html_gz[] = {
#include "index.html.gz.inc"
};
uint8_t Puara::factory_html_gz[] = {
#include "factory.html.gz.inc"
};
uint8_t Puara::reboot_html_gz[] = {
#include "reboot.html.gz.inc"
};
uint8_t Puara::saved_html_gz[] = {
#include "saved.html.gz.inc"
};
uint8_t Puara::scan_html_gz[] = {
#include "scan.html.gz.inc"
};
uint8_t Puara::settings_html_gz[] = {
#include "settings.html.gz.inc"
};
uint8_t Puara::update_html_gz[] = {
#include "update.html.gz.inc"
};
uint8_t Puara::style_css_gz[] = {
#include "style.css.gz.inc"
};
// Static HTTP resources
http_resource_detail_static Puara::index_html_gz_resource_detail = {
	.common = {
            .bitmask_of_supported_http_methods = BIT(HTTP_GET),
			.type = HTTP_RESOURCE_TYPE_STATIC,
			.content_encoding = "gzip",
			.content_type = "text/html",
		},
	.static_data = index_html_gz,
	.static_data_len = sizeof(index_html_gz),
};
http_resource_detail_static Puara::reboot_html_gz_resource_detail = {
	.common = {
            .bitmask_of_supported_http_methods = BIT(HTTP_GET),
			.type = HTTP_RESOURCE_TYPE_STATIC,
			.content_encoding = "gzip",
			.content_type = "text/html",
		},
	.static_data = reboot_html_gz,
	.static_data_len = sizeof(reboot_html_gz),
};
http_resource_detail_static Puara::factory_html_gz_resource_detail = {
	.common = {
            .bitmask_of_supported_http_methods = BIT(HTTP_GET),
			.type = HTTP_RESOURCE_TYPE_STATIC,
			.content_encoding = "gzip",
			.content_type = "text/html",
		},
	.static_data = factory_html_gz,
	.static_data_len = sizeof(factory_html_gz),
};
http_resource_detail_static Puara::saved_html_gz_resource_detail = {
	.common = {
            .bitmask_of_supported_http_methods = BIT(HTTP_GET),
			.type = HTTP_RESOURCE_TYPE_STATIC,
			.content_encoding = "gzip",
			.content_type = "text/html",
		},
	.static_data = saved_html_gz,
	.static_data_len = sizeof(saved_html_gz),
};
http_resource_detail_static Puara::scan_html_gz_resource_detail = {
	.common = {
            .bitmask_of_supported_http_methods = BIT(HTTP_GET),
			.type = HTTP_RESOURCE_TYPE_STATIC,
			.content_encoding = "gzip",
			.content_type = "text/html",
		},
	.static_data = scan_html_gz,
	.static_data_len = sizeof(scan_html_gz),
};
http_resource_detail_static Puara::settings_html_gz_resource_detail = {
	.common = {
            .bitmask_of_supported_http_methods = BIT(HTTP_POST),
			.type = HTTP_RESOURCE_TYPE_STATIC,
			.content_encoding = "gzip",
			.content_type = "text/html",
		},
	.static_data = settings_html_gz,
	.static_data_len = sizeof(settings_html_gz),
};
http_resource_detail_static Puara::update_html_gz_resource_detail = {
	.common = {
            .bitmask_of_supported_http_methods = BIT(HTTP_GET),
			.type = HTTP_RESOURCE_TYPE_STATIC,
			.content_encoding = "gzip",
			.content_type = "text/html",
		},
	.static_data = update_html_gz,
	.static_data_len = sizeof(update_html_gz),
};
http_resource_detail_static Puara::style_css_gz_resource_detail = {
	.common = {
            .bitmask_of_supported_http_methods = BIT(HTTP_GET),
			.type = HTTP_RESOURCE_TYPE_STATIC,
			.content_encoding = "gzip",
			.content_type = "text/css",
		},
	.static_data = style_css_gz,
	.static_data_len = sizeof(style_css_gz),
};

// Define HTTP Services
HTTP_SERVICE_DEFINE(puara_service, NULL, &Puara::http_service_port,
		    CONFIG_HTTP_SERVER_MAX_CLIENTS, 10, NULL, NULL, NULL);

HTTP_RESOURCE_DEFINE(index_html_gz_resource, puara_service, "/",
		     &Puara::index_html_gz_resource_detail);
HTTP_RESOURCE_DEFINE(factory_html_gz_resource, puara_service, "/factory",
		     &Puara::factory_html_gz_resource_detail);
HTTP_RESOURCE_DEFINE(reboot_html_gz_resource, puara_service, "/reboot",
		     &Puara::reboot_html_gz_resource_detail);
HTTP_RESOURCE_DEFINE(saved_html_gz_resource, puara_service, "/saved",
		     &Puara::saved_html_gz_resource_detail);
HTTP_RESOURCE_DEFINE(scan_html_gz_resource, puara_service, "/scan",
		     &Puara::scan_html_gz_resource_detail);
HTTP_RESOURCE_DEFINE(settings_html_gz_resource, puara_service, "/settings",
		     &Puara::settings_html_gz_resource_detail);
HTTP_RESOURCE_DEFINE(update_html_gz_resource, puara_service, "/update",
		     &Puara::update_html_gz_resource_detail);
HTTP_RESOURCE_DEFINE(style_css_gz_resource, puara_service, "/style.css",
		     &Puara::style_css_gz_resource_detail);

unsigned int Puara::get_version() {
    return version;
};

void Puara::set_version(unsigned int user_version) {
    version = user_version;
};

void Puara::start(std::vector<puara_parent_settings> sensor_setings, Monitors monitor) {
    std::cout 
    << "\n"
    << "**********************************************************\n"
    << "* Puara Module Manager                                   *\n"
    << "* Metalab - Société des Arts Technologiques (SAT)        *\n"
    << "* Input Devices and Music Interaction Laboratory (IDMIL) *\n"
    << "* Edu Meneses (2022) - https://www.edumeneses.com        *\n"
    << "* Firmware version: " << version << "                             *\n"
    << "**********************************************************\n"
    << std::endl;

    // Get variables for the device
    if (sensor_setings.size() > 0) {
        for (auto parent_temp: sensor_setings) {
            parent_variables.push_back(parent_temp);

            // Get every child variable in the parent variable structure
            if (parent_temp.nested_settings.size() > 0) {
                for (auto temp: parent_temp.nested_settings) {
                    if (variables_fields.find(temp.name) == variables_fields.end()) {
                        variables_fields.insert({temp.name, variables.size()});
                        variables.push_back(temp);
                    } else {
                        int variable_index = variables_fields.at(temp.name);
                        variables.at(variable_index) = temp;
                    }
                }
            }
        }
    }
      
    // Setup storage
    settings_subsys_init();
    settings_register(&puara_config); // register settings
    settings_register(&puara_variables); // register variables
    settings_load();

    start_wifi();
    start_webserver();
    start_mdns_service(dmiName, dmiName);
    wifi_scan();

    module_monitor = monitor;

    // some delay added as start listening blocks the hw monitor
    std::cout << "Puara Start Done!\n\n  Type \"puara reboot\" in the serial monitor to reset the controller.\n\n";
}

void Puara::sta_connect() {
    int ret = 0;
	ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, sta_iface, &wifi_config_sta,
			   sizeof(struct wifi_connect_req_params));

    if (ret) {
		LOG_ERR("Unable to Connect to (%s)", wifiSSID);
	}
}

void Puara::ap_connect() {
	if (!ap_iface) {
		LOG_ERR("AP: is not initialized");
	}

	LOG_INF("Turning on AP Mode");
	if (strlen(APpasswd) == 0) {
		wifi_config_ap.security = WIFI_SECURITY_TYPE_NONE;
	} else {

		wifi_config_ap.security = WIFI_SECURITY_TYPE_PSK;
	}

	int ret = net_mgmt(NET_REQUEST_WIFI_AP_ENABLE, ap_iface, &wifi_config_ap,
			   sizeof(struct wifi_connect_req_params));
	if (ret) {
		LOG_ERR("NET_REQUEST_WIFI_AP_ENABLE failed, err: %d", ret);
	}
}

void Puara::wifi_init() {
    // Adding network call back events
    net_mgmt_init_event_callback(&cb, wifi_event_handler, NET_EVENT_WIFI_MASK);
	net_mgmt_add_event_callback(&cb);

    // Wait for iface to be initialised
    sta_iface = net_if_get_wifi_sta();
    while (!sta_iface) {
        LOG_INF("STA: is not initialized");
        sta_iface = net_if_get_wifi_sta();
    }
    LOG_INF("STA: is initialized");

    // Wait for ap to be initialised
    ap_iface = net_if_get_wifi_sap();
    while (!ap_iface) {
        LOG_INF("AP: is not initialized");
        ap_iface = net_if_get_wifi_sap();
    }
    LOG_INF("AP: is initialized");

    // Disable Power saving
    struct wifi_ps_params params;
    params.enabled = WIFI_PS_DISABLED;
    params.type = WIFI_PS_PARAM_STATE;
    
    // Request disabling power saving
    if (net_mgmt(NET_REQUEST_WIFI_PS, sta_iface, &params, sizeof(params))) {
		LOG_INF("PS %s failed. Reason: %s\n",
			   params.enabled ? "enable" : "disable",
			   wifi_ps_get_config_err_code_str(params.fail_reason));
	}

    // Connect to wifi
    sta_connect();

    // Enabled access point
    ap_connect();
}

void Puara::start_wifi() {

    ApStarted = false;

    // Check if device name is empty
    if (strlen(device) == 0) {
        std::cout << "start_wifi: Module name unpopulated. Using default name: Puara" << std::endl;
        strcpy(device,"Puara");
    }

    // Create dmiName from device name and ID
    std::stringstream tempBuf;
    tempBuf << Puara::device << "_" << std::setfill('0') << std::setw(PUARA_MAX_ID_LENGTH) << id;
    Puara::dmiName = tempBuf.str();

    // Check if wifiSSID is empty and wifiPSK have less than 8 characteres
    if ( strlen(APpasswd) < 8) {
        std::cout 
        << "startWifi: AP password error. Possible causes:" << "\n"
        << "startWifi:   - no AP password" << "\n"
        << "startWifi:   - password is less than 8 characteres long" << "\n"
        << "startWifi:   - password is set to \"password\"" << "\n"
        << "startWifi: Using default AP password: password" << "\n"
        << "startWifi: It is strongly recommended to change the password" << std::endl;
        strcpy(APpasswd, "password");
    }
    if ( strlen(wifiSSID) < 1) {
        std::cout << "start_wifi: No blank SSID allowed. Using default name: Puara" << std::endl;
        strcpy(wifiSSID, "Puara");
    }

    // Configure wifi station settings
    wifi_config_sta.ssid = (const uint8_t *)wifiSSID;
	wifi_config_sta.ssid_length = strlen(wifiSSID);
	wifi_config_sta.psk = (const uint8_t *)wifiPSK;
	wifi_config_sta.psk_length = strlen(wifiPSK);
	wifi_config_sta.security = WIFI_SECURITY_TYPE_PSK;
	wifi_config_sta.channel = WIFI_CHANNEL_ANY;
	wifi_config_sta.band = WIFI_FREQ_BAND_UNKNOWN;
    wifi_config_sta.bandwidth = WIFI_FREQ_BANDWIDTH_20MHZ;
    wifi_config_sta.mfp = WIFI_MFP_OPTIONAL;

    // Configure wifi ap settings
    // Default to a 5GHz Soft access point on channel 149
    wifi_config_ap.ssid = (const uint8_t *)dmiName.c_str();
	wifi_config_ap.ssid_length = dmiName.length();
	wifi_config_ap.psk = (const uint8_t *)APpasswd;
	wifi_config_ap.psk_length = strlen(APpasswd);
	wifi_config_ap.channel = 149;
	wifi_config_ap.band = WIFI_FREQ_BAND_5_GHZ;
    wifi_config_ap.bandwidth = WIFI_FREQ_BANDWIDTH_20MHZ;

    //Initialize Wifi
    std::cout << "startWifi: Starting WiFi config" << std::endl;
    Puara::connect_counter = 0;
    wifi_init();
    ApStarted = false;
}

std::string Puara::get_dmi_name() {
    return dmiName;
}

void Puara::find_and_replace(std::string old_text, std::string new_text, std::string & str) {

    std::size_t old_text_position = str.find(old_text);
    while (old_text_position!=std::string::npos) {
        str.replace(old_text_position,old_text.length(),new_text);
        old_text_position = str.find(old_text);
    }
    std::cout << "http (find_and_replace): Success" << std::endl;
}

void Puara::find_and_replace(std::string old_text, double new_number, std::string & str) {

    std::size_t old_text_position = str.find(old_text);
    while (old_text_position!=std::string::npos) {
        std::string conversion = std::to_string(new_number);
        str.replace(old_text_position,old_text.length(),conversion);
        old_text_position = str.find(old_text);
    }
    std::cout << "http (find_and_replace): Success" << std::endl;
}

void Puara::find_and_replace(std::string old_text, unsigned int new_number, std::string & str) {

    std::size_t old_text_position = str.find(old_text);
    while (old_text_position!=std::string::npos) {
        std::string conversion = std::to_string(new_number);
        str.replace(old_text_position,old_text.length(),conversion);
        old_text_position = str.find(old_text);
    }
    std::cout << "http (find_and_replace): Success" << std::endl;
}

void Puara::checkmark(std::string old_text, bool value, std::string & str) {

    std::size_t old_text_position = str.find(old_text);
    if (old_text_position!=std::string::npos) {
        std::string conversion;
        if (value) {
            conversion = "checked";
        } else {
            conversion = "";
        }
        str.replace(old_text_position,old_text.length(),conversion);
        std::cout << "http (checkmark): Success" << std::endl;
    } else {
        std::cout << "http (checkmark): Could not find the requested string" << std::endl;
    }
}

int Puara::start_webserver(void) {    
    // if (!ApStarted) {
    //     std::cout << "start_webserver: Cannot start webserver: AP and STA not initializated" << std::endl;
    //     return NULL;
    // }
    
    // Start webserver
    http_server_start();

    return 0;
}

void Puara::stop_webserver(void) {
    // Stop the httpd server
    // httpd_stop(webserver);
}
std::string Puara::convertToString(char* a) {
    std::string s(a);
    return s;
}

void Puara::send_serial_data(std::string data) {
    std::cout << Puara::data_start << data << Puara::data_end << std::endl;
}

void Puara::reboot_with_delay() {
    k_msleep(reboot_delay);
    sys_reboot(SYS_REBOOT_COLD);
}

void Puara::start_mdns_service(const char * device_name, const char * instance_name) {
    // //initialize mDNS service
    // esp_err_t err = mdns_init();
    // if (err) {
    //     std::cout << "MDNS Init failed: " << err << std::endl;
    //     return;
    // }
    // //set hostname
    // ESP_ERROR_CHECK(mdns_hostname_set(device_name));
    // //set default instance
    // ESP_ERROR_CHECK(mdns_instance_name_set(instance_name));
    // std::cout << "MDNS Init completed. Device name: " << device_name << "\n" << std::endl;
}

void Puara::start_mdns_service(std::string device_name, std::string instance_name) {
    // //initialize mDNS service
    // esp_err_t err = mdns_init();
    // if (err) {
    //     std::cout << "MDNS Init failed: " << err << std::endl;
    //     return;
    // }
    // //set hostname
    // ESP_ERROR_CHECK(mdns_hostname_set(device_name.c_str()));
    // //set default instance
    // ESP_ERROR_CHECK(mdns_instance_name_set(instance_name.c_str()));
    // std::cout << "MDNS Init completed. Device name: " << device_name << "\n" << std::endl;
}

void Puara::wifi_scan(void) {
    // uint16_t number = wifiScanSize;
    // wifi_ap_record_t ap_info[wifiScanSize];
    // uint16_t ap_count = 0;
    // memset(ap_info, 0, sizeof(ap_info));

    // esp_wifi_scan_start(NULL, true);
    // ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    // ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    // std::cout << "wifi_scan: Total APs scanned = " << ap_count << std::endl;
    // wifiAvailableSsid.clear();
    // for (int i = 0; (i < wifiScanSize) && (i < ap_count); i++) {
    //     wifiAvailableSsid.append("<strong>SSID: </strong>");
    //     wifiAvailableSsid.append(reinterpret_cast<const char*>(ap_info[i].ssid));
    //     wifiAvailableSsid.append("<br>      (RSSI: ");
    //     wifiAvailableSsid.append(std::to_string(ap_info[i].rssi));
    //     wifiAvailableSsid.append(", Channel: ");
    //     wifiAvailableSsid.append(std::to_string(ap_info[i].primary));
    //     wifiAvailableSsid.append(")<br>");
    // }
}

std::string Puara::urlDecode(std::string text) {
      
    std::string escaped;
    for (auto i = text.begin(), nd = text.end(); i < nd; ++i){
      auto c = ( *i );
       switch(c) {
        case '%':
          if (i[1] && i[2]) {
              char hs[]{ i[1], i[2] };
              escaped += static_cast<char>(strtol(hs, nullptr, 16));
              i += 2;
          }
          break;
        case '+':
          escaped += ' ';
          break;
        default:
          escaped += c;
      }
    }
    return escaped;
}

bool Puara::get_StaIsConnected() {
    StaIsConnected = wifi_enabled;
    return StaIsConnected;
}

int Puara::puara_variable_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    const char *next;
    int rc;

    for (auto& var : variables) {
        if (settings_name_steq(name, var.name.c_str(), &next) && !next) {
            if (len > var.size) {
                printk("Length %d\n", len);
                return -EINVAL;
            }

            rc = read_cb(cb_arg, var.value, var.size);
            if (rc >= 0) {
                return 0;
            }

            return rc;
        }
    }

}

int Puara::puara_config_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    const char *next;
    int rc;
    if (settings_name_steq(name, "DeviceName", &next) && !next) {
        if (len > (sizeof(device) - 1)) {
            printk("Length %d\n", len);
            return -EINVAL;
        }

        rc = read_cb(cb_arg, &device, sizeof(device));
        if (rc >= 0) {
            return 0;
        }

        return rc;
    }
    if (settings_name_steq(name, "DeviceID", &next) && !next) {
        if (len != sizeof(id)) {
            return -EINVAL;
        }

        rc = read_cb(cb_arg, &id, sizeof(id));
        if (rc >= 0) {
            return 0;
        }

        return rc;
    }
    if (settings_name_steq(name, "SSID", &next) && !next) {
        if (len > (sizeof(wifiSSID) - 1)) {
            printk("Length %d\n", len);
            return -EINVAL;
        }

        rc = read_cb(cb_arg, &wifiSSID, sizeof(wifiSSID));
        if (rc >= 0) {
            return 0;
        }

        return rc;
    }

    if (settings_name_steq(name, "password", &next) && !next) {
        if (len > sizeof(wifiPSK)) {
            printk("Length %d\n", len);
            return -EINVAL;
        }

        rc = read_cb(cb_arg, &wifiPSK, sizeof(wifiPSK));
        if (rc >= 0) {
            return 0;
        }

        return rc;
    }


    if (settings_name_steq(name, "APpasswd", &next) && !next) {
        if (len > (sizeof(APpasswd)-1)) {
            printk("Length %d\n", len);
            return -EINVAL;
        }

        rc = read_cb(cb_arg, &APpasswd, sizeof(APpasswd));
        if (rc >= 0) {
            return 0;
        }

        return rc;
    }

    if (settings_name_steq(name, "oscIP1", &next) && !next) {
        if (len > (sizeof(oscIP1) - 1)) {
            return -EINVAL;
        }

        rc = read_cb(cb_arg, &oscIP1, sizeof(oscIP1));
        if (rc >= 0) {
            return 0;
        }

        return rc;
    }

    if (settings_name_steq(name, "oscIP2", &next) && !next) {
        if (len > (sizeof(oscIP2) - 1)) {
            return -EINVAL;
        }

        rc = read_cb(cb_arg, &oscIP2, sizeof(oscIP2));
        if (rc >= 0) {
            return 0;
        }

        return rc;
    }

    if (settings_name_steq(name, "oscPORT1", &next) && !next) {
        if (len != sizeof(oscPORT1)) {
            return -EINVAL;
        }

        rc = read_cb(cb_arg, &oscPORT1, sizeof(oscPORT1));
        if (rc >= 0) {
            return 0;
        }

        return rc;
    }

    if (settings_name_steq(name, "oscPORT2", &next) && !next) {
        if (len != sizeof(oscPORT2)) {
            return -EINVAL;
        }

        rc = read_cb(cb_arg, &oscPORT2, sizeof(oscPORT2));
        if (rc >= 0) {
            return 0;
        }

        return rc;
    }

    return -ENOENT;
}

int Puara::saveVar(std::string varName, void* varValue, size_t var_len) {
    // Save value
    config_str = "sensor/";
    config_str.append(varName);

    // Update value in runtime
    int storage_key = variables_fields.at(varName);
    int err = settings_save_one(config_str.c_str(), varValue, var_len);

    // If error print to log
    if (err != 0) {
        LOG_ERR("Failed to save variable <%s>.", varName.c_str());
    }

    // Update variable
    if (var_len < variables[storage_key].size) {
        memcpy(&variables[storage_key].value, varValue, var_len);
    } else {
        LOG_ERR("Failed to update variable <%s>.", varName.c_str());
    }

    return 0;
}

int Puara::saveConfig(std::string varName, int varValue) {
    // Save value
    config_str = "config/";
    config_str.append(varName);

    // Update value in runtime
    int storage_key = config_fields.at(varName);
    switch (storage_key)
    { 
        case puara_keys::OSC_PORT1:
            oscPORT1 = varValue;
            settings_save_one(config_str.c_str(), &oscPORT1, sizeof(oscPORT1));
            break;
        case puara_keys::OSC_PORT2:
            // If I'm setting the osc ports make sure those are Numbers
            oscPORT2 = varValue;
            settings_save_one(config_str.c_str(), &oscPORT2, sizeof(oscPORT2));
            break;
        case puara_keys::DEVICE_ID:
            id = varValue;
            settings_save_one(config_str.c_str(), &id, sizeof(id));
            break; 
        default:
            return -1;
    }
    return 0;
}
int Puara::saveConfig(std::string varName, const char *varValue) {
    int storage_key = config_fields.at(varName);
    config_str = "config/";
    config_str.append(varName);

    // Save value
    switch (storage_key)
    { 
        case puara_keys::SSID:
            strcpy(wifiSSID, varValue);
            settings_save_one(config_str.c_str(), &wifiSSID, strlen(wifiSSID));
            break;
        case puara_keys::PASSWORD:
            strcpy(wifiPSK,varValue);
            settings_save_one(config_str.c_str(), &wifiPSK, strlen(wifiPSK));
            break;
        case puara_keys::AP_PASSWORD:
            strcpy(APpasswd,varValue);
            settings_save_one(config_str.c_str(), &APpasswd, strlen(APpasswd));
            break;
        case puara_keys::OSC_IP1:
            strcpy(oscIP1,varValue);
            settings_save_one(config_str.c_str(), &oscIP1, strlen(oscIP1));
            break;
        case puara_keys::OSC_IP2:
            strcpy(oscIP2,varValue);
            settings_save_one(config_str.c_str(), &oscIP2, strlen(oscIP2));
            break;
        case puara_keys::DEVICE_NAME:
            strcpy(device,varValue);
            settings_save_one(config_str.c_str(), &device, strlen(device));
            break;
        default:
            return -1;
    }
    return 0;
}

int Puara::set(settingsVariables var) {
    int ret;
    int storage_key = config_fields.at(var.name);

    switch (storage_key)
    { 
        // Text Settings
        case puara_keys::SSID:
        case puara_keys::PASSWORD:
        case puara_keys::AP_PASSWORD:
        case puara_keys::OSC_IP1:
        case puara_keys::OSC_IP2:
        case puara_keys::DEVICE_NAME:
            ret = saveConfig(var.name, var.textValue.c_str());
            if (ret != 0) {
                return -1;
            }
            break;
        // Numerical Settings
        case puara_keys::OSC_PORT1:
        case puara_keys::OSC_PORT2:
        case puara_keys::DEVICE_ID:
            ret = saveConfig(var.name, var.numberValue);
            if (ret != 0) {
                return -1;
            }
            break;
        default:
            return -1; 
            break;
    }
    return 0;
}

int Puara::get(settingsVariables *var) {
    int storage_key = config_fields.at(var->name);

    switch (storage_key)
    { 
        case puara_keys::SSID:
            var->textValue = wifiSSID;
            break;
        case puara_keys::PASSWORD:
            var->textValue = wifiPSK;
            break;
        case puara_keys::AP_PASSWORD:
            var->textValue = APpasswd;
            break;
        case puara_keys::OSC_IP1:
            var->textValue = oscIP1;
            break;
        case puara_keys::OSC_IP2:
            var->textValue = oscIP2;
            break;
        case puara_keys::DEVICE_NAME:
            var->textValue = device;
            break;
        case puara_keys::OSC_PORT1:
            var->textValue = std::to_string(oscPORT1);
            break;
        case puara_keys::OSC_PORT2:
            var->textValue = std::to_string(oscPORT2);
            break;
        case puara_keys::ENABLE_LIBMAPPER:
            if (enableLibmapper) {
                var->textValue = "ENABLED";
            } else {
                var->textValue = "DISABLED";
            }
            break;
        case puara_keys::DEVICE_ID:
            var->textValue = std::to_string(id);
            break;
        default:
            var->textValue = "NULL";
            return -1;
            break;
    }
    return 0;
}

std::vector<puara_parent_settings> Puara::getSensorSettings() {
    return parent_variables;
}

int Puara::getVar(std::string varName, void* var, size_t len) {
    // Check if key is valid
    if (variables_fields.find(varName) == variables_fields.end()) {
        LOG_ERR("Failed to retrieve variable <%s>. Variable not found in variable fields", varName.c_str());
        return -1;
    }
    // Get child variable
    puara_child_settings temp = variables.at(variables_fields.at(varName));

    // Copy value to
    if ((var != NULL) && (len <= temp.size)) {
        memcpy(var, temp.value, len);
    } else {
        LOG_ERR("Failed to retrieve variable <%s>. Variable size requested %d is larger than stored variable size %d", varName.c_str(), len, temp.size);
        return -1;
    }

    // Return size of the variable
    return len;
}

std::string Puara::getIP1() {
    return oscIP1;
}

std::string Puara::getIP2() {
    return oscIP2;
}

int unsigned Puara::getPORT1() {
    return oscPORT1;
}

int unsigned Puara::getPORT2() {
    return oscPORT2;
}

std::string Puara::getPORT1Str() {
    return std::to_string(oscPORT1);
}

std::string Puara::getPORT2Str() {
    return std::to_string(oscPORT2);
}

int unsigned Puara::getLocalPORT() {
    return localPORT;
}

std::string Puara::getLocalPORTStr() {
    return std::to_string(localPORT);
}

bool Puara::IP1_ready() {
    if (!strcmp(oscIP1, "0.0.0.0") || !strcmp(oscIP1,"")) {
        return false;
    } else {
        return true;
    }
}

bool Puara::IP2_ready() {
    if (!strcmp(oscIP2,"0.0.0.0") || !strcmp(oscIP2,"")) {
        return false;
    } else {
        return true;
    }
}

// Wifi handlers
void Puara::wifi_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
			       struct net_if *iface)
{
	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT: {
		LOG_INF("Connected to %s", wifiSSID);
        wifi_enabled = true;
		break;
	}
	case NET_EVENT_WIFI_DISCONNECT_RESULT: {
		LOG_INF("Disconnected from %s", wifiSSID);
        wifi_enabled = false;
		break;
	}
	case NET_EVENT_WIFI_AP_ENABLE_RESULT: {
		LOG_INF("AP Mode is enabled. Waiting for station to connect");
        wifi_enabled = false;
		break;
	}
	case NET_EVENT_WIFI_AP_DISABLE_RESULT: {
		LOG_INF("AP Mode is disabled.");
        ap_enabled = false;
		break;
	}
	case NET_EVENT_WIFI_AP_STA_CONNECTED: {
		struct wifi_ap_sta_info *sta_info = (struct wifi_ap_sta_info *)cb->info;

		LOG_INF("station: " MACSTR " joined ", sta_info->mac[0], sta_info->mac[1],
			sta_info->mac[2], sta_info->mac[3], sta_info->mac[4], sta_info->mac[5]);
		break;
	}
	case NET_EVENT_WIFI_AP_STA_DISCONNECTED: {
		struct wifi_ap_sta_info *sta_info = (struct wifi_ap_sta_info *)cb->info;

		LOG_INF("station: " MACSTR " leave ", sta_info->mac[0], sta_info->mac[1],
			sta_info->mac[2], sta_info->mac[3], sta_info->mac[4], sta_info->mac[5]);
		break;
	}
	default:
		break;
	}
}
void Puara::enable_dhcpv4_server(void)
{
	static struct in_addr addr;
	static struct in_addr netmaskAddr;

	if (net_addr_pton(AF_INET, WIFI_AP_IP_ADDRESS, &addr)) {
		LOG_ERR("Invalid address: %s", WIFI_AP_IP_ADDRESS);
		return;
	}

	if (net_addr_pton(AF_INET, WIFI_AP_NETMASK, &netmaskAddr)) {
		LOG_ERR("Invalid netmask: %s", WIFI_AP_NETMASK);
		return;
	}

	net_if_ipv4_set_gw(ap_iface, &addr);

	if (net_if_ipv4_addr_add(ap_iface, &addr, NET_ADDR_MANUAL, 0) == NULL) {
		LOG_ERR("unable to set IP address for AP interface");
	}

	if (!net_if_ipv4_set_netmask_by_addr(ap_iface, &addr, &netmaskAddr)) {
		LOG_ERR("Unable to set netmask for AP interface: %s", WIFI_AP_NETMASK);
	}

	addr.s4_addr[3] += 10; /* Starting IPv4 address for DHCPv4 address pool. */

	if (net_dhcpv4_server_start(ap_iface, &addr) != 0) {
		LOG_ERR("DHCP server is not started for desired IP");
		return;
	}

	LOG_INF("DHCPv4 server started...\n");
}

// Create a version of the class
Puara puara_module;