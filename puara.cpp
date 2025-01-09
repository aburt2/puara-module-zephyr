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
unsigned int Puara::oscPORT1 = 8000;
unsigned int Puara::oscPORT2 = 8000;

unsigned int Puara::get_version() {
    return version;
};

void Puara::set_version(unsigned int user_version) {
    version = user_version;
};

void Puara::start(Monitors monitor) {
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
      
    // Setup storage
    settings_subsys_init();
    settings_register(&puara_config);
    settings_load();

    start_wifi();
    start_webserver();
    start_mdns_service(dmiName, dmiName);
    wifi_scan();

    module_monitor = monitor;
    
    // some delay added as start listening blocks the hw monitor
    std::cout << "Puara Start Done!\n\n  Type \"reboot\" in the serial monitor to reset the ESP32.\n\n";
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
    int ret = 0;
	ret = net_mgmt(NET_REQUEST_WIFI_AP_ENABLE, ap_iface, &wifi_config_ap,
			   sizeof(struct wifi_connect_req_params));
    
    // Enable DHCPV4 Server
    enable_dhcpv4_server();

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

    // Connect to wifi
    sta_connect();
}

void Puara::start_wifi() {

    ApStarted = false;

    // Check if wifiSSID is empty and wifiPSK have less than 8 characteres
    if (dmiName.empty() ) {
        std::cout << "start_wifi: Module name unpopulated. Using default name: Puara" << std::endl;
       dmiName = "Puara";
    }
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
    wifi_config_ap.ssid = (const uint8_t *)dmiName.c_str();
	wifi_config_ap.ssid_length = dmiName.length();
	wifi_config_ap.psk = (const uint8_t *)APpasswd;
	wifi_config_ap.psk_length = strlen(APpasswd);
	wifi_config_ap.channel = WIFI_CHANNEL_ANY;
	wifi_config_ap.band = WIFI_FREQ_BAND_2_4_GHZ;
    wifi_config_ap.bandwidth = WIFI_FREQ_BANDWIDTH_20MHZ;

    //Initialize Wifi
    std::cout << "startWifi: Starting WiFi config" << std::endl;
    Puara::connect_counter = 0;
    wifi_init();
    ApStarted = false;
}

void Puara::read_config_json() { // Deserialize
    
    // std::cout << "json: Mounting FS" << std::endl;
    // Puara::mount_spiffs();

    // std::cout << "json: Opening config json file" << std::endl;
    // FILE* f = fopen("/spiffs/config.json", "r");
    // if (f == NULL) {
    //     std::cout << "json: Failed to open file" << std::endl;
    //     return;
    // }

    // std::cout << "json: Reading json file" << std::endl;
    // std::ifstream in("/spiffs/config.json");
    // std::string contents((std::istreambuf_iterator<char>(in)), 
    // std::istreambuf_iterator<char>());

    // Puara::read_config_json_internal(contents);

    // fclose(f);
    // Puara::unmount_spiffs();
}

