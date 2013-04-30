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

var exec = require('child_process').exec;

var me = module.exports = {
    si_date: function(now) {
        var now = now || new Date();
        var timeStamp = now.getFullYear() + "-" + me.zpad(now.getMonth() + 1, 2) + "-" + me.zpad(now.getDate(), 2) + "_" + me.zpad(now.getHours(), 2) + ":" + me.zpad(now.getMinutes(), 2) + ":" + me.zpad(now.getSeconds(), 2) + "." + now.getMilliseconds();
        return timeStamp;
    },

    /**
     * High level os command utility.
     * @param cmd {string} command to execute.
     * @param opts.callback {Function} callback to pass stdout to on completion. 
     */
    oscmd: function (cmd, opts) {
        opts = opts || {};
        exec(cmd, opts,
        function (error, stdout, stderr) {
            //if (stderr && !opts.ignore) {
            //    console.log("ERROR: Command '" + cmd + "' stderr: \n" + stderr);
            //}

            if (error !== null && !opts.ignore) {
                console.error("ERROR: " + error);
                //console.error("Command '" + cmd + "' exec error: \n" + error);
            }

            if (opts.callback) {
                opts.callback.call(this, stdout);
            }
        }
        );
    },

    /**
     * Trim whitespace off the start and end of a string.
     */
    trim: function (str) {
        if (typeof str !== 'string') return str;
        return str.replace(/^\s+|\s+$/g, "");
    },

    /**
     * Typical library extend() like jQuery or underscore have.
     */
    extend: function () {
        for (var i = 1; i < arguments.length; i++) {
            for (k in arguments[i]) {
                arguments[0][k] = arguments[i][k];
            }
        }
        return arguments[0];

    },

    fix: function (val, places) {
        var shift = Math.pow(10, places);
        return Math.floor(val * shift) / shift;
    },

    zpad: function (num, digits) {
        num = '' + num; // make sure number is a string.
        while (num.length < digits) {
            num = '0' + num;
        }
        return num;
    },

    distanceInMeters: function (lat1, lng1, lat2, lng2) {
        var PI = 3.14159265;
        var earthRadius = 3958.75;
        var dLat = (lng2-lng1) * PI / 180;
        var dLng = (lat2-lat1) * PI / 180;
        var a = Math.sin(dLat/2) * Math.sin(dLat/2) +
            Math.cos(lng1 * PI / 180) * Math.cos(lng2 * PI/180) *
            Math.sin(dLng/2) * Math.sin(dLng/2);
        var c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1-a));
        var dist = earthRadius * c;

        var meterConversion = 1609;

        return dist * meterConversion;
    },

    intPart: function(num) {
        var sgn = Math.abs(num)/num;
        return sgn * Math.floor(sgn*num);
    },

    // convert from decimal degrees into integer degrees with minutes.
    gpsFix: function (gps) {
        var fgps = parseFloat(gps);
        var dgps = me.intPart(fgps);
        var tmp = Math.abs((fgps - dgps)*60).toFixed(2);
        var sfgps = '';
        if(tmp < 10)
            sfgps = '0' + tmp;
        else
            sfgps = '' + tmp;
        return '' + dgps + ',' + sfgps;
    },

    errorWithTrace: function (msg, e) {
        if (!e) {
            console.trace();
            e = {};
        }
        console.error(msg + "\n" + e + "\n" + (e.stack || e.stacktrace || ""));
    }
};