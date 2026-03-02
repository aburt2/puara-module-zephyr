// Shell Commands for puara module
#include "puara.h"
#include <iostream>
#include <zephyr/shell/shell.h>

// Declare Shell commands
static int cmd_reboot_device(const struct shell *sh, size_t argc, char **argv, uint32_t period);
static int cmd_puara_ping(const struct shell *sh, size_t argc, char **argv, uint32_t period);
static int cmd_whoareyou(const struct shell *sh, size_t argc, char **argv, uint32_t period);

// Config settings
// TODO: replace with 3 commands, read/send/write and use options to decide between config or settings
static int cmd_set(const struct shell *sh, size_t argc, char **argv, uint32_t period);
static int cmd_get(const struct shell *sh, size_t argc, char **argv, uint32_t period);
static int parse_setting_args_set(const struct shell *sh, size_t argc, char *argv[], settingsVariables *var);
static int parse_setting_args_get(const struct shell *sh, size_t argc, char *argv[], std::vector<std::string> *var_names);

// Define shell commands
static int cmd_reboot_device(const struct shell *sh, size_t argc, char **argv, uint32_t period) {
    puara_module.reboot_with_delay();
    return 0;
}

static int cmd_puara_ping(const struct shell *sh, size_t argc, char **argv, uint32_t period) {
    std::cout << "pong" << std::endl;
    return 0;
}

static int cmd_whoareyou(const struct shell *sh, size_t argc, char **argv, uint32_t period) {
    std::cout << puara_module.get_dmi_name() << std::endl;
    return 0;
}

static int parse_setting_args_set(const struct shell *sh, size_t argc, char *argv[], settingsVariables *var) {
    // Options setup
	int opt;
	int opt_index = 0;
    struct sys_getopt_state *state;
    static const struct sys_getopt_option set_options[] = {
        {"name", sys_getopt_required_argument, 0, 'n'},
        {"id", sys_getopt_required_argument, 0, 'd'},
		{"ssid", sys_getopt_required_argument, 0, 's'},
		{"psk", sys_getopt_required_argument, 0, 'p'},
		{"apsk", sys_getopt_required_argument, 0, 'a'},
		{"oscip1", sys_getopt_required_argument, 0, 'i'},
		{"oscip2", sys_getopt_required_argument, 0, 'o'},
		{"port1", sys_getopt_required_argument, 0, 'r'},
		{"port2", sys_getopt_required_argument, 0, 't'},
        {"enableLibmapper", sys_getopt_required_argument, 0, 'l'},
		{0, 0, 0, 0}
    };

    // Check that I've submitted an argument


    while ((opt = sys_getopt_long(argc, argv, "n:d:s:p:a:i:o:r:t:l:", set_options, &opt_index)) != -1) {
        state = sys_getopt_state_get();
        switch (opt) {
        case 'n':
                var->name = "DeviceName";
                var->textValue = state->optarg;
                break;
        case 'd':
                var->name = "DeviceID";
                var->numberValue = atoi(state->optarg);
                break;
        case 's':
                var->name = "SSID";
                var->textValue = state->optarg;
                break;
        case 'p':
                var->name = "password";
                var->textValue = state->optarg;
                break;
        case 'a':
                var->name = "APpasswd";
                var->textValue = state->optarg;
                break;
        case 'i':
                var->name = "oscIP1";
                var->textValue = state->optarg;
                break;
        case 'o':
                var->name = "oscIP2";
                var->textValue = state->optarg;
                break;
        case 'r':
                var->name = "oscPORT1";
                var->numberValue = atoi(state->optarg);
                break;
        case 't':
                var->name = "oscPORT2";
                var->numberValue = atoi(state->optarg);
                break;
        case 'l':
                var->name = "enableLibmapper";
                var->numberValue = atoi(state->optarg);
                break;
        default:
                shell_error(sh, "Invalid option %c\n", state->optopt);
                return -EINVAL;
        }
    }

    // Return 0 when done
    return 0;
}   

static int parse_setting_args_get(const struct shell *sh, size_t argc, char *argv[], std::vector<std::string> *var_names) {
    // Options setup
	int opt;
	int opt_index = 0;
    struct sys_getopt_state *state;
    static const struct sys_getopt_option get_options[] = {
        {"name", sys_getopt_no_argument, 0, 'n'},
        {"wifi", sys_getopt_no_argument, 0, 'w'},
		{"osc", sys_getopt_no_argument, 0, 'o'},
		{"sensor", sys_getopt_no_argument, 0, 's'},
		{0, 0, 0, 0}
    };

    while ((opt = sys_getopt_long(argc, argv, "nwos", get_options, &opt_index)) != -1) {
        state = sys_getopt_state_get();
        switch (opt) {
        case 'n':
                var_names->push_back("DeviceName");
                var_names->push_back("DeviceID");
                break;
        case 'w':
                var_names->push_back("SSID");
                var_names->push_back("password");
                var_names->push_back("APpasswd");
                var_names->push_back("enableLibmapper");
                break;
        case 'o':
                var_names->push_back("oscIP1");
                var_names->push_back("oscPORT1");
                var_names->push_back("oscIP2");
                var_names->push_back("oscPORT2");
                break;
        case 's':
                var_names->push_back("sensor");
                break;
        default:
                shell_error(sh, "Invalid option %c\n", state->optopt);
                return -EINVAL;
        }
    }

    // Return 0 when done
    return 0;
}   