void Puara::read_config_json_internal(std::string& contents) {
    // std::cout << "json: Getting data" << std::endl;
    // cJSON *root = cJSON_Parse(contents.c_str());
    // if (cJSON_GetObjectItem(root, "device")) {
    //     Puara::device = cJSON_GetObjectItem(root,"device")->valuestring;
    // }
    // if (cJSON_GetObjectItem(root, "id")) {
    //     Puara::id = cJSON_GetObjectItem(root,"id")->valueint;
    // }
    // if (cJSON_GetObjectItem(root, "author")) {
    //     Puara::author = cJSON_GetObjectItem(root,"author")->valuestring;
    // }
    // if (cJSON_GetObjectItem(root, "institution")) {
    //     Puara::institution = cJSON_GetObjectItem(root,"institution")->valuestring;
    // }
    // if (cJSON_GetObjectItem(root, "APpasswd")) {
    //     Puara::APpasswd = cJSON_GetObjectItem(root,"APpasswd")->valuestring;
    // }
    // if (cJSON_GetObjectItem(root, "wifiSSID")) {
    //     Puara::wifiSSID = cJSON_GetObjectItem(root,"wifiSSID")->valuestring;
    // }
    // if (cJSON_GetObjectItem(root, "wifiPSK")) {
    //     Puara::wifiPSK = cJSON_GetObjectItem(root,"wifiPSK")->valuestring;
    // }
    // if (cJSON_GetObjectItem(root, "persistentAP")) {
    //     Puara::persistentAP = cJSON_GetObjectItem(root,"persistentAP")->valueint;
    // }
    // if (cJSON_GetObjectItem(root, "oscIP1")) {
    //     Puara::oscIP1 = cJSON_GetObjectItem(root,"oscIP1")->valuestring;
    // }
    // if (cJSON_GetObjectItem(root, "oscPORT1")) {
    //     Puara::oscPORT1 = cJSON_GetObjectItem(root,"oscPORT1")->valueint;
    // }
    // if (cJSON_GetObjectItem(root, "oscIP2")) {
    //     Puara::oscIP2 = cJSON_GetObjectItem(root,"oscIP2")->valuestring;
    // }
    // if (cJSON_GetObjectItem(root, "oscPORT2")) {
    //     Puara::oscPORT2 = cJSON_GetObjectItem(root,"oscPORT2")->valueint;
    // }
    // if (cJSON_GetObjectItem(root, "localPORT")) {
    //     Puara::localPORT = cJSON_GetObjectItem(root,"localPORT")->valueint;
    // }
    
    // std::cout << "\njson: Data collected:\n\n"
    // << "device: " << device << "\n"
    // << "id: " << id << "\n"
    // << "author: " << author << "\n"
    // << "institution: " << institution << "\n"
    // << "APpasswd: " << APpasswd << "\n"
    // << "wifiSSID: " << wifiSSID << "\n"
    // << "wifiPSK: " << wifiPSK << "\n"
    // << "persistentAP: " << persistentAP << "\n"
    // << "oscIP1: " << oscIP1 << "\n"
    // << "oscPORT1: " << oscPORT1 << "\n"
    // << "oscIP2: " << oscIP2 << "\n"
    // << "oscPORT2: " << oscPORT2 << "\n"
    // << "localPORT: " << localPORT << "\n"
    // << std::endl;
    
    // cJSON_Delete(root);

    // std::stringstream tempBuf;
    // tempBuf << Puara::device << "_" << std::setfill('0') << std::setw(3) << Puara::id;
    // Puara::dmiName = tempBuf.str();
    // printf("Device unique name defined: %s\n",dmiName.c_str());
}

void Puara::read_settings_json() {

    // std::cout << "json: Mounting FS" << std::endl;
    // Puara::mount_spiffs();

    // std::cout << "json: Opening settings json file" << std::endl;
    // FILE* f = fopen("/spiffs/settings.json", "r");
    // if (f == NULL) {
    //     std::cout << "json: Failed to open file" << std::endl;
    //     return;
    // }

    // std::cout << "json: Reading json file" << std::endl;
    // std::ifstream in("/spiffs/settings.json");
    // std::string contents((std::istreambuf_iterator<char>(in)), 
    // std::istreambuf_iterator<char>());

    // Puara::read_settings_json_internal(contents);
    // fclose(f);
    // Puara::unmount_spiffs();
}

