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
/*
var spawn = require('child_process').spawn,
    oop = require('./oop.js'),
    logging = require('./logging.js'),
    Class = oop.Class,
    List = oop.List;

SubSystem = Class(function () {
    this.errors = []; // buffer to store errors until sent to client.
    this.output = []; // ''           messages ''
}, {
    spawn: function () {
        var self = this;
        
        if(!self.handle || self.handle.killed) {
            logging.info('Spawning ' + this.name);
            self.handle = spawn(self.path, self.args || []);
            self.handle.killed = false;

            self.handle.stdout.on('data', function (data) {
                data += '';
                if(self.onData) self.onData(data);
                self.output.push(data);
                if(self.output.length > 100) //Clear the first half from the buffer.
                self.output = self.output.slice(Math.floor(self.output.length / 2));
            });

            self.handle.stderr.on('data', function (data) {
                // TODO: resolve video errors.
                var values = '' + data;
                var lines = values.split("\n");
                for(var i in lines) {
                    var error = lines[i];
                    var tokens = error.split(":");
                    if(tokens[0] == "WARN") logging.warn(error.substring(5));
                    else if(self.name !== 'video' && error.length > 0) {
                        self.errors.push(error);
                        logging.error(error);
                        if(self.errors.length > 100) //Clear the first half from the buffer.
                        self.errors = self.errors.slice(Math.floor(self.errors.length / 2));
                    }
                }
            });

            self.handle.on('exit', function (code) {
                logging.error(self.name + ' EXITED!');
                self.errors.push(self.name + ' RESTARTED, EXIT CODE: ' + code);
                setTimeout(function () {
                    if(!self.handle.killed) {
                        self.handle.killed = true;
                        self.spawn(self);
                    }
                }, 10000);
            });
        }
    },

    kill: function () {
        if(typeof this.handle !== "undefined" && typeof this.handle.killed !== "undefined" && !this.handle.killed) {
            this.handle.killed = true;
            this.handle.kill();
            delete this.handle;
        }
    },

    status: function () {
        if (typeof this.parse == 'function') {
            status = this.parse();
            this.flush();
            return status;
        }

        return null;
    },

    flush: function () {
        this.errors = [];
        this.output = [];
    }
});

module.exports = {
    SubSystem: SubSystem
};*/