static int cmd_set(const struct shell *sh, size_t argc, char **argv, uint32_t period) {
    settingsVariables var;
    int ret = 0;
    if (parse_setting_args_set(sh, argc, argv, &var) != 0) {
        shell_help(sh);
        return -EINVAL;
    }

    // Check if there is a variable at all
    if (var.name.empty()) {
        // If the variable is empty it means nothing was provided
        shell_error(sh, "No setting was specified");
        shell_help(sh);
        return -EINVAL;
    }

    ret = puara_module.set(var);

    if (ret != 0) {
        shell_error(sh, "Error in saving variable: %s", var.name.c_str());
        shell_help(sh);
        return -EINVAL;
    }

    shell_info(sh, "Successfully saved variable");
    return 0;
}

static int cmd_get(const struct shell *sh, size_t argc, char **argv, uint32_t period) {
    std::vector<std::string> var_names;
    settingsVariables var;
    std::vector<puara_parent_settings> sensor_settings;
    int ret = 0;
    if (parse_setting_args_get(sh, argc, argv, &var_names) != 0) {
        shell_help(sh);
        return -EINVAL;
    }

    // Check if there is a variable at all
    if (var_names.empty()) {
        // If the variable is empty it means nothing was provided
        shell_error(sh, "No setting was specified");
        shell_help(sh);
        return -EINVAL;
    }

    for (auto varName: var_names) {
        if (varName == "sensor") {
            sensor_settings = puara_module.getSensorSettings();
            for (auto sensor: sensor_settings) {
                shell_info(sh, "\n%s Settings", sensor.name.c_str());
                for (int i = 0; i < sensor.count; i++) {
                    puara_child_settings setting = sensor.nested_settings[i];
                    if (setting.type == "text") {
                        char temp[PUARA_MAX_CONFIG_LENGTH];
                        puara_module.getVar(setting.name, &temp, setting.size);
                        shell_info(sh, "%s: %s", setting.name.c_str(), temp);
                    } else if (setting.type == "int") {
                        int temp = 0;
                        puara_module.getVar(setting.name, &temp, setting.size);
                        shell_info(sh, "%s: %d", setting.name.c_str(), temp);
                    } else if (setting.type == "float") {
                        float temp = 0;
                        puara_module.getVar(setting.name, &temp, setting.size);
                        shell_info(sh, "%s: %.2f", setting.name.c_str(), temp);
                    } else if (setting.type == "array") {
                        float temp[PUARA_MAX_ARRAY_SIZE];
                        puara_module.getVar(setting.name, &temp, setting.size);

                        // Compute size of array
                        size_t array_size = setting.size / sizeof(float);
                        std::string array_str = "[";
                        
                        // Add numbers to string
                        for (size_t i = 0; i < array_size; i++) {
                            std::stringstream stream;
                            stream << std::fixed << std::setprecision(2) << temp[i];
                            array_str.append(stream.str());
                            if (i+1 < array_size) {
                                array_str.append(",");
                            }
                        }
                        
                        // Add closing bracked to 
                        array_str.append("]");

                        // Print out setting
                        shell_info(sh, "%s: %s", setting.name.c_str(), array_str.c_str());
                    }
                }
            }
        } else {
            var.name = varName;
            ret = puara_module.get(&var);

            if (ret != 0) {
                shell_error(sh, "Error in getting variable: %s", var.name.c_str());
                return -EINVAL;
            }

            // Print output of cmd to shell
            shell_info(sh, "%s: %s", var.name.c_str(), var.textValue.c_str());

            // Reset variable
            var.name = "";
            var.textValue = "";
        }
    }
    return 0;
}

// Create sub commands for puara shell
SHELL_STATIC_SUBCMD_SET_CREATE(sub_puara_commands,
        SHELL_CMD(reboot, NULL, "Reboot device", cmd_reboot_device),
        SHELL_CMD(ping, NULL, "Ping command.", cmd_puara_ping),
        SHELL_CMD(whoareyou, NULL, "Returns device name.", cmd_whoareyou),
        SHELL_CMD_ARG(set, NULL, "Set device setting\n"
                                "[-n --name]: Device Name\n"
                                "[-d --id]: Device ID\n"
                                "[-s --ssid]: SSID.\n"
                                "[-p, --psk]: SSID Password (valid only for secure SSIDs)\n"
                                "[-a, --apsk]: AP Password\n"
                                "[-i, --oscip1]: OSC IP address 1\n"
                                "[-o, --oscip2]: OSC IP address 2\n"
                                "[-r, --port1]: OSC port for IP address 1\n"
                                "[-t, --port2]: OSC port for IP address 2\n",
                                cmd_set, 2, 10),
        SHELL_CMD_ARG(get, NULL, "Get device setting\n"
                                "[-n --name]: Device Name\n"
                                "[-w --wifi]: WiFi Settings.\n"
                                "[-o, --osc]: OSC Settings\n"
                                "[-s, --sensor]: Sensor Settings\n",
                                cmd_get, 2, 10),
	SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_CMD_REGISTER(puara, &sub_puara_commands, "Puara Module commands", NULL);