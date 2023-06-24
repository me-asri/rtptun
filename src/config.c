#define _POSIX_C_SOURCE 200809L

#include "config.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <strings.h>

#include <errno.h>

#include "log.h"

static char *str_trim(char *str);
static bool str_is_char_quoted(char *str, char *c, char quote);
static char *str_strip(char *str);

static config_section_t *config_add_section(config_t *config, const char *name);
static config_section_t *config_get_section(config_t *config, const char *name);
static config_property_t *config_add_property(config_t *config, config_section_t *section, const char *key, const char *value);

int config_open(config_t *config, const char *path)
{
    int result = CONFIG_SUCCESS;

    FILE *file = fopen(path, "r");
    if (!file)
    {
        log_e("Failed to open file");

        result = CONFIG_IO_ERROR;
        goto error;
    }

    memset(config, 0, sizeof(config_t));

    // Reserve first section for global properties
    if (!config_add_section(config, NULL))
    {
        log_e("Failed to create global section");

        result = CONFIG_SYS_ERROR;
        goto sect_error;
    }

    char *buffer = NULL;
    size_t buffer_len = 0;
    ssize_t chars = 0;

    config_section_t *last_section = config->first_section;
    while ((chars = getline(&buffer, &buffer_len, file)) != -1)
    {
        if (buffer[chars - 1] == '\n')
        {
            buffer[chars - 1] = '\0';
            chars--;
        }

        char *line = str_trim(buffer);
        size_t line_len = strlen(line);

        // Ignore blank lines
        if (line_len == 0)
            continue;

        // Ignore comments
        if (line[0] == ';')
            continue;

        // Parse section
        if (line[0] == '[' && line[line_len - 1] == ']')
        {
            line[line_len - 1] = '\0';
            char *section_name = str_trim(line + 1);

            if (config_has_section(config, section_name))
            {
                log_e("Duplicate section '%s' in config file", section_name);

                result = CONFIG_INVALID_ERROR;
                goto parse_error;
            }

            if ((last_section = config_add_section(config, section_name)) < 0)
            {
                log_e("Failed to add section '%s'", section_name);

                result = CONFIG_SYS_ERROR;
                goto parse_error;
            }

            continue;
        }

        // Parse property
        char *delim, *ptr = line;
        while ((delim = strchr(ptr, '=')) != NULL && (str_is_char_quoted(line, delim, '\'') ||
                                                      str_is_char_quoted(line, delim, '"')))
            ptr = delim + 1;

        if (delim != NULL && delim != line)
        {
            const char *value = str_strip(str_trim(delim + 1));
            // Make sure property has a value
            if (*value != '\0')
            {
                *delim = '\0';
                const char *key = str_trim(line);

                if (!config_add_property(config, last_section, key, value))
                {
                    log_e("Failed to add property '%s'", key);

                    result = CONFIG_SYS_ERROR;
                    goto parse_error;
                }

                continue;
            }
        }

        log_e("Unrecognized entry: '%s' in config file", line);

        result = CONFIG_INVALID_ERROR;
        goto parse_error;
    }
    if (chars == -1 && (errno == EINVAL || errno == ENOMEM))
    {
        elog_e("getline() failed");

        result = CONFIG_IO_ERROR;
        goto read_error;
    }

    free(buffer);
    fclose(file);

    return result;

parse_error:
read_error:
    free(buffer);

sect_error:
    config_free(config);

    fclose(file);

error:
    return result;
}

void config_free(config_t *config)
{
    for (config_section_t *section = config->first_section; section != NULL;)
    {
        for (config_property_t *property = section->first_property; property != NULL;)
        {
            config_property_t *tmp = property->next;
            free(property);
            property = tmp;
        }

        config_section_t *tmp = section->next;
        free(section);
        section = tmp;
    }
}

int config_get_str(config_t *config, const char *section_name, const char *key, const char **value)
{
    config_section_t *section = config_get_section(config, section_name);
    if (!section)
        return CONFIG_NO_SECTION_ERROR;

    for (config_property_t *property = section->first_property; property != NULL; property = property->next)
    {
        if (strncasecmp(property->key, key, CONFIG_MAX_STR_LEN) == 0)
        {
            *value = property->value;
            return CONFIG_SUCCESS;
        }
    }
    return CONFIG_NO_PROPERTY_ERROR;
}

