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

VMS.Component = function (props) {
	this.$= $('.' + props.name);
	this.$value= $('.' + props.name + ' .value');
	this.$status= $('.' + props.name + '>.status');

	$.extend(this, props);
};
VMS.Component.prototype.update = function (opts) {

    if (opts.errors && opts.errors.length) {
        opts.status = opts.status || "Error";
        for(k in opts.errors)
        {
            var content = opts.errors[k];
            var lastError = $('#errors p:last').text();
            var m = lastError.match(/(.*)\((\d+)\)$/);
            if (lastError == content || (m != null && m[1] == content)) {
                var count = 2;
                if (m != null && m[2] != null) count = parseInt(m[2]) + 1;
                $('#errors p:last').html(content + "(" + count + ")");
            } else {
            	$('#errors').append("<p>" + content + "</p>");
            }
            if($('#errors').text().length > 5000)
            {
                $('#errors p').slice(0, Math.floor($('#errors p').length/2)).remove();
            }
        }
    }

    this.$status.text(opts.status == "OK" ? "OK" : opts.status);
    if (opts.value) {
        this.$value.text(opts.value);

        if (this.dial) {
	    this.dial.update(parseInt(''+opts.value));
        }
    }

    this.$.addClass(opts.status === "OK" ? "ok" : "fail").removeClass(opts.status === "OK" ? "fail" : "ok");
}

VMS.SENSOR_CLASSES = {};

/**
 * RFID sensor client
 */
VMS.SENSOR_CLASSES.RFID = function(){
	VMS.Component.apply(this, arguments);
	this.$trip_scans = $('#trip_scans');
	this.$string_scans = $('#string_scans');
}
VMS.SENSOR_CLASSES.RFID.prototype = new VMS.Component({
	name: 'RFID'
});
VMS.SENSOR_CLASSES.RFID.prototype.update = function (opts) {
    VMS.Component.prototype.update.apply(this, [opts]);
    /* also update the scan counters */
    if (opts.string_scans) {
        this.$string_scans.text(opts.string_scans);
    }
    if (opts.trip_scans) {
        this.$trip_scans.text(opts.trip_scans);
    }
};


VMS.SENSOR_CLASSES.PSI = function () {
	VMS.Component.apply(this, arguments);
};

VMS.SENSOR_CLASSES.PSI.prototype = new VMS.Component({
	name: 'PSI'
});

VMS.SENSOR_CLASSES.PSI.prototype.update = function (opts) {
    opts.value = Math.max(0,parseFloat(opts.value));
    VMS.Component.prototype.update.apply(this, [opts]);
};

/**
 * GPS sensor client
 */
VMS.SENSOR_CLASSES.GPS = function () {
	VMS.Component.apply(this, arguments);
	this.$heading = $('.GPS .heading');
	this.$speed = $('.GPS .speed');
	this.$dtime = $('.GPS .dtime');
};

VMS.SENSOR_CLASSES.GPS.prototype = new VMS.Component({
	name: 'GPS'
});
VMS.SENSOR_CLASSES.GPS.prototype.update = function (opts) {
	VMS.Component.prototype.update.apply(this, [opts]);
	if (opts.heading) {
		    this.$heading.text(opts.heading);
		    if (this.compass) this.compass.update(Math.floor(opts.heading));
	    }
	if (opts.speed) {
		    this.$speed.text(opts.speed);
		    if (this.speedometer) this.speedometer.update(Math.floor(opts.speed));
	    }
	if (opts.dtime) {
	    this.$dtime.text(opts.dtime);
	}
};

VMS.SENSOR_CLASSES.DISK = function () {
	VMS.Component.apply(this, arguments);
	this.$available = $('.DISK .available');
};

VMS.SENSOR_CLASSES.DISK.prototype = new VMS.Component({
	name: 'DISK'
});
VMS.SENSOR_CLASSES.DISK.prototype.update = function (opts) {
	VMS.Component.prototype.update.apply(this, [opts]);
	if (opts.available) {
	    if(this.last_available)
	    {
		if(this.last_available - opts.available <= 100)
		{
		    this.$available.text("calculating");
		}
		else
		{
		    var dif = this.last_available - opts.available;
		    var minutes = Math.ceil(parseFloat(opts.available) /dif *7 /60);
		    var hours = Math.floor(minutes / 60) || 0;
		    minutes = minutes - hours * 60;
		    var days = Math.floor(hours /24);
		    hours = hours - days * 24;
		    if(days != 0)
		        this.$available.text('' + days + 'd ' + hours + 'h ' + minutes + ' m');
		    else if(hours != 0)
		        this.$available.text('' + hours + 'h ' + minutes + 'm');
		    else if(minutes < 30)
		        this.$available.text("< 30m");
		    else
		        this.$available.text('' + minutes + 'm');
		}
	    }
	    this.last_available = opts.available;
	}
};
