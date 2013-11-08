/*
EM: Electronic Monitor - Control Box Software
Copyright (C) 2012 Ecotrust Canada
Knowledge Systems and Planning

This file is part of EM.

EM is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

EM is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with EM. If not, see <http://www.gnu.org/licenses/>.

You may contact Ecotrust Canada via our website http://ecotrust.ca
*/

#define INIT_CONFIG
#include "config.h"
#include "output.h"

#include <cstring>
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <string>

using namespace std;

char NOTFOUND[] = "\0"; ///< The value returned by the getter function when the required configuration is found.
int config_length = 0;

char config[32][2][255]; ///< Hold all the configurations.

/**
 * Get a configuration by key.
 * @param key The key of the configuration.
 * @return The configuration with the give key. NOTFOUND if no configuration with that key exists.
 */
string getConfig(const char* key, const char* default_value) {
    for(int i = 0; i < config_length; i++) {
        if (strcmp(key, config[i][0]) == 0) return string(config[i][1]);
    }

    E("Missing key '" << key << "' in config file, using default '" << default_value << "'");
    return string(default_value);
}


int readConfigFile(const char *file) {
    FILE *config_fd;
    char dummy[255];

    config_fd = fopen(file, "r");

    if (!config_fd) {
        E("Couldn't load config file " << file);
        return 1;
    }

    config[config_length][1][0] = '\0';  // this value will change if line is read.

    while (fscanf(config_fd, "%[^#=]=%[^\n]\n", config[config_length][0], config[config_length][1]) != EOF) {
        if (config[config_length][1][0] != '\0') {
            config_length++;
        }

        // scan away blank lines and comments.
        fscanf(config_fd, "%[\n]", dummy);
        fscanf(config_fd, "#%[^\n]\n", dummy);
        fscanf(config_fd, "%[\n]", dummy);

        config[config_length][1][0] = '\0';  // used to detect whether a line was read
    }

    fclose(config_fd);
    return 0;
}