void Puara::read_settings_json_internal(std::string& contents, bool merge) {
    // std::cout << "json: Getting data" << std::endl;
    // cJSON *root = cJSON_Parse(contents.c_str());
    // cJSON *setting = NULL;
    // cJSON *settings = NULL;

    // std::cout << "json: Parse settings information" << std::endl;
    // settings = cJSON_GetObjectItemCaseSensitive(root, "settings");
   
    // settingsVariables temp;
    // if (!merge) {
    //     variables.clear();
    // }
    // std::cout << "json: Extract info" << std::endl;
    // cJSON_ArrayForEach(setting, settings) {
    //     cJSON *name = cJSON_GetObjectItemCaseSensitive(setting, "name");
    //     cJSON *value = cJSON_GetObjectItemCaseSensitive(setting, "value");
    //     temp.name = name->valuestring;
    //     if (!cJSON_IsNumber(value)) {
    //         temp.textValue = value->valuestring;
    //         temp.type = "text";
    //         temp.numberValue = 0;
    //     } else {
    //         temp.textValue.empty();
    //         temp.numberValue = value->valuedouble;
    //         temp.type = "number";
    //     }
    //     if (variables_fields.find(temp.name) == variables_fields.end()) {
    //         variables_fields.insert({temp.name, variables.size()});
    //         variables.push_back(temp);
    //     } else {
    //         int variable_index = variables_fields.at(temp.name);
    //         variables.at(variable_index) = temp;
    //     }
    // }

    // // Print acquired data
    // std::cout << "\nModule-specific settings:\n\n";
    // for (auto it : variables) {
    //     std::cout << it.name << ": ";
    //     if (it.type == "text") {
    //         std::cout << it.textValue << "\n";
    //     } else if (it.type == "number") {
    //         std::cout << it.numberValue << "\n";
    //     }
    // }
    // std::cout << std::endl;
    
    // cJSON_Delete(root);
}


void Puara::write_config_json() {    
    // std::cout << "SPIFFS: Mounting FS" << std::endl;
    // Puara::mount_spiffs();

    // std::cout << "SPIFFS: Opening config.json file" << std::endl;
    // FILE* f = fopen("/spiffs/config.json", "w");
    // if (f == NULL) {
    //     std::cout << "SPIFFS: Failed to open config.json file" << std::endl;
    //     return;
    // }

    // cJSON *device_json = NULL;
    // cJSON *id_json = NULL;
    // cJSON *author_json = NULL;
    // cJSON *institution_json = NULL;
    // cJSON *APpasswd_json = NULL;
    // cJSON *wifiSSID_json = NULL;
    // cJSON *wifiPSK_json = NULL;
    // cJSON *persistentAP_json = NULL;
    // cJSON *oscIP1_json = NULL;
    // cJSON *oscPORT1_json = NULL;
    // cJSON *oscIP2_json = NULL;
    // cJSON *oscPORT2_json = NULL;
    // cJSON *localPORT_json = NULL;

    // cJSON *root = cJSON_CreateObject();

    // device_json = cJSON_CreateString(device.c_str());
    // cJSON_AddItemToObject(root, "device", device_json);
    
    // id_json = cJSON_CreateNumber(id);
    // cJSON_AddItemToObject(root, "id", id_json);
    
    // author_json = cJSON_CreateString(author.c_str());
    // cJSON_AddItemToObject(root, "author", author_json);
    
    // institution_json = cJSON_CreateString(institution.c_str());
    // cJSON_AddItemToObject(root, "institution", institution_json);
    
    // APpasswd_json = cJSON_CreateString(APpasswd.c_str());
    // cJSON_AddItemToObject(root, "APpasswd", APpasswd_json);
    
    // wifiSSID_json = cJSON_CreateString(wifiSSID.c_str());
    // cJSON_AddItemToObject(root, "wifiSSID", wifiSSID_json);
    
    // wifiPSK_json = cJSON_CreateString(wifiPSK.c_str());
    // cJSON_AddItemToObject(root, "wifiPSK", wifiPSK_json);

    // persistentAP_json = cJSON_CreateNumber(persistentAP);
    // cJSON_AddItemToObject(root, "persistentAP", persistentAP_json);
    
    // oscIP1_json = cJSON_CreateString(oscIP1.c_str());
    // cJSON_AddItemToObject(root, "oscIP1", oscIP1_json);
    
    // oscPORT1_json = cJSON_CreateNumber(oscPORT1);
    // cJSON_AddItemToObject(root, "oscPORT1", oscPORT1_json);
    
    // oscIP2_json = cJSON_CreateString(oscIP2.c_str());
    // cJSON_AddItemToObject(root, "oscIP2", oscIP2_json);
    
    // oscPORT2_json = cJSON_CreateNumber(oscPORT2);
    // cJSON_AddItemToObject(root, "oscPORT2", oscPORT2_json);
    
    // localPORT_json = cJSON_CreateNumber(localPORT);
    // cJSON_AddItemToObject(root, "localPORT", localPORT_json);

    // std::cout << "\njson: Data stored:\n"
    // << "\ndevice: " << device << "\n"
    // << "id: " << id << "\n"
    // << "author: " << author << "\n"
    // << "institution: " << institution << "\n"
    // << "APpasswd: " << APpasswd << "\n"
    // << "wifiSSID: " << wifiSSID << "\n"
    // << "wifiPSK: " << wifiPSK << "\n"
    // << "persistentAP: " << persistentAP << "\n"
    // << "oscIP1: " << oscIP1 << "\n"
    // << "oscPORT1: " << oscPORT1 << "\n"
    // << "oscIP2: " << oscIP2 << "\n"
    // << "oscPORT2: " << oscPORT2 << "\n"
    // << "localPORT: " << localPORT << "\n"
    // << std::endl;

    // // Save to config.json
    // std::cout << "write_config_json: Serializing json" << std::endl;
    // std::string contents = cJSON_Print(root);
    // std::cout << "SPIFFS: Saving file" << std::endl;
    // fprintf(f, "%s", contents.c_str());
    // std::cout << "SPIFFS: closing" << std::endl;
    // fclose(f);

    // std::cout << "write_config_json: Delete json entity" << std::endl;
    // cJSON_Delete(root);

    // std::cout << "SPIFFS: umounting FS" << std::endl;
    // Puara::unmount_spiffs();
}

