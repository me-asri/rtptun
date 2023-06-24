#ifndef RTPTUN_CONFIG_H
#define RTPTUN_CONFIG_H

#include <stdbool.h>

#define CONFIG_MAX_STR_LEN 255

typedef struct config_property
{
    char key[CONFIG_MAX_STR_LEN + 1];
    char value[CONFIG_MAX_STR_LEN + 1];

    struct config_property *next;
} config_property_t;

typedef struct config_section
{
    char name[CONFIG_MAX_STR_LEN + 1];

    config_property_t *first_property;
    config_property_t *last_property;

    struct config_section *next;
} config_section_t;

typedef struct config
{
    config_section_t *first_section;
    config_section_t *last_section;
} config_t;

enum config_ret
{
    // Operation successful
    CONFIG_SUCCESS = 0,
    // No such section exists
    CONFIG_NO_SECTION_ERROR,
    // No such property exists
    CONFIG_NO_PROPERTY_ERROR,
    // Invalid config file
    CONFIG_INVALID_ERROR,
    // Invalid value
    CONFIG_VALUE_ERROR,
    // IO error
    CONFIG_IO_ERROR,
    // System error
    CONFIG_SYS_ERROR
};

int config_open(config_t *config, const char *path);
void config_free(config_t *config);

int config_get_str(config_t *config, const char *section_name, const char *key, const char **value);
int config_get_int(config_t *config, const char *section_name, const char *key, int *value);
int config_get_bool(config_t *config, const char *section_name, const char *key, bool *value);

bool config_has_section(config_t *config, const char *section_name);
bool config_has_property(config_t *config, const char *section_name, const char *key);

#endif