int config_get_int(config_t *config, const char *section, const char *key, int *value)
{
    const char *str_value;
    int ret;
    if ((ret = config_get_str(config, section, key, &str_value)) != CONFIG_SUCCESS)
        return ret;

    char *endptr;
    long num = strtol(str_value, &endptr, 10);
    if (*endptr != '\0')
        return CONFIG_VALUE_ERROR;
    if (num > INT_MAX)
        return CONFIG_VALUE_ERROR;

    *value = num;
    return CONFIG_SUCCESS;
}

int config_get_bool(config_t *config, const char *section, const char *key, bool *value)
{
    const char *str_value;
    int ret;
    if ((ret = config_get_str(config, section, key, &str_value)) != CONFIG_SUCCESS)
        return ret;

    if (strcasecmp("true", str_value) == 0)
        *value = true;
    else if (strcasecmp("false", str_value) == 0)
        *value = false;
    else
        return CONFIG_VALUE_ERROR;

    return CONFIG_SUCCESS;
}

bool config_has_section(config_t *config, const char *section)
{
    return (config_get_section(config, section));
}

bool config_has_property(config_t *config, const char *section_name, const char *key)
{
    const char *tmp;
    return (config_get_str(config, section_name, key, &tmp) == CONFIG_SUCCESS);
}

config_section_t *config_add_section(config_t *config, const char *name)
{
    config_section_t *next = malloc(sizeof(config_section_t));
    if (!next)
        return NULL;

    if (!name)
    {
        next->name[0] = '\0';
    }
    else
    {
        strncpy(next->name, name, CONFIG_MAX_STR_LEN);
        next->name[CONFIG_MAX_STR_LEN] = '\0';
    }

    next->next = NULL;
    next->first_property = NULL;
    next->last_property = NULL;

    if (!config->first_section)
    {
        config->first_section = next;
        config->last_section = next;
    }
    else
    {
        config->last_section->next = next;
        config->last_section = next;
    }

    return next;
}

config_section_t *config_get_section(config_t *config, const char *name)
{
    // First section reserved for global properties
    if (!name)
        return config->first_section;

    for (config_section_t *ptr = config->first_section; ptr != NULL; ptr = ptr->next)
    {
        if (strncasecmp(ptr->name, name, CONFIG_MAX_STR_LEN) == 0)
        {
            return ptr;
        }
    }

    return NULL;
}

config_property_t *config_add_property(config_t *config, config_section_t *section, const char *key, const char *value)
{
    config_property_t *next = malloc(sizeof(config_property_t));
    if (!next)
        return NULL;

    strncpy(next->key, key, CONFIG_MAX_STR_LEN);
    next->key[CONFIG_MAX_STR_LEN] = '\0';

    strncpy(next->value, value, CONFIG_MAX_STR_LEN);
    next->value[CONFIG_MAX_STR_LEN] = '\0';

    next->next = NULL;

    if (!section->first_property)
    {
        section->first_property = next;
        section->last_property = next;
    }
    else
    {
        section->last_property->next = next;
        section->last_property = next;
    }

    return next;
}

char *str_trim(char *str)
{
    while (isspace(*str))
        str++;

    if (*str == '\0')
        return str;

    char *end = str + strlen(str) - 1;
    while (end > str && isspace(*end))
        end--;
    end[1] = '\0';

    return str;
}

bool str_is_char_quoted(char *str, char *c, char quote)
{
    bool right_quote = false, left_quote = false;

    if (*c == '\0')
        return false;

    // Check right quote
    char *ptr = c + 1;
    while (*ptr)
    {
        if (*ptr == quote)
        {
            right_quote = true;
            break;
        }

        ptr++;
    }

    // Check left quote
    ptr = c - 1;
    while (ptr >= str)
    {
        if (*ptr == quote)
        {
            left_quote = true;
            break;
        }

        ptr--;
    }

    return (right_quote && left_quote);
}

char *str_strip(char *str)
{
    size_t len = strlen(str);

    if ((str[0] != '"' || str[len - 1] != '"') && (str[0] != '\'' || str[len - 1] != '\''))
        return str;

    str[len - 1] = '\0';
    return ++str;
}