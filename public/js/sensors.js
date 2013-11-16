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

    $.extend(this, props);
};

VMS.Component.prototype.update = function (opts) {
    this.$.removeClass('ok').removeClass('fail').addClass(opts.state === 0 ? "ok" : "fail");

    if(!recorderResponding || !serverResponding) {
        this.$.addClass('inactive');
        $('#recording_status').hide();
    } else {
        this.$.removeClass('inactive');
    }

    // states that will invalidate data being shown; thus the else if
    if(opts.state) { // if there is an error state
        for(var e in VMS.sensorStates) {
            if(VMS.sensorStates[e].flag & opts.state) {
                this.$value.text(VMS.sensorStates[e].msg.toUpperCase());
                break;
            }
        }
    } else if (typeof(opts.value) !== 'undefined' && this.$value.text() != opts.value) {
        this.$value.html(opts.value);
        if (this.dial) { this.dial.update(parseInt('' + opts.value)); }
    }
}

VMS.SENSOR_CLASSES = {};

/**
 * GPS sensor client
 */
VMS.SENSOR_CLASSES.GPS = function () {
    VMS.Component.apply(this, arguments);
    this.$speed = $('.GPS .speed');
    this.$heading = $('.GPS .heading');
    this.$datetime = $('.datetime');
    this.$fix_quality = $('#system-info .fix_quality');
    this.$sats_used = $('#system-info .sats_used');
    this.$latlon_mode = $('#latlon_mode');
};

VMS.SENSOR_CLASSES.GPS.prototype = new VMS.Component({
    name: 'GPS'
});

VMS.SENSOR_CLASSES.GPS.prototype.update = function (opts) {
    if(recorderResponding && serverResponding) {
        if(opts.state & VMS.sensorStates.GPS_ESTIMATED.flag) {
            $('#estimated').show();
        } else {
            $('#estimated').hide();
        }

        if (opts.state & VMS.sensorStates.GPS_INSIDE_FERRY_LANE.flag) {
            $('#inside_ferry_area').show();
        } else {
            $('#inside_ferry_area').hide();
        }

        if (opts.state & VMS.sensorStates.GPS_INSIDE_HOME_PORT.flag) {
            $('#recording_status h2').text(VMS.sensorStates[GPS_INSIDE_HOME_PORT].msg.toUpperCase());
            $('#recording_status').show();
        } else {
            $('#recording_status').hide();
        }
    }

    // we don't want to expose any of these GPS states as a sensor error
    opts.state = opts.state
        & (~VMS.sensorStates.GPS_ESTIMATED.flag)
        & (~VMS.sensorStates.GPS_INSIDE_FERRY_LANE.flag)
        & (~VMS.sensorStates.GPS_INSIDE_HOME_PORT.flag);
    
    if (this.$latlon_mode.val() == 'dec') {
        opts.value = opts.latitude + ',' + opts.longitude;
    } else {
        opts.value = gpsKit.decimalLatToDMS(opts.latitude) + '<br>' + gpsKit.decimalLongToDMS(opts.longitude);
    }

    VMS.Component.prototype.update.apply(this, [opts]);

    if(typeof(opts.datetime) !== 'undefined') this.$datetime.text(opts.datetime.substring(0, opts.datetime.indexOf('.')));

    opts.speed = Math.floor(opts.speed);
    opts.heading = Math.floor(opts.heading);

    if (this.$speed.text() != opts.speed) {
        this.$speed.text(opts.speed);
        if (this.speedometer) this.speedometer.update(opts.speed);
    }

    if (this.$heading.text() != opts.heading) {
        this.$heading.text(opts.heading);
        if (this.compass) this.compass.update(opts.heading);
    }

    switch (opts.satQuality) {
        case 0: opts.satQuality = '0 (NO FIX)'; break;
        case 1: opts.satQuality = '1 (GPS FIX)'; break;
        case 2: opts.satQuality = '2 (DGPS FIX)'; break;
    }

    if (this.$fix_quality.text() != opts.satQuality) {
        this.$fix_quality.text(opts.satQuality);
    }

    if (this.$sats_used.text() != opts.satsUsed) {
        this.$sats_used.text(opts.satsUsed);
    }
};

/**
 * RFID sensor client
 */
VMS.SENSOR_CLASSES.RFID = function(){
    VMS.Component.apply(this, arguments);
    this.$trip_scans = $('.RFID .trip_scans');
    this.$string_scans = $('.RFID .string_scans');
}

