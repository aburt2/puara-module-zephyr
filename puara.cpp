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

LOG_MODULE_REGISTER(PUARA_MODULE);

#define MACSTR "%02X:%02X:%02X:%02X:%02X:%02X"

// WiFi Event Masks
#define NET_EVENT_WIFI_MASK                                                                        \
	(NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT |                        \
	 NET_EVENT_WIFI_AP_ENABLE_RESULT | NET_EVENT_WIFI_AP_DISABLE_RESULT |                      \
	 NET_EVENT_WIFI_AP_STA_CONNECTED | NET_EVENT_WIFI_AP_STA_DISCONNECTED)
#define PUARA_WIFI_SCAN_EVENTS (                   \
				NET_EVENT_WIFI_SCAN_RESULT        |\
				NET_EVENT_WIFI_SCAN_DONE          |\
				NET_EVENT_WIFI_RAW_SCAN_RESULT)


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
puara_device_settings Puara::device_config = {};
int Puara::http_service_port = 80;
std::string Puara::wifiAvailableSsid;
net_mgmt_event_callback Puara::wifi_scan_cb;

unsigned int Puara::get_version() {
    return version;
};

void Puara::set_version(unsigned int user_version) {
    version = user_version;
};

void Puara::start(std::vector<puara_parent_settings> sensor_settings, Monitors monitor) {
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

    // Configure filesystem for storing variables
    configure_storage(sensor_settings);
      
    // Setup storage
    settings_subsys_init();
    settings_register(&puara_config); // register settings
    settings_register(&puara_variables); // register variables
    settings_load();

    // Setup WiFi
    start_wifi();
    // start_webserver();

    // Setup serial monitor
    module_monitor = monitor;

    // some delay added as start listening blocks the hw monitor
    std::cout << "Puara Start Done!\n\n  Type \"puara reboot\" in the serial monitor to reset the controller.\n\n";
}

