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

#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <iostream>
using namespace std;

char NOTFOUND[] = "\0"; ///< The value returned by the getter function when the required configuration is found.
int config_length = 0;

char config[32][2][255]; ///< Hold all the configurations.

/**
 * Get a configuration by key.
 * @param key The key of the configuration.
 * @return The configuration with the give key. NOTFOUND if no configuration with that key exists.
 */
char* getConfig(const char* key) {
    for(int i = 0; i < config_length; i++) {
        if (strcmp(key, config[i][0]) == 0) return config[i][1];
    }

    cerr << "ERROR: Missing key '" << key << "' in config file" << endl;
    exit(-1);
    
    return NOTFOUND;
}


int readConfigFile() {
    FILE* config_fd;
    char dummy[255];

    config_fd = fopen(CONFIG_FILENAME, "r");

    if (!config_fd) {
        cerr << "ERROR: Couldn't load config file " << CONFIG_FILENAME << endl;
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
