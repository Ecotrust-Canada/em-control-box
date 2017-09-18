var fs = require('fs'),
    config = require('./config.js'),
    exec = require('child_process').exec;

var me = module.exports = {
    _numCams: "test",
    _videoPlaying: true,

    index: function (req, res) {
        var EM_STATE = require(config.JSON_STATE_FILE);

        res.render('index.jade', {
            versions: fs.readFileSync('/em-release') + '' + " - " + EM_STATE.recorderVersion + " - " + config.SERVER_VERSION,
            rfid: config.rfid,
            show_camera_toggle: config.show_camera_toggle
        });
    },

    em_state: function(req, res) {
        var extension = req.params.extension;
        fs.readFile(config.JSON_STATE_FILE, function(err, data) {
            if (err) throw err;

            var em_state = JSON.parse(data);
            me._numCams = em_state.SYS.numCams;
            em_state.SYS.videoPlaying = me._videoPlaying;
            data = JSON.stringify(em_state);
            if (extension === 'jsonp') {
                data = "jsonpCallback(" + data + ")";
            }
            res.setHeader('Content-Type', 'application/json');
            res.setHeader('Content-Length', data.length);
            res.end(data);
        });
    },

    states: function(req, res) {
        fs.readFile('/opt/em/public/states.json', function(err, data) {
            if (err) throw err;

            res.setHeader('Content-Type', 'application/json');
            res.setHeader('Content-Length', data.length);
            res.end(data);
        });
    },

    options: function(req, res) {
        fs.readFile('/opt/em/public/options.json', function(err, data) {
            if (err) throw err;

            res.setHeader('Content-Type', 'application/json');
            res.setHeader('Content-Length', data.length);
            res.end(data);
        });
    },
    
    report: function (req, res) {
        var d = new Date(),
            stamp = d.getFullYear() + "-" + d.getMonth() + "-" + d.getDate() + "_" + d.getHours() + "-" + d.getMinutes() + "-" + d.getSeconds(),
            filename = config.EM_DIR + '/data/reports/' + stamp + '.txt',
            report = "Compliance Issue Self Report\n";

        report += "  DATE/TIME: " + stamp + "\n";
        report += "  ISSUE: " + req.body.issue + "\n";
        report += "  DESCRIPTION: " + req.body.description + "\n\n";

        fs.writeFile(filename, report, function (err) {
            if (err) {
                console.error(err);
                res.send({
                    success: false
                });
            } else {
                res.send({
                    success: true
                });
            }
        });
    },
    presystemCheck: function (req, res) {
      var d = new Date(),
              stamp = d.getFullYear() + "-" + d.getMonth() + "-" + d.getDate() + "_" + 
                      d.getHours() + "-" + d.getMinutes() + "-" + d.getSeconds(),
              filename = '/mnt/data/screenshots/pre-system-check' + stamp + '.png';

      exec("/usr/bin/scrot "+filename, function(err, stdout, stderr) {
        if (err) {
            console.error(err);
            res.send({
              message: "Sorry, failed to save screenshot:" + err + "|" + stderr,
              success: false
            });
        } else {
            res.send({
              message: "Thank You for your System Check.\nScreenshot was saved in \n"+filename+".",
              success: true
            }); 
        };
      });
    },
    /* stop and start video recording by toggling status of network interface, 
    *  this is a terrable hack that explots the fact that video recording stops 
    *  somewhat gracefully when camera connectivity is lost.
    *  really we should communicate to the em recording process that video should
    *  be stopped and leave the network interface alone */
    stopVideoRecording: function (req, res) {
        var command = 'ip link set eth0 down';

        exec(command, function (err, stdout, stderr) {
            if (err) {
                console.error(err);
                res.send({
                    command: command,
                    message: 'Sorry, failed to disable camera.',
                    success: false
                });
            } else {
                res.send({
                    command: command,
                    message: 'Camera has been disabled.',
                    success: true
                });
            }
        });
    },
    startVideoRecording: function (req, res) {
        var command = 'ip link set eth0 up';

        exec(command, function (err, stdout, stderr) {
            if (err) {
                console.error(err);
                res.send({
                    command: command,
                    message: 'Sorry, failed to enable camera.',
                    success: false
                });
            } else {
                res.send({
                    command: command,
                    message: 'Camera has been enabled.',
                    success: true
                });
            }
        });
    },
    resetString: function (req, res) {
        exec("/usr/bin/pkill -SIGUSR1 em-rec", 
            function(error, stdout, stderr) {
                var success = true;
                if (stderr || error){
                    console.error('Reset String: stderr: ' + stderr);
                    console.error('Reset String: error: ' + error.code);
                    success = false;
                }
                res.send({
                    success: success
                });
        });
    },

    resetTrip: function (req, res) {
        exec("/usr/bin/pkill -SIGUSR2 em-rec", 
            function(error,stdout,stderr){
                var success = true;
                if (stderr || error){
                    console.error('Reset Trip: stderr: ' + stderr);
                    console.error('Reset Trip: error: ' + error.code);
                    success = false;
                }
                res.send({
                    success: success
                });
        });
    },

    searchRFID: function (req, res) {
        var success = false, result = {}, day, diff;

        if(typeof req.body.days !== 'undefined' && req.body.days.length > 0) {
            days = Number(req.body.days);
            if(!isNaN(days) && req.body.days.indexOf("-") == -1 && req.body.days.indexOf(".") == -1) {
                var fileName = config.BACKUP_DATA_MNT + '/' + config.RFID_DATA;
                var rfidScans = fs.readFileSync(fileName);

                if (!rfidScans) {
                    console.error('ERROR: Couldn\'t load ' + fileName);
                    return;
                }

                rfidScans = (rfidScans + '').split("\n");

                var today = new Date();
                var one_day = 1000 * 60 * 60 * 24;

                for (var i = 0; i < rfidScans.length; i++) {
                    if (rfidScans[i].length > 0) {
                        columns = rfidScans[i].split(',');

                        day = new Date(columns[2]);
                        if (day) {
                            diff = Math.floor( (today.getTime() - day.getTime()) / one_day );

                            if(diff >= days) {
                                success = true;
                                result[columns[3]] = { 'lat': columns[4], 'lon': columns[5], 'date': columns[2], 'diff': diff };
                            }
                        }
                    }
                }
            } else {
                success = "FORMAT_ERROR";
            }
        } else {
            success = "FORMAT_ERROR";
        }

        if(success) {
            res.send({
                success: success,
                rfidTags: result
            });
        } else {
            res.send({
                success: false
            });
        }
    }
};
