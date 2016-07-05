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

var path = require('path'),
    fs = require('fs'),
    express = require('express'),
    config = require('./lib/config.js'),
    routes = require('./lib/routes.js'),
    server = express(),
    exec = require('child_process').exec,
    port = 8081,
    videoPlayingFalseCount = 0;

function syscallReporter(error, stdout, stderr) {
    if (error) console.error(error);
    if (stderr) console.error(stderr);
}

function check_video_playing() {    
    exec("/opt/em/bin/check-browser-video.sh", function(error, stdout, stderr){
        var data = JSON.parse(stdout);
        
        // if number of mplayer processes does not match number of cameras
        if(routes._numCams > 0 && (data.numProcs != routes._numCams || data.cpuUsage < 2)) {
            videoPlayingFalseCount++;

            if(videoPlayingFalseCount >= 4) {
                routes._videoPlaying = false;
            }

            return;
        }

        routes._videoPlaying = true;
        videoPlayingFalseCount = 0;
    });    
}

console.log("INFO: Ecotrust EM Web Server v%s is starting", config.SERVER_VERSION);

process.chdir(__dirname); // ensure working directory is correct
config.setup();

server.configure(function() {
    server.set('views', path.join(__dirname, 'views'));
    server.enable('view cache');
    server.use(express.static(path.join(__dirname, 'public')));
    server.use(express.bodyParser());
    server.use(server.router);
});

server.get('/em',                   routes.index);
server.get('/em_state.:extension',        routes.em_state);
server.get('/states.json',          routes.states);
server.get('/options.json',         routes.options);
server.get('/500',                  function(req, res) { throw new Error("500 - Internal server error"); });

server.post('/report',                routes.report);
server.post('/reset_trip',            routes.resetTrip);
server.post('/reset_string',          routes.resetString);
server.post('/stop_video_recording',  routes.stopVideoRecording);
server.post('/start_video_recording', routes.startVideoRecording);
server.post('/search_rfid',           routes.searchRFID);
server.post('/presystem_check',       routes.presystemCheck);

server.listen(port);

// warm up the cache
exec("/usr/bin/wget -O - -q -t 1 http://127.0.0.1:8081/em/ > /dev/null", syscallReporter);

var gpsd = require('node-gpsd');

var listener = new gpsd.Listener({
    port: 2947,
    hostname: 'localhost',
    logger:  {
        info: function() {},
        warn: console.warn,
        error: console.error
    },
    parse: true
});

listener.connect(function() {
    console.log('GPS Connected');
    listener.watch();
});


if ( !Date.prototype.toTimeDateCtl ) {
  ( function() {

    function pad(number) {
      var r = String(number);
      if ( r.length === 1 ) {
        r = '0' + r;
      }
      return r;
    }

    Date.prototype.toTimeDateCtl = function() {
      return this.getFullYear()
        + '-' + pad( this.getMonth() + 1 )
        + '-' + pad( this.getDate() )
        + ' ' + pad( this.getHours() )
        + ':' + pad( this.getMinutes() )
        + ':' + pad( this.getSeconds() )
    };

  }() );
}

var date;
listener.on("TPV", function(data){
    if (!date && data.time) {
        date = new Date(data.time);
        console.log('timedatectl set-time "' + date.toTimeDateCtl() + '"');
        exec('timedatectl set-time "' + date.toTimeDateCtl() + '"', function(){
            syscallReporter.apply(this,arguments);
            exec('hwclock --systohc', syscallReporter);
        });
    }
});
 
console.log("INFO: Listening on port " + port);

// every 900 msecs check video
setInterval(check_video_playing, 800);