VMS.SENSOR_CLASSES.RFID.prototype = new VMS.Component({
    name: 'RFID'
});

VMS.SENSOR_CLASSES.RFID.prototype.update = function (opts) {
    if (!opts.lastScannedTag) {
        opts.lastScannedTag = 'no scans yet';
        $('.RFID').addClass('warn');
    } else {
        $('.RFID').removeClass('warn');
    }

    opts.value = opts.lastScannedTag;
    VMS.Component.prototype.update.apply(this, [opts]);

    /* also update the scan counters */
    if (this.$string_scans.text() != opts.stringScans) { this.$string_scans.text(opts.stringScans); }
    if (this.$trip_scans.text() != opts.tripScans) { this.$trip_scans.text(opts.tripScans); }
};

/**
 * AD sensor client
 */
VMS.SENSOR_CLASSES.AD = function () {
    VMS.Component.apply(this, arguments);
    this.$battery = $('#system-info .battery');
};

VMS.SENSOR_CLASSES.AD.prototype = new VMS.Component({
    name: 'AD'
});

VMS.SENSOR_CLASSES.AD.prototype.update = function (opts) {
    opts.state = opts.state & (~VMS.sensorStates.AD_BATTERY_LOW.flag);
    opts.value = Math.max(0, Math.floor(opts.psi));
    VMS.Component.prototype.update.apply(this, [opts]);
    this.$battery.text(opts.battery.toFixed(2));
};

/**
 * SYS sensor client
 */
VMS.SENSOR_CLASSES.SYS = function () {
    VMS.Component.apply(this, arguments);
    this.$available = $('.SYS .available');
    this.$disk_number = $('.SYS .diskNumber');
    this.$diskavail_mode = $('#diskavail_mode');
    this.$uptime = $('#system-info .uptime');
    this.$load = $('#system-info .load');
    this.$cpu_percent = $('#system-info .cpu_percent');
    this.$ram_free = $('#system-info .ram_free');
    this.$ram_total = $('#system-info .ram_total');
    this.$temp_core0 = $('#system-info .temp_core0');
    this.$temp_core1 = $('#system-info .temp_core1');
    this.$os_free = $('#system-info .os_free');
    this.$os_total = $('#system-info .os_total');
    this.$data_free = $('#system-info .data_free');
    this.$data_total = $('#system-info .data_total');
};

VMS.SENSOR_CLASSES.SYS.prototype = new VMS.Component({
    name: 'SYS'
});

delayCounter = 999;
blocksToMB = 4096 / 1024 / 1024;
lastPercentUsed = 999;

