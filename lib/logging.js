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
    util = require('./util.js');

var me = module.exports = {
    /**
     *	Log information locally & keep permanent record for optional reference/debugging later.
     */
    info: function () {
        this.log("INFO", arguments);
    },
    
    /**
     * Flag a critical system issue. These errors should "never" happen, and should be screened for
     * along with EM data analysis. (ie, a javascript error)
     */
    critical: function(msg, e) {
        this.crash(msg, e);
    },

    /**
     * Flag a system crash. These errors should "never" happen.
     * The name, arguments and stacktrace of the error are logged.
     */
    crash: function(msg, e) {
        if (!e) {
            console.trace();
            e = {};
        }
        this.error(msg + "\nMESSAGE: " + msg + "\n" + e + "\n" + (e.stack || e.stacktrace || ""));
    },
    
    /**
     * Flag an issue (see warn), and alert the web client of the issue. These errors are expected to happen,
     * if imposed by the environment. (ie, sensor cable unplugged.)
     */
    error: function () {
        this.log("ERROR", arguments);
    },

    /**
     *	Flag a (minor) issue in the permanent record to be checked during analysis. (ie, temperature slightly high reducing system mtbf)
     */
    warn: function () {
        this.log("WARN", arguments);
    },

    log: function () {
        var timeStamp = util.si_date();
        var type = arguments[0];
        var msgs = arguments[1];
        for (var key = 0; key < msgs.length; key++) {
            var msg = util.trim(msgs[key]);
            if (typeof msg === 'undefined' || msg.length == 0) continue;
            console.log(timeStamp + "\t" + type + "\t" + msg);
        }
    }

};
