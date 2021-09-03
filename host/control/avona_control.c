/*
 * test.c
 *
 *  Created on: Apr 7, 2021
 *      Author: mbruno
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "argtable/argtable3.h"
#include "commands.h"

#define VERSION_CMD 0x80

struct arg_lit *help, *version;
struct arg_str *get, *set, *params;
struct arg_end *end;

int main(int argc, char **argv)
{
    control_ret_t ret = CONTROL_ERROR;

    void *argtable[] = {
        help    = arg_lit0(NULL, "help", "display this help and exit"),
        version = arg_lit0(NULL, "version", "display version info and exit"),

        get     = arg_str0("g", "get", "<cmd>", "Gets the value(s) for the specified command. Must not be used with --set."),
        set     = arg_str0("s", "set", "<cmd>", "Sets the provided parameter value(s) with the specified command. Must not be used with --get."),

        params  = arg_str0(NULL, NULL, "<param>", "Command parameters for use with set"),

        end     = arg_end(20),
    };


    int nerrors;
    nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0) {
        printf("Usage: %s", argv[0]);
        arg_print_syntax(stdout, argtable, "\n");
        printf("Control tool for Avona\n\n");
        arg_print_glossary_gnu(stdout, argtable);
        return 0;
    }

    if (nerrors > 0) {
        /* Display the error details contained in the arg_end struct.*/
        arg_print_errors(stdout, end, argv[0]);
        printf("Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

#if USE_USB
    ret = control_init_usb(0x20B1, 0x3652, 0);
#elif USE_I2C
    ret = control_init_i2c(0x42);
#endif

    if (ret == CONTROL_SUCCESS) {
        cmd_t *cmd;
        cmd_param_t *cmd_values = NULL;

        ret = CONTROL_ERROR;

        if (get->count != 0 && set->count == 0) {

            if (params->count == 0) {

                cmd = command_lookup(get->sval[0]);
                if (cmd != NULL) {
                    cmd_values = calloc(cmd->num_values, sizeof(cmd_param_t));
                    ret = command_get(cmd, cmd_values, cmd->num_values);
                } else {
                    printf("Command %s not recognized\n", get->sval[0]);
                }
            } else {
                printf("Get commands do not take any parameters\n");
            }

            if (cmd_values != NULL) {
                for (int i = 0; i < cmd->num_values; i++) {
                    command_value_print(cmd, cmd_values[i]);
                }
                printf("\n");
            }

        } else if (set->count != 0 && get->count == 0) {

            cmd = command_lookup(set->sval[0]);
            if (cmd != NULL) {

                if (cmd->num_values == params->count) {
                    cmd_values = calloc(params->count, sizeof(cmd_param_t));

                    for (int i = 0; i < params->count; i++) {
                        cmd_values[i] = command_arg_string_to_value(cmd, params->sval[i]);
                    }

                    ret = command_set(cmd, cmd_values, params->count);
                } else {
                    printf("The command %s requires %d parameters\n", cmd->cmd_name, cmd->num_values);
                }

            } else {
                printf("Command %s not recognized\n", get->sval[0]);
            }

        } else if (set->count != 0 && get->count != 0) {
            printf("Must not specify both --get and --set commands\n");
        }

        if (cmd_values != NULL) {
            free(cmd_values);
        }
    }

    return ret == CONTROL_SUCCESS ? 0 : 1;
}
