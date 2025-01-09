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
static int parse_setting_args_get(const struct shell *sh, size_t argc, char *argv[], settingsVariables *var);

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
    struct getopt_state *state;
    static const struct option set_options[] = {
		{"ssid", required_argument, 0, 's'},
		{"ssid_password", required_argument, 0, 'p'},
		{"ap_password", required_argument, 0, 'a'},
		{"OSC IP1", required_argument, 0, 'i'},
		{"OSC IP2", required_argument, 0, 'o'},
		{"OSC Port1", required_argument, 0, 'r'},
		{"OSC Port2", required_argument, 0, 't'},
		{0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "s:p:a:i:o:r:t:", set_options, &opt_index)) != -1) {
        state = getopt_state_get();
        switch (opt) {
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
        default:
                printk("Invalid option %c\n", state->optopt);
                return -EINVAL;
        }
    }

    // Return 0 when done
    return 0;
}   

static int parse_setting_args_get(const struct shell *sh, size_t argc, char *argv[], settingsVariables *var) {
    // Options setup
	int opt;
	int opt_index = 0;
    struct getopt_state *state;
    static const struct option get_options[] = {
		{"ssid", no_argument, 0, 's'},
		{"ssid_password", no_argument, 0, 'p'},
		{"ap_password", no_argument, 0, 'a'},
		{"OSC IP1", no_argument, 0, 'i'},
		{"OSC IP2", no_argument, 0, 'o'},
		{"OSC Port1", no_argument, 0, 'r'},
		{"OSC Port2", no_argument, 0, 't'},
		{0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "spaiort", get_options, &opt_index)) != -1) {
        state = getopt_state_get();
        switch (opt) {
        case 's':
                var->name = "SSID";
                break;
        case 'p':
                var->name = "password";
                break;
        case 'a':
                var->name = "APpasswd";
                break;
        case 'i':
                var->name = "oscIP1";
                break;
        case 'o':
                var->name = "oscIP2";
                break;
        case 'r':
                var->name = "oscPORT1";
                break;
        case 't':
                var->name = "oscPORT2";
                break;
        default:
                printk("Invalid option %c\n", state->optopt);
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
        return -EINVAL;
    }
    ret = puara_module.set(var);

    if (ret) {
        std::cout << "Error in saving variable: " << ret << std::endl;
        return -EINVAL;
    }

    std::cout << "Successfully saved variable" << std::endl;
    return 0;
}

static int cmd_get(const struct shell *sh, size_t argc, char **argv, uint32_t period) {
    settingsVariables var;
    int ret = 0;
    if (parse_setting_args_get(sh, argc, argv, &var) != 0) {
        return -EINVAL;
    }
    ret = puara_module.get(&var);

    if (ret) {
        std::cout << "Error in geting variable: " << ret << std::endl;
        return -EINVAL;
    }

    // Print output of cmd to shell
    printk("%s: %s\n", var.name.c_str(), var.textValue.c_str());
    return 0;
}

// Create sub commands for puara shell
SHELL_STATIC_SUBCMD_SET_CREATE(sub_puara_commands,
        SHELL_CMD(reboot, NULL, "Reboot device", cmd_reboot_device),
        SHELL_CMD(ping, NULL, "Ping command.", cmd_puara_ping),
        SHELL_CMD(whoareyou, NULL, "Returns device name.", cmd_whoareyou),
        SHELL_CMD(set, NULL, "Set device setting\n"
                                "<-s --ssid \"<SSID>\">: SSID.\n"
                                "[-p, --psk]: SSID Password (valid only for secure SSIDs)\n"
                                "[-a, --apsk]: AP Password\n"
                                "[-i, --oscip1]: OSC IP address 1\n"
                                "[-o, --oscip2]: OSC IP address 2\n"
                                "[-r, --port1]: OSC port for IP address 1\n"
                                "[-t, --port2]: OSC port for IP address 2\n",
                                cmd_set),
        SHELL_CMD(get, NULL, "Set device setting\n"
                                "<-s --ssid \"<SSID>\">: SSID.\n"
                                "[-p, --psk]: SSID Password (valid only for secure SSIDs)\n"
                                "[-a, --apsk]: AP Password\n"
                                "[-i, --oscip1]: OSC IP address 1\n"
                                "[-o, --oscip2]: OSC IP address 2\n"
                                "[-r, --port1]: OSC port for IP address 1\n"
                                "[-t, --port2]: OSC port for IP address 2\n",
                                cmd_get),
	SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_CMD_REGISTER(puara, &sub_puara_commands, "Puara Module commands", NULL);