VMS.SENSOR_CLASSES.SYS.prototype.update = function (opts, force_update) {
    if(typeof(force_update) !== 'undefined') {
        delayCounter = 999;
        return; // WTF?
    }

    if(recorderResponding && serverResponding) {
        if (opts.state & VMS.sensorStates.SYS_VIDEO_RECORDING.flag) {
            $('#recording_status h2').text(VMS.sensorStates.SYS_VIDEO_RECORDING.msg.toUpperCase());
        } else {
            $('#recording_status h2').text(VMS.sensorStates.SYS_VIDEO_NOT_RECORDING.msg.toUpperCase());
        }

        $('#recording_status').show();
    }

    if(lastIteration % 2 == 0) {
        if($('#using_os_disk').css('display') != 'none')  $('#using_os_disk h2').text('USING OS DISK');
        if($('#data_disk_full').css('display') != 'none') $('#data_disk_full h2').text('DATA DISK FULL');
        if($('#os_disk_full').css('display') != 'none')   $('#os_disk_full h2').text('OS DISK FULL');
    } else {
        if($('#using_os_disk').css('display') != 'none')  $('#using_os_disk h2').text('CALL ECOTRUST NOW!');
        if($('#data_disk_full').css('display') != 'none') {
            if(fishingArea == "A") $('#data_disk_full h2').text('DFO REQUIRED SWAP');
            else $('#data_disk_full h2').text('NEW DISK REQUIRED');
        }
        if($('#os_disk_full').css('display') != 'none')   $('#os_disk_full h2').text('CALL ECOTRUST NOW!');
    }

    opts.state = opts.state
        & (~VMS.sensorStates.SYS_VIDEO_AVAILABLE.flag)
        & (~VMS.sensorStates.SYS_VIDEO_RECORDING.flag)
        & (~VMS.sensorStates.SYS_VIDEO_NOT_RECORDING.flag);

    if(delayCounter > 3 && lastIteration >= 8) {
        var osDiskPercentUsed = Math.floor(100 * (1 - opts.osDiskFreeBlocks / opts.osDiskTotalBlocks));

        if (opts.dataDiskPresent == "true") {
            if (this.$diskavail_mode.val() == "fake") {
                var diskMinutesLeft = opts.dataDiskMinutesLeftFake;
            } else {
                var diskMinutesLeft = opts.dataDiskMinutesLeft;
            }
            
            if (opts.fishingArea == 'A') var dataDiskPercentUsed = Math.floor(100 * (1 - opts.dataDiskMinutesLeftFake / 30240));
            else var dataDiskPercentUsed = Math.floor(100 * (1 - opts.dataDiskFreeBlocks / opts.dataDiskTotalBlocks));
            
            var percentUsed = dataDiskPercentUsed;
            this.$disk_number.text('#' + opts.dataDiskLabel.substring(5, opts.dataDiskLabel.length));
            opts.state = opts.state || 0;
            $('#using_os_disk').hide();
        } else {
            var diskMinutesLeft = opts.osDiskMinutesLeft;
            var percentUsed = osDiskPercentUsed;
            this.$disk_number.text('OS');
            //opts.state = 1;
            $('#using_os_disk').show();
        }

        if (this.dial && percentUsed != lastPercentUsed) {
            this.dial.update(percentUsed);
            lastPercentUsed = percentUsed;
        }

        if (!isNaN(diskMinutesLeft)) {
            if(opts.fishingArea == 'A' && diskMinutesLeft < 3 * 24 * 60 ||
                diskMinutesLeft < 1 * 12 * 60) {
                if(opts.dataDiskPresent == "true") opts.state = opts.state | VMS.sensorStates.SYS_DATA_DISK_FULL.flag;
                else opts.state = opts.state | VMS.sensorStates.SYS_OS_DISK_FULL.flag;
            } else {
                opts.state = opts.state || 0;
            }

            VMS.Component.prototype.update.apply(this, [opts]);

            var minutes = diskMinutesLeft;
            var hours = Math.floor(minutes / 60) || 0;
            minutes = minutes - hours * 60;
            var days = Math.floor(hours /24);
            hours = hours - days * 24;
            if(days != 0) this.$available.text('' + days + 'd ' + hours + 'h'/* + minutes + 'm'*/);
            else if(hours != 0) this.$available.text('' + hours + 'h'/* + minutes + 'm'*/);
            else if(minutes < 30 && minutes > 0) this.$available.text("< 30m");
            else if(minutes == 0) this.$available.text("FULL");
            else this.$available.text('' + minutes + 'm');

            lastMinutesLeft = diskMinutesLeft;
        }

        this.$uptime.text(opts.uptime);
        this.$load.text(opts.load);
        this.$cpu_percent.text(opts.cpuPercent);
        this.$ram_free.text(Math.floor(opts.ramFreeKB / 1024));
        this.$ram_total.text(Math.floor(opts.ramTotalKB / 1024));
        this.$temp_core0.text(opts.tempCore0);
        this.$temp_core1.text(opts.tempCore1);
        this.$os_free.text(Math.floor(opts.osDiskFreeBlocks * blocksToMB));
        this.$os_total.text(Math.floor(opts.osDiskTotalBlocks * blocksToMB));
        
        if (opts.dataDiskPresent == "true") {
            // <= 3 days for Area A, <= 1 day for Maine
            if(opts.state & VMS.sensorStates.SYS_DATA_DISK_FULL.flag ||
               opts.fishingArea == 'A' && opts.dataDiskMinutesLeftFake <= 4320 ||
               opts.dataDiskMinutesLeft <= 720) {
                $('#data_disk_full').show();
            } else {
                $('#data_disk_full').hide();
            }

            this.$data_free.text(Math.floor(opts.dataDiskFreeBlocks * blocksToMB));
            this.$data_total.text(Math.floor(opts.dataDiskTotalBlocks * blocksToMB));
        } else {
            this.$data_free.text("N/A");
            this.$data_total.text("N/A");
        }

        if(opts.state & VMS.sensorStates.SYS_OS_DISK_FULL.flag) {
        /*if (osDiskPercentUsed >= 90) {*/
            $('#os_disk_full').show();
        } else {
            $('#os_disk_full').hide();
        }

        delayCounter = 0;
    } else {
        delayCounter++;
    }
};