void Puara::write_settings_json() {    
    // std::cout << "SPIFFS: Mounting FS" << std::endl;
    // Puara::mount_spiffs();

    // std::cout << "SPIFFS: Opening settings.json file" << std::endl;
    // FILE* f = fopen("/spiffs/settings.json", "w");
    // if (f == NULL) {
    //     std::cout << "SPIFFS: Failed to open settings.json file" << std::endl;
    //     return;
    // }

    // cJSON *root = cJSON_CreateObject();
    // cJSON *settings = cJSON_CreateArray();
    // cJSON *setting = NULL;
    // cJSON *data = NULL;
    // cJSON_AddItemToObject(root, "settings", settings);

    // for (auto it : variables) {
    //     setting = cJSON_CreateObject();
    //     cJSON_AddItemToArray(settings, setting);
    //     data = cJSON_CreateString(it.name.c_str());
    //     cJSON_AddItemToObject(setting, "name", data);
    //     if (it.type == "text") {
    //         data = cJSON_CreateString(it.textValue.c_str());
    //     } else if (it.type == "number") {
    //         data = cJSON_CreateNumber(it.numberValue);
    //     }
    //     cJSON_AddItemToObject(setting, "value", data);
    // }

    // // Save to settings.json
    // std::cout << "write_settings_json: Serializing json" << std::endl;
    // std::string contents = cJSON_Print(root);
    // std::cout << "SPIFFS: Saving file" << std::endl;
    // fprintf(f, "%s", contents.c_str());
    // std::cout << "SPIFFS: closing" << std::endl;
    // fclose(f);

    // std::cout << "write_settings_json: Delete json entity" << std::endl;
    // cJSON_Delete(root);

    // std::cout << "SPIFFS: umounting FS" << std::endl;
    // Puara::unmount_spiffs();
}

std::string Puara::get_dmi_name() {
    return dmiName;
}

