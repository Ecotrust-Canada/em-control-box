var util = require('./util.js'),
    logging = require('./logging.js'),
    config = require('./config.js'),
    tasks = [],
    tick = 0;

var me = module.exports = {
    start: function(EM) {
        messages = EM.messages;

        function addTask(fn, opts) {
            logging.info("Scheduling periodic task: " + fn.name);
            tasks.push( { callback: fn, opts: opts });
        }

        setInterval(function () {
            tick++;
            for (var i = 0; i < tasks.length; i++) {
                try {
                    if (tick % tasks[i].opts.frequency == tasks[i].opts.offset) {
                        tasks[i].callback();
                    }
                } catch(e) {
                    logging.critical("Task #"+i+" failed", e);
                }
            }
        }, 1000);


        
        /**
         * Profile the box heat
         */
        function getLmSensors() {
            try {
                util.oscmd("sensors", {
                    callback: function (output) {
                        
                        output = ('' + output).split("\n");
                        output.forEach(function(item){
                            var m = item.match(/Core\s0:\s+\+(\d+\.\d+)/)
                            if (m && m[1]) {
                                var temp = parseFloat(m[1]), status;
                                if (temp <= 55) {
                                    status = "OK";
                                    logging.info('CORE 0 TEMP:'+temp+' DEGREES');
                                } else if (temp < 70) {
                                    status = "RUNNING HOT";
                                    logging.warn('CORE 0 TEMP:'+temp+' DEGREES');
                                } else {
                                    status = "OVERHEATING";
                                    logging.error('CORE 0 TEMP:'+temp+' DEGREES');
                                }
                                messages.TEMP = {
                                    value: m[1],
                                    status: status
                                };
                                
                            }
                        });
                    }
               });
            } catch(e) {
                logging.critical("Crashed while getting temperature", e);
            }
        }
        
        function checkBrowser() {
            try {
                util.oscmd("ps aux | grep -s firefox | grep -sv grep", {
                    ignore: true,
                    callback: function (output) {
                        var renderer, parentp;
                        var lines = output.trim().split("\n");
                        if (lines[0]) parentp=true;

                        for (var i = 0; i < lines.length; i++) {
                            if (lines[i].indexOf('renderer') !== -1) {
                                renderer=true
                            }
                        }
                        
                        if (!renderer && parentp) {
                            util.oscmd("sudo killall -q chrome", {callback:function(output){}});
                            logging.critical("Chrome crashed!");
                        }
                    }
                });
            } catch(e) {
                logging.critical("Crashed while checking browser", e);
            } 
        }
        
        //addTask(getLmSensors, { frequency: 180, offset: 6 });
        //addTask(checkBrowser, { frequency: 10, offset: 10 });
    }
};