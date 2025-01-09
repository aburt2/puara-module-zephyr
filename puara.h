//****************************************************************************//
// Puara Module Manager - WiFi and file system functions                      //
// Metalab - Société des Arts Technologiques (SAT)                            //
// Input Devices and Music Interaction Laboratory (IDMIL), McGill University  //
// Edu Meneses (2022) - https://www.edumeneses.com                            //
//****************************************************************************//

#ifndef PUARA_H
#define PUARA_H

#define PUARA_SERIAL_BUFSIZE 1024

#include <stdio.h>
#include <string>
#include <cstring>
#include <ostream>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <unordered_map>


// Need to be included for Zephyr Shell Commands
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/shell/shell.h>

// Needed for zephyr wifi connection
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/dhcpv4_server.h>

// Needed for settings
#include <zephyr/settings/settings.h>
#if defined(CONFIG_SETTINGS_FILE)
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#endif

#define STORAGE_PARTITION	storage_partition
#define STORAGE_PARTITION_ID	FIXED_PARTITION_ID(STORAGE_PARTITION)
#define PUARA_MAX_CONFIG_LENGTH 32

// Needed for power control
#include <zephyr/sys/reboot.h>

// Settings structure
struct settingsVariables {
    std::string name;
    std::string type;
    std::string textValue;
    double numberValue;
};

// MACROS for ease of use
enum puara_keys {
    SSID = 1,
    AP_PASSWORD = 2,
    OSC_IP1 = 4,
    OSC_PORT1 = 5,
    OSC_IP2 = 6,
    OSC_PORT2 = 7,
    PASSWORD = 8
};

// Wifi event handler
class Puara {
    private:
        unsigned int version;
        std::string dmiName = "puara";
        
        std::vector<settingsVariables> variables;
        std::unordered_map<std::string,int> variables_fields;

        std::unordered_map<std::string,int> config_fields = {
            {"SSID",1},
            {"APpasswd",2},
            {"APpasswdValidate",3},
            {"oscIP1",4},
            {"oscPORT1",5},
            {"oscIP2",6},
            {"oscPORT2",7},
            {"password",8},
            {"reboot",9},
            {"persistentAP",10},
            {"localPORT",11}
        };

        std::string device;
        unsigned int id;
        std::string author;
        std::string institution;
        static char APpasswd[PUARA_MAX_CONFIG_LENGTH];
        static char wifiSSID[PUARA_MAX_CONFIG_LENGTH];
        static char wifiPSK[PUARA_MAX_CONFIG_LENGTH];
        static char oscIP1[PUARA_MAX_CONFIG_LENGTH];
        static char oscIP2[PUARA_MAX_CONFIG_LENGTH];
        static unsigned int oscPORT1;
        static unsigned int oscPORT2;
        unsigned int localPORT;
        
        bool StaIsConnected;
        bool ApStarted;
        static bool persistentAP;
        std::string currentSSID;
        std::string currentSTA_IP;
        std::string currentSTA_MAC;
        std::string currentAP_IP;
        std::string currentAP_MAC;
        const int wifiScanSize = 20;
        std::string wifiAvailableSsid;
        
        // Wifi properties
        static bool wifi_enabled;
        static bool ap_enabled;
        struct net_if *sta_iface;
        struct net_if *ap_iface;
        struct net_mgmt_event_callback cb;
        wifi_connect_req_params wifi_config_sta;
        wifi_connect_req_params wifi_config_ap;
        const short int channel = 6;
        const short int max_connection = 5;
        const short int wifi_maximum_retry = 5;
        short int connect_counter;

        // Storage settings
        const bool spiffs_format_if_mount_failed = false;
        std::string config_str = "config/";
        static char tmp_setting[PUARA_MAX_CONFIG_LENGTH];

        // Wifi Setup
        void wifi_init();
        static void wifi_event_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event, struct net_if *iface);
        void enable_dhcpv4_server();

        // Storage handlers
        static int puara_config_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg);
        struct settings_handler puara_config = {
            .name = "config",
            .h_set = puara_config_set,
        };

        // Get JSON values
        void read_settings_json_internal(std::string& contents, bool merge=false);
        void read_config_json_internal(std::string& contents);
        void merge_settings_json(std::string& new_contents);

        // Webserver helpers
        std::string prepare_index();
        void find_and_replace(std::string old_text, std::string new_text, std::string &str);
        void find_and_replace(std::string old_text, double new_number, std::string &str);
        void find_and_replace(std::string old_text, unsigned int new_number, std::string &str);
        void checkmark(std::string old_text, bool value, std::string & str);

        // Reboot functions
        const int reboot_delay = 3000;
        std::string urlDecode(std::string text);
    
    public:
        // Monitor types
        enum Monitors {
            UART_MONITOR = 0,
            JTAG_MONITOR = 1,
            USB_MONITOR = 2
        };

        void start(Monitors monitor = UART_MONITOR); 
        
        int start_webserver(void);
        void stop_webserver(void);
        void start_wifi();
        std::string get_dmi_name();
        unsigned int get_version();
        void set_version(unsigned int user_version);
        std::string getIP1();
        std::string getIP2();
        int unsigned getPORT1();
        int unsigned getPORT2();
        std::string getPORT1Str();
        std::string getPORT2Str();
        int unsigned getLocalPORT();
        std::string getLocalPORTStr();
        void mount_spiffs();
        void unmount_spiffs();
        const std::string data_start = "<<<";
        const std::string data_end = ">>>";
        void read_config_json();
        void write_config_json();
        void read_settings_json();
        void write_settings_json();
        void send_serial_data(std::string data);

        // Wifi methods
        void start_mdns_service(const char * device_name, const char * instance_name);
        void start_mdns_service(std::string device_name, std::string instance_name);
        void wifi_scan();
        void sta_connect();
        void ap_connect();
        bool get_StaIsConnected();
        bool IP1_ready();
        bool IP2_ready();

        // Convertion method
        std::string convertToString(char* a);

        // Storage methods
        // Saving values
        int saveVar(std::string varName, double varValue);
        int saveVar(std::string varName, std::string varValue);
        int saveConfig(std::string varName, int varValue);
        int saveConfig(std::string varName, const char *varValue);
        
        // Retrieving values
        double getVarNumber (std::string varName);
        std::string getVarText(std::string varName);
        double getConfigNumber (std::string varName);
        std::string getConfigText(std::string varName);

        // Retrieving and sending config/settings data
        int set(settingsVariables var);
        int get(settingsVariables *var);

        // Setup storage
        void config_storage();

        // Reboot system
        void reboot_with_delay();

        // Set default monitor as UART
        int module_monitor = UART_MONITOR;
};

// instantiate the class to be used in main.cpp and puara_console.cpp
extern Puara puara_module;
#endif