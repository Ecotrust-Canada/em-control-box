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

var SERVER_VERSION = '2.0.0';

console.log("INFO: Ecotrust EM Web Server v%s is starting", SERVER_VERSION);

var path = require('path'),
    fs = require('fs'),
    express = require('express'),
    util = require('./lib/util.js'),
    config = require('./lib/config.js'),
    routes = require('./lib/routes.js'),
    server = express(),
    port = 8081;

// Ensure working directory is correct.
process.chdir(__dirname);

config.setup();
//tasks.start(EM);

var EM_STATE = require(config.JSON_STATE_FILE);
EM_STATE.releaseVersion = fs.readFileSync('/em-release');
EM_STATE.serverVersion = SERVER_VERSION;

server.set('views', path.join(__dirname, 'views'));
server.use(express.static(path.join(__dirname, 'public')));
server.use(express.bodyParser());
server.use(server.router);

server.get('/em',                   routes._index(EM_STATE));
//server.get('/camFrame.html',         routes.camFrame);
//server.get('/cam/:id',              routes.camServer);
server.get('/em_state.json',        routes.em_state);
server.post('/report',              routes.report);
server.post('/retrieve_elog_info',  routes.retrieveElogInfo);
server.post('/display_elog_top',    routes.displayElogTop);
server.post('/save_elog_top',       routes.saveElogTop);
//server.post('/import_date',         routes.importDate;
server.post('/reset_trip',          routes.resetTrip);
server.post('/reset_string',        routes.resetString);
server.post('/get_trip_and_string', routes.getTripString);
server.post('/display_elog',        routes.displayElog);
server.post('/search_rfid',         routes.searchRFID);
server.post('/log_elog_row',        routes.logElogRow);
server.get('/500',                  function(req, res) { throw new Error("500 - Internal server error"); });
//server.get('/*',                    function(req, res) { throw new Error("404 - Not found"); });

server.listen(port);
console.log("INFO: Listening on port " + port);

/* Periodically check if > X seconds has gone by since last call to get /em_state.json,
if it has, means browser is not responding, should restart browser

/* detect this somehow + reveal in UI
console.error("ERROR: Not recording! Please reboot system. If this persists, please return to port.");

/* Calculate average speed -- WHY?
    speed = body[7],
    sa = 0,
    sh = VMT.speed_history;

sh.push(parseFloat(speed));
if (sh.length > 10) {
    sh.shift();
}
for (var j = 0; j < sh.length;j++) {
    sa += sh[j];
}
VMT.average_speed = sa / sh.length;

/* Deal with these issues
if(parseFloat(cpuUsage) > 95)
{
    status = "HIGH CPU USAGE";
    logging.warn("High CPU usage: " + cpuUsage + "%");
}
if(memoryLeft < 10)
{
    status = "HIGH MEMORY USAGE";
    logging.error("High MEM usage");
}

/* Update RFID cache file -- WHY?

if(protocol == 'RFID') {
    this.updateRFIDCache(out.RFID.value, lat, lng, body[2].substring(0, 10));
    out.RFID.trip_scans = config.RFID_TAGS.trip_scans;
    out.RFID.string_scans = config.RFID_TAGS.string_scans;
}

setup_video: function(){
  try {
    util.oscmd("cat " + config.VMS_RESOLUTION + " 2>/dev/null; true", { callback: function(output) {   
        var m = (''+output||'').match(/(\d+)x/);
        if (m) {
            logging.info('Got video resolution [' + m[1] + ']');

            VMT.subsystems.getByName('video').args = [m[1]]; 
            VMT.subsystems.getByName('video').spawn();
        } else {
            if (VMT.res_read_tries < 10) { // allow 10 seconds for user to log in (automatic).
                VMT.res_read_tries ++;
                logging.info('Trying to read ' + config.VMS_RESOLUTION + ' (' + VMT.res_read_tries + ') ...');
                setTimeout(VMT.setup_video, 1000);
            } else {
                logging.error ("Failed to get video resolution [default = 1024x768]");
                VMT.subsystems.getByName('video').args = ["1024"];
                VMT.subsystems.getByName('video').spawn();
            }
        }
    }});
  } catch (e) {
      logging.crash("Crashed while getting video resolution", e);
      VMT.subsystems.getByName('video').args = ["1024"];
      VMT.subsystems.getByName('video').spawn();
  }
},

updateRFIDCache: function (id, lat, lng, sdate) {
    config.RFID_TAGS.string_scans += 1;
    config.RFID_TAGS.trip_scans += 1;
    
    ddate = new Date(sdate);
    config.RFID_TAGS.data[id] = {
        lat: lat,
        lng: lng,
        ddate: ddate
    };

    var fout = fs.createWriteStream(config.SCANNED_TAGS, {'flags': 'w'});
    fout.write(JSON.stringify(config.RFID_TAGS) || '{}');
},
*/
