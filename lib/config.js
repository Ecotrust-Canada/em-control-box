/*
VMS: Vessel Monitoring System - Control Box Software
Copyright (C) 2011 Ecotrust Canada
Knowledge Systems and Planning

This file is part of VMS.

VMS is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

VMS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with VMS. If not, see <http://www.gnu.org/licenses/>.

You may contact Ecotrust Canada via our websitehttp://ecotrust.ca
*/

var fs = require('fs'),
    path = require('path'),
    EM_CONFIG_FILE = "/etc/em.conf";

var me = module.exports = {
    SERVER_VERSION: '2.0.1',

    setup: function() {
        /**
         *  Read EM instance configuration file.
         */
        var data = fs.readFileSync(EM_CONFIG_FILE);

        if (!data) {
            console.error("ERROR: Couldn't load config file %s", err);
            return;
        }

        data = (data + '').split("\n");
        for (var i = 0; i < data.length; i++) {
            if (data[i].length > 0 && data[i][0] != '#') {
                line = data[i].split("=");
                me[line[0]] = line[1];
            }
        }
    }
};