void Puara::configure_storage(std::vector<puara_parent_settings> sensor_settings) {
    // Get variables for the device
    if (sensor_settings.size() > 0) {
        for (auto parent_temp: sensor_settings) {
            parent_variables.push_back(parent_temp);

            // Get every child variable in the parent variable structure
            if (parent_temp.count > 0) {
                for (int i = 0; i < parent_temp.count; i++) {
                    puara_child_settings temp = parent_temp.nested_settings[i];
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

    // Set up config structure
    std::vector<std::string> var_names = {"SSID", "APpasswd", "oscIP1", "oscPORT1", "oscIP2", "oscPORT2", "enableLibmapper", "password", "DeviceName", "DeviceID"};
    int idx = 0;
    int ret = 0;
    for (auto varName: var_names) {
        // Setup dummy variable and add the variable name
        settingsVariables var;
        var.name = varName;

        // Get variable from storage
        ret = get(&var);
        if (ret == 0) {
            // Save variable to device settings
            device_config.settings[idx] = var;

            // Update index
            idx++;
        } else {
            LOG_ERR("Error in getting variable: %s", var.name.c_str());
        }
    }

    // Update number of device settings
    device_config.settings_len = idx + 1;
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

    // Initialise event callback for wifi scan results
    net_mgmt_init_event_callback(&wifi_scan_cb, wifi_mgmt_scan_event_handler, PUARA_WIFI_SCAN_EVENTS);

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

    // Set wifi and ap enabled to false
    wifi_enabled = false;
    ap_enabled = false;

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
    wifi_config_sta.timeout = PUARA_WIFI_CONNECTION_TIMEOUT;

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

int Puara::start_webserver(void) {    
    // Start webserver
    http_server_start();

    return 0;
}

void Puara::stop_webserver(void) {
    // Stop the httpd server
    http_server_stop();
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

void Puara::wifi_scan(void) {
	struct wifi_scan_params params;
    net_mgmt_add_event_callback(&wifi_scan_cb);

    // Reset availableSSID string
    wifiAvailableSsid = "";

    if (net_mgmt(NET_REQUEST_WIFI_SCAN, sta_iface, &params, sizeof(params))) {
        LOG_WRN("Scan request failed\n");
    }

    LOG_INF("Scan requested\n");
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
            var->numberValue = oscPORT1;
            break;
        case puara_keys::OSC_PORT2:
            var->textValue = std::to_string(oscPORT2);
            var->numberValue = oscPORT2;
            break;
        case puara_keys::ENABLE_LIBMAPPER:
            if (enableLibmapper) {
                var->textValue = "ENABLED";
            } else {
                var->textValue = "DISABLED";
            }
            var->numberValue = enableLibmapper;
            break;
        case puara_keys::DEVICE_ID:
            var->textValue = std::to_string(id);
            var->numberValue = id;
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

bool Puara::libmapper_ready() {
    return enableLibmapper;
}

// Wifi handlers
void Puara::wifi_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
			       struct net_if *iface)
{
	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT: {
        struct wifi_status *connect_status = (struct wifi_status *)cb->info;

        // Print log message depending on connection status
        switch (connect_status->conn_status)
        {
        case WIFI_STATUS_CONN_SUCCESS:
            LOG_INF("Connected to %s", wifiSSID);
            wifi_enabled = true;
            break;
        case WIFI_STATUS_CONN_WRONG_PASSWORD:
            LOG_INF("Failed to connect to %s. Incorrect Password", wifiSSID);
            wifi_enabled = false;
            break;
        case WIFI_STATUS_CONN_TIMEOUT:
            LOG_INF("Failed to connect to %s. Connection Timeout", wifiSSID);
            wifi_enabled = false;
            break;
        case WIFI_STATUS_CONN_AP_NOT_FOUND:
            LOG_INF("Failed to connect to %s. Access point not found", wifiSSID);
            wifi_enabled = false;
            break;
        case WIFI_STATUS_CONN_FAIL:
        default:
            LOG_INF("Failed to connect to %s", wifiSSID);
            wifi_enabled = false;
            break;
        }
		break;
	}
	case NET_EVENT_WIFI_DISCONNECT_RESULT: {
		LOG_INF("Disconnected from %s", wifiSSID);
        wifi_enabled = false;
		break;
	}
	case NET_EVENT_WIFI_AP_ENABLE_RESULT: {
		LOG_INF("AP Mode is enabled. Waiting for station to connect");
        ap_enabled = true;
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

void Puara::wifi_mgmt_scan_event_handler(struct net_mgmt_event_callback *cb,
					 uint64_t mgmt_event, struct net_if *iface)
{
	switch (mgmt_event) {
	case NET_EVENT_WIFI_SCAN_RESULT:
		handle_wifi_scan_result(cb);
		break;
	case NET_EVENT_WIFI_SCAN_DONE:
		handle_wifi_scan_done(cb);
		break;
	default:
		break;
	}
}

void Puara::handle_wifi_scan_done(struct net_mgmt_event_callback *cb)
{
	const struct wifi_status *status =
    (const struct wifi_status *)cb->info;

	if (status->status) {
		LOG_WRN("Scan request failed (%d)\n", status->status);
	} else {
		LOG_DBG("Scan request done\n");
	}

	net_mgmt_del_event_callback(&wifi_scan_cb);
}

void Puara::handle_wifi_scan_result(struct net_mgmt_event_callback *cb)
{
	const struct wifi_scan_result *entry =
		(const struct wifi_scan_result *)cb->info;
	uint8_t mac_string_buf[sizeof("xx:xx:xx:xx:xx:xx")];

    // Add result to available SSIDs
    wifiAvailableSsid.append("<strong>SSID: </strong>");
    wifiAvailableSsid.append(reinterpret_cast<const char*>(entry->ssid));
    wifiAvailableSsid.append("<br>      (RSSI: ");
    wifiAvailableSsid.append(std::to_string(entry->rssi));
    wifiAvailableSsid.append(", Channel: ");
    wifiAvailableSsid.append(std::to_string(entry->channel));
    wifiAvailableSsid.append(")<br>");
}



void Puara::enable_dhcpv4_server(void)
{
	static struct net_in_addr addr;
	static struct net_in_addr netmaskAddr;

	if (net_addr_pton(NET_AF_INET, WIFI_AP_IP_ADDRESS, &addr)) {
		LOG_ERR("Invalid address: %s", WIFI_AP_IP_ADDRESS);
		return;
	}

	if (net_addr_pton(NET_AF_INET, WIFI_AP_NETMASK, &netmaskAddr)) {
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