// std::string Puara::prepare_index() {
//     Puara::mount_spiffs();
//     std::cout << "http (spiffs): Reading index file" << std::endl;
//     std::ifstream in("/spiffs/index.html");
//     std::string contents((std::istreambuf_iterator<char>(in)), 
//     std::istreambuf_iterator<char>());
//     // Put the module info on the HTML before send response
//     Puara::find_and_replace("%DMINAME%", Puara::dmiName, contents);
//     if (Puara::StaIsConnected) {
//         Puara::find_and_replace("%STATUS%", "Currently connected on "
//                                              "<strong style=\"color:Tomato;\">" + 
//                                              Puara::wifiSSID + "</strong> network", 
//                                              contents);
//     } else {
//         Puara::find_and_replace("%STATUS%", "Currently not connected to any network", 
//                                  contents);
//     }
//     Puara::find_and_replace("%CURRENTSSID%", Puara::currentSSID, contents);
//     Puara::find_and_replace("%CURRENTPSK%", Puara::wifiPSK, contents);
//     Puara::checkmark("%CURRENTPERSISTENT%", Puara::persistentAP, contents);
//     Puara::find_and_replace("%DEVICENAME%", Puara::device, contents);
//     Puara::find_and_replace("%CURRENTOSC1%", Puara::oscIP1, contents);
//     Puara::find_and_replace("%CURRENTPORT1%", Puara::oscPORT1, contents);
//     Puara::find_and_replace("%CURRENTOSC2%", Puara::oscIP2, contents);
//     Puara::find_and_replace("%CURRENTPORT2%", Puara::oscPORT2, contents);
//     Puara::find_and_replace("%CURRENTLOCALPORT%", Puara::localPORT, contents);
//     Puara::find_and_replace("%CURRENTSSID2%", Puara::wifiSSID, contents);
//     Puara::find_and_replace("%CURRENTIP%", Puara::currentSTA_IP, contents);
//     Puara::find_and_replace("%CURRENTAPIP%", Puara::currentAP_IP, contents);
//     Puara::find_and_replace("%CURRENTSTAMAC%", Puara::currentSTA_MAC, contents);
//     Puara::find_and_replace("%CURRENTAPMAC%", Puara::currentAP_MAC, contents);
//     std::ostringstream tempBuf;
//     tempBuf << std::setfill('0') << std::setw(3) << std::hex << Puara::id;
//     Puara::find_and_replace("%MODULEID%", tempBuf.str(), contents);
//     Puara::find_and_replace("%MODULEAUTH%", Puara::author, contents);
//     Puara::find_and_replace("%MODULEINST%", Puara::institution, contents);
//     Puara::find_and_replace("%MODULEVER%", Puara::version, contents);

//     Puara::unmount_spiffs();

//     return contents;
// }


// esp_err_t Puara::update_get_handler(httpd_req_t *req) {

//     const char* resp_str = (const char*) req->user_ctx;
//     Puara::mount_spiffs();
//     std::cout << "http (spiffs): Reading update.html file" << std::endl;
//     std::ifstream in(resp_str);
//     std::string contents((std::istreambuf_iterator<char>(in)), 
//     std::istreambuf_iterator<char>());
//     //httpd_resp_set_type(req, "text/html");
//     httpd_resp_sendstr(req, contents.c_str());
    
//     Puara::unmount_spiffs();

