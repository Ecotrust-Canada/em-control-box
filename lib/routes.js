var fs = require('fs'),
    util = require('./util.js'),
    config = require('./config.js'),
    //logging = require('./logging.js'),
    cam_cache = [];

var me = module.exports = {
    _index: function(EM_STATE) {
        return function (req, res) {
            res.render('index.jade', {
                versions: EM_STATE.releaseVersion + "-" + EM_STATE.recorderVersion + "-" + EM_STATE.serverVersion,
                diskNumber: EM_STATE.dataDiskLabel.replace("DATA_", "")
            });
        }
    },

    em_state: function(req, res) {
        var json = fs.readFileSync(config.JSON_STATE_FILE);
        res.setHeader('Content-Type', 'application/json');
        res.setHeader('Content-Length', json.length);
        res.end(json);
    },

    report: function (req, res) {
        var d = new Date(),
            stamp = d.getFullYear() + "-" + d.getMonth() + "-" + d.getDate() + "_" + d.getHours() + "-" + d.getMinutes() + "-" + d.getSeconds(),
            filename = "/mnt/data/reports/report-" + stamp + ".txt",
            report = "Compliance Issue Self Report\n\n";

        report += "  DATE/TIME : " + stamp + "\n\n";
        report += "  ISSUE : " + req.body.issue + "\n\n";
        report += "  DESCRIPTION : " + req.body.description + "\n\n";

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

/*
    _importGPS: function(EM) {
        return function (req, res) {
            res.send({
                success: true,
                curW: EM.curW,
                curN: EM.curN
            });
        }
    },
*/

    retrieveElogInfo: function (req, res) {      
        res.send({
            success: true,
            vrn: config.vrn,
            vessel: config.vessel,
        });
    },

    displayElogTop: function (req, res) {
        var success = false;
        var ELOG_TOP = {};
        fs.readFile(config.ELOG_HEADER, function (err, data) {
            data = '' + data;
            if (!err && ('' + data).length > 10) {
                success = true;
                
                try {
                    ELOG_TOP = JSON.parse('' + data);
                } catch (e) {
                    util.errorWithTrace("Failed to parse JSON.", e);
                }
            }

            res.send({
                success: success,
                ELOG_TOP: ELOG_TOP
            });
        });
    },

    saveElogTop: function (req, res) {
        var success = true;
        var error = "";
       
        if(!req.body.skipper)
        {
            success = false;
            error = "Please input the Skipper.";
        }
        else if(!req.body.fin)
        {
            success = false;
            error = "Please input the F.I.N.";
        }
        else if(isNaN(req.body.fin))
        {
            success = false;
            error = "Invalid F.I.N.";
        }
        else if(!req.body.fm_grounding_lines && !req.body.fm_singles)
        {
            success = false;
            error = "Please select the Fishing Method.";
        }
        else if(!req.body.bf_jars && !req.body.bf_clips && !req.body.bf_cages)
        {
            success = false;
            error = "Please select the Bait Fastener.";
        }
        else if(!req.body.depths)
        {
            success = false;
            error = "Please select the Depths.";
        }
        else if(!req.body.catch_weights)
        {
            success = false;
            error = "Please select the Catch Weights.";
        }
        
        if(success)
        {
            var data = JSON.stringify(req.body);
            var fout_data = fs.createWriteStream(config.DATA_MNT + '/ELOG_TOP.' + util.si_date() + '.JSON', {'flags': 'w'});
            var fout_local = fs.createWriteStream(config.ELOG_HEADER, {'flags': 'w'});
            fout_data.write(data);
            fout_local.write(data);
        }

        res.send({
            success:success,
            error:error
        });
    },

    logElogRow: function (req, res) {
        var success = true;
        var error = "";
        var days = [-1, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
        var isInt = function (x) {
            x = x.toString().trim(); 
            while(x[0] == '0')
                x = x.substring(1);
            var y = parseInt(x);
            if(isNaN(y))
                return false;
            var stry = y.toString();
            while(stry.length < x.length)
                stry = "0" + stry;
            return x.toString() == stry;
        };

        if(!isInt(req.body.date_hauled_month) || Number(req.body.date_hauled_month)< 1 || Number(req.body.date_hauled_month> 12))
        {
            success = false;
            error = "date_hauled_month";
        }
        else if(!isInt(req.body.date_hauled_day) || Number(req.body.date_hauled_day)< 1 || Number(req.body.date_hauled_day> days[req.body.date_hauled_month]))
        {
            success = false;
            error = "date_hauled_day";
        }
        else if(!isInt(req.body.date_hauled_hour) || Number(req.body.date_hauled_hour)< 0 || Number(req.body.date_hauled_hour> 24))
        {
            success = false;
            error = "date_hauled_hour";
        }
        else if(!isInt(req.body.date_hauled_minute) || Number(req.body.date_hauled_minute)< 0 || Number(req.body.date_hauled_minute>= 60))
        {
            success = false;
            error = "date_hauled_minute";
        }
        else if(req.body.depth_min == "" || isNaN(req.body.depth_min))
        {
            success = false;
            error = "depth_min";
        }
        else if(req.body.depth_max == "" || isNaN(req.body.depth_max))
        {
            success = false;
            error = "depth_max";
        }
        else if(!isInt(req.body.number_of_pieces))
        {
            success = false;
            error = "number_of_pieces";
        }
        else if(isNaN(req.body.weight_of_catch == "" || req.body.weight_of_catch))
        {
            success = false;
            error = "weight_of_catch";
        }

        if(success)
        {
            var fout = fs.createWriteStream(config.ELOG, {'flags': 'a'});
            fout.write(req.body.date_hauled_month + "," + req.body.date_hauled_day + "," + 
                    req.body.date_hauled_hour + "," + req.body.date_hauled_minute + "," +
                    req.body.depth_min + "," + req.body.depth_max + "," + req.body.species + "," +
                    req.body.number_of_pieces + "," + req.body.weight_of_catch + "," +
                    req.body.released_num + "," + req.body.released_wt + "," + 
                    req.body.kept_num + "," + req.body.kept_wt + "," + 
                    req.body.pbs_code + "," + req.body.remarks + "\n");
        }

        res.send({
            success:success,
            error:error
        });
    },

/*
    _importDate: function(EM) {
        return function (req, res) {
            res.send({
                success: "success",
                month: EM.month,
                day: EM.day,
                hour: EM.hour,
                minute: EM.minute
            });
        }
    },
*/

    resetTrip: function (req, res) {
        config.RFID_TAGS.trip_scans = 0;
        var fout = fs.createWriteStream(config.SCANNED_TAGS, {'flags': 'w'});
        fout.write(JSON.stringify(config.RFID_TAGS) || '{}');

        res.send({
            success: true
        });
    },

    resetString: function (req, res) {
        config.RFID_TAGS.string_scans = 0;
        var fout = fs.createWriteStream(config.SCANNED_TAGS, {'flags': 'w'});
        fout.write(JSON.stringify(config.RFID_TAGS) || '{}');

        res.send({
            success: true
        });
    },

    getTripString: function (req, res) {
        while (!config.RFID_TAGS);
        res.send({
            success: true,
            trip_scans: config.RFID_TAGS.trip_scans,
            string_scans: config.RFID_TAGS.string_scans
        });
    },

    displayElog: function (req, res) {
        fs.readFile(config.ELOG, function (err, data) {
            if (err) 
            {
                data = '';
                console.error(err);
            }
            elog = '' + data;
            elog = elog.split("\n");
            res.send({
                success:true,
                elog:elog
            });

        });
    },

    searchRFID: function (req, res) {
        var success = false, result = {}, day;

        if(typeof req.body.days !== 'undefined' && req.body.days.length > 0) {
            days = Number(req.body.days);
            if(!isNaN(days) && req.body.days.indexOf("-") == -1 && req.body.days.indexOf(".") == -1) {
                var today = new Date();
                var one_day = 1000*60*60*24;
                for(id in config.RFID_TAGS.data) {
                    day = new Date(config.RFID_TAGS.data[id].ddate);

                    if (day) {
                        dif = Math.floor((today.getTime() - day.getTime())/one_day);
                        if(dif >= days) {
                            success = true;
                            result[id] = config.RFID_TAGS.data[id];
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
                rfids: result
            });
        } else {
            res.send({
                success: false
            });
        }
    }
};