//     return ESP_OK;
// }

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
    // Puara::webserver = NULL;

    // Puara::webserver_config.task_priority      = tskIDLE_PRIORITY+5;
    // Puara::webserver_config.stack_size         = 4096;
    // Puara::webserver_config.core_id            = tskNO_AFFINITY;
    // Puara::webserver_config.server_port        = 80;
    // Puara::webserver_config.ctrl_port          = 32768;
    // Puara::webserver_config.max_open_sockets   = 7;
    // Puara::webserver_config.max_uri_handlers   = 9;
    // Puara::webserver_config.max_resp_headers   = 9;
    // Puara::webserver_config.backlog_conn       = 5;
    // Puara::webserver_config.lru_purge_enable   = true;
    // Puara::webserver_config.recv_wait_timeout  = 5;
    // Puara::webserver_config.send_wait_timeout  = 5;
    // Puara::webserver_config.global_user_ctx = NULL;
    // Puara::webserver_config.global_user_ctx_free_fn = NULL;
    // Puara::webserver_config.global_transport_ctx = NULL;
    // Puara::webserver_config.global_transport_ctx_free_fn = NULL;
    // Puara::webserver_config.open_fn = NULL;
    // Puara::webserver_config.close_fn = NULL;
    // Puara::webserver_config.uri_match_fn = NULL;

    // Puara::index.uri = "/";
    // Puara::index.method    = HTTP_GET,
    // Puara::index.handler   = index_get_handler,
    // Puara::index.user_ctx  = (char*)"/spiffs/index.html";

    // Puara::indexpost.uri = "/";
    // Puara::indexpost.method    = HTTP_POST,
    // Puara::indexpost.handler   = index_post_handler,
    // Puara::indexpost.user_ctx  = (char*)"/spiffs/index.html";

    // Puara::style.uri = "/style.css";
    // Puara::style.method    = HTTP_GET,
    // Puara::style.handler   = style_get_handler,
    // Puara::style.user_ctx  = (char*)"/spiffs/style.css";

    // // Puara::factory.uri = "/factory.html";
    // // Puara::factory.method    = HTTP_GET,
    // // Puara::factory.handler   = get_handler,
    // // Puara::factory.user_ctx  = (char*)"/spiffs/factory.html";

    // Puara::reboot.uri = "/reboot.html";
    // Puara::reboot.method    = HTTP_GET,
    // Puara::reboot.handler   = get_handler,
    // Puara::reboot.user_ctx  = (char*)"/spiffs/reboot.html";

    // Puara::scan.uri = "/scan.html";
    // Puara::scan.method    = HTTP_GET,
    // Puara::scan.handler   = scan_get_handler,
    // Puara::scan.user_ctx  = (char*)"/spiffs/scan.html";

    // // Puara::update.uri = "/update.html";
    // // Puara::update.method    = HTTP_GET,
    // // Puara::update.handler   = get_handler,
    // // Puara::update.user_ctx  = (char*)"/spiffs/update.html";

    // Puara::settings.uri = "/settings.html";
    // Puara::settings.method    = HTTP_GET,
    // Puara::settings.handler   = settings_get_handler,
    // Puara::settings.user_ctx  = (char*)"/spiffs/settings.html";

    // Puara::settingspost.uri = "/settings.html";
    // Puara::settingspost.method    = HTTP_POST,
    // Puara::settingspost.handler   = settings_post_handler,
    // Puara::settingspost.user_ctx  = (char*)"/spiffs/settings.html";

    // // Start the httpd server
    // std::cout << "webserver: Starting server on port: " << webserver_config.server_port << std::endl;
    // if (httpd_start(&webserver, &webserver_config) == ESP_OK) {
    //     // Set URI handlers
    //     std::cout << "webserver: Registering URI handlers" << std::endl;
    //     httpd_register_uri_handler(webserver, &index);
    //     httpd_register_uri_handler(webserver, &indexpost);
    //     httpd_register_uri_handler(webserver, &style);
    //     httpd_register_uri_handler(webserver, &scan);
    //     //httpd_register_uri_handler(webserver, &factory);
    //     httpd_register_uri_handler(webserver, &reboot);
    //     // httpd_register_uri_handler(webserver, &update);
    //     httpd_register_uri_handler(webserver, &settings);
    //     httpd_register_uri_handler(webserver, &settingspost);
    //     return webserver;
    // }

    // std::cout << "webserver: Error starting server!" << std::endl;
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
    StaIsConnected = wifi_enabled && !ap_enabled;
    return StaIsConnected;
}

int Puara::puara_config_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    const char *next;
    int rc;
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
        case puara_keys::SSID:
        case puara_keys::PASSWORD:
        case puara_keys::AP_PASSWORD:
        case puara_keys::OSC_IP1:
        case puara_keys::OSC_IP2:
            ret = saveConfig(var.name, var.textValue.c_str());
            if (ret) {
                return -1;
            }
            break;
        case puara_keys::OSC_PORT1:
        case puara_keys::OSC_PORT2:
            // If I'm setting the osc ports make sure those are Numbers
            ret = saveConfig(var.name, var.numberValue);
            if (ret) {
                return -1;
            }
            break;
        default:
            ret = saveConfig(var.name, var.textValue.c_str());
            if (ret) {
                return -1;
            }
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
        case puara_keys::OSC_PORT1:
            var->textValue = std::to_string(oscPORT1);
            break;
        case puara_keys::OSC_PORT2:
            var->textValue = std::to_string(oscPORT2);
            break;
        default:
            var->textValue = "NULL";
            break;
    }
    return 0;
}

double Puara::getVarNumber(std::string varName) {
    return variables.at(variables_fields.at(varName)).numberValue;
}
        
std::string Puara::getVarText(std::string varName) {
    return variables.at(variables_fields.at(varName)).textValue;
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
void Puara::wifi_event_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event,
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