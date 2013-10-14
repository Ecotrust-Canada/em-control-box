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

window.VMS = {};
VMS.sensorStates = {};
// List of selectors (or elements) that subscribe to post messages of sensor data.
VMS.subscribers = {
    ".tab-elog iframe": "http://localhost:1337"
};
// I don't expect these to change throughout a run of the software so it's
// safe to make them global (they come from SYS{} in the state file)
var fishingArea, numCams, zoomedCam = 0, aspectH, aspectV;

/**
 * Initialization after DOM loads.
 */
$(function (undef) {
    $TABS = $('.tab-body');
    
    $.getJSON('/em_state.json', function (state) {

        if (state) {

            fishingArea = state.SYS.fishingArea;
            numCams = state.SYS.numCams;

            if(fishingArea == "GM") {
                $('.RFID').hide();
                $('#diskavail_mode').val('real');
                aspectH = 16;
                aspectV = 9;
                setTimeout(function() {
                    $('.tab-cam .cameras').replaceWith(getCameraEmbeds());
                }, (48 - state.runIterations) * 1000);
            } else {
                aspectH = 4;
                aspectV = 3;
                $('.tab-cam .cameras').replaceWith(getCameraEmbeds());
            }
        }
    });

    $.getJSON('/sensorStates.json', function (states) {
        if (states) VMS.sensorStates = states;
    });

    function getAvailableDimensions() {
        var videoWidthMax = $(window).width() - 280;
        //if (videoWidthMax > 1024) videoWidthMax = 1024;

        if ($('#system-info').css('display') != 'none') {
            var videoHeightMax = $(window).height() - 162;
            //if (videoHeightMax > 768) videoHeightMax = 768;
        } else {
            var videoHeightMax = $(window).height() - 86;
            //if (videoHeightMax > 768) videoHeightMax = 768;
        }

        if (Math.floor(videoWidthMax / aspectH * aspectV) > videoHeightMax) {
            return [Math.floor(videoHeightMax / aspectV * aspectH), videoHeightMax];
        } else {
            return [videoWidthMax, Math.floor(videoWidthMax / aspectH * aspectV)];
        }
    }

    function getCameraEmbeds() {
        var viewportDims = getAvailableDimensions();
        var divOpen = '<div class="cameras" style="width: ' + viewportDims[0] + 'px; height: ' + viewportDims[1] + 'px;">';
        //var divOpen = '<div class="cameras">';
        var divClose = '</div>';

        if(fishingArea == 'GM') {
            if(numCams > 1) {
                if (zoomedCam == 0) {
                    var content = ''
                    
                    //var heightMax = $(window).height() - 88;
                    var heightMax = $(window).height() - 160;
                    
                    for (var i = 1; i <= numCams; i++) {
                        //if (i == 1 || i == 3) content = content + '<tr>';
                        //content = content + '<td><embed src="rtsp://1.1.1.' + i + ':7070/track1" type="video/mp4" width="' + Math.floor(viewportDims[0]/2) + '" height="' + Math.floor(viewportDims[1]/2) + '" /></td>';
                        content = content + '<embed src="rtsp://1.1.1.' + i + ':7070/track1" type="video/mp4" width="' + Math.floor(heightMax / 2 / aspectV * aspectH) + '" height="' + Math.floor(heightMax / 2) + '" />';
                        //if (i == 2 || i == 4) content = content + '</tr>';
                    }
                    content = content + '';
                } else {
                    var content = '<embed src="rtsp://1.1.1.' + zoomedCam + ':7070/track1" type="video/mp4" width="' + viewportDims[0] + '" height="' + viewportDims[1] + '" />';
                }

                return divOpen + content + divClose;
            } else {
                return divOpen + '<embed src="rtsp://1.1.1.1:7070/track1" type="video/mp4" width="' + viewportDims[0] + '" height="' + viewportDims[1] + '" />' + divClose;
            }
        } else {
            return divOpen + '<embed src="file:///dev/cam0" type="video/raw" width="' + viewportDims[0] + '" height="' + viewportDims[1] + '" />' + divClose;
        }
    }

    var _components = ["GPS", "RFID", "AD", "SYS"];

    $.each(_components, function (i, sys) {
        VMS[sys] = new(VMS.SENSOR_CLASSES[sys] || VMS.Component)({
            name: sys
        });
    });
    
    // dials setup
    var psi_paper = Raphael("psi_holder", 124, 124);
    var gps_paper = Raphael("gps_holder", 246, 132);
    var disk_paper = Raphael("disk_holder", 124, 124);
    var ALL_DIALS = {
            x: 2,
            y: 3,
            angle0: 230,
            r: 60,
            arange: 260
        },
        SML_DIAL = $.extend({}, ALL_DIALS, {}),
        MED_DIAL = $.extend({}, ALL_DIALS, {}),
        LRG_DIAL = $.extend({}, ALL_DIALS, {});

    VMS.GPS.speedometer = Dial($.extend({},MED_DIAL,{
        paper: gps_paper,
        label_text:"SPD",
        range: 15,
        danger: 10,
        tick_size: 5,
        y: 10,
    })).draw();

    VMS.GPS.compass = Dial($.extend({},LRG_DIAL,{
        paper: gps_paper,
        label_text:"DIR",
        x:125,
        range: 360,
        angle0:0,
        arange:360,
        tick_size: 90,
        y: 10,
    })).draw();

    VMS.AD.dial = Dial($.extend({},MED_DIAL,{
        paper: psi_paper,
        label_text:"PSI",
        range: 2500,
        danger: 2000,
        tick_size: 1250
    })).draw();

    VMS.SYS.dial = Dial($.extend({},SML_DIAL,{
        paper:disk_paper,
        label_text:"%",
        range: 100,
        danger: 90,
        tick_size: 20
    })).draw();

    noResponseCount = 0;
    noRecorderCount = 0;
    lastIterationTimer = 0;
    
    function updateUI() {
        // TODO: check for memory leaks
        $.getJSON('/em_state.json', function (status) {
            var out, err;

            if (status) {
                $('#no_response').hide();

                // Send to subscribers (currently on the Elog)
                for (var sub_key in VMS.subscribers) {
                    var win = $(sub_key).get(0).contentWindow;
                    win.postMessage(
                        status,
                        $(sub_key).attr('src')
                    );
                }

                if (parseInt('' + status.runIterations) == parseInt('' + lastIterationTimer)) {
                    if (noRecorderCount >= 2) {
                        $("#no_recorder").show();
                        console.error('ERROR: No Recorder');

                        VMS.GPS.$.addClass('inactive');
                        VMS.RFID.$.addClass('inactive');
                        VMS.AD.$.addClass('inactive');
                        VMS.SYS.$.addClass('inactive');
                    }

                    noRecorderCount++;
                } else {
                    $("#no_recorder").hide();

                    status.GPS.datetime = status.currentDateTime;

                    if (status.GPS.latitude != 0) {
                        VMS.GPS.$.removeClass('inactive');
                    } else {
                        VMS.GPS.$.addClass('inactive');
                    }
                    VMS.RFID.$.removeClass('inactive');
                    VMS.AD.$.removeClass('inactive');
                    VMS.SYS.$.removeClass('inactive');

                    if (status.GPS.state & VMS.sensorStates.GPS_ESTIMATED.flag) {
                        $('#estimated').show();
                    } else {
                        $('#estimated').hide();
                    }

                    if (status.GPS.state & VMS.sensorStates.GPS_INSIDE_FERRY_LANE.flag) {
                        $('#inside_ferry_area').show();
                    } else {
                        $('#inside_ferry_area').hide();
                    }

                    if (status.GPS.state & VMS.sensorStates.GPS_INSIDE_HOME_PORT.flag) {
                        $('#inside_home_port').show();
                    } else {
                        $('#inside_home_port').hide();
                    }

                    VMS.GPS.update(status.GPS);
                    if(status.SYS.fishingArea != "GM") VMS.RFID.update(status.RFID);
                    VMS.AD.update(status.AD);
                    VMS.SYS.update(status.SYS);

                    lastIterationTimer = status.runIterations;
                    noResponseCount = 0;
                    noRecorderCount = 0;
                }
            } else {
                if(noResponseCount >= 2) {
                    $('#no_response').show();
                    VMS.GPS.$.addClass('inactive');
                    VMS.RFID.$.addClass('inactive');
                    VMS.AD.$.addClass('inactive');
                    VMS.SYS.$.addClass('inactive');
                }
                noResponseCount++;
            }
        });

        // stuff we want to happen every interval regardless of whether we got state
        if ($('#using_os_disk').css('display') != 'none') {
            if ($('#using_os_disk h2').text() == 'USING OS DISK') {
                $('#using_os_disk h2').text('CALL ECOTRUST NOW!');
            } else {
                $('#using_os_disk h2').text('USING OS DISK');
            }
        }

        if ($('#os_disk_full').css('display') != 'none') {
            if ($('#os_disk_full h2').text() == 'OS DISK FULL') {
                $('#os_disk_full h2').text('CALL ECOTRUST NOW!');
            } else {
                $('#os_disk_full h2').text('OS DISK FULL');
            }
        }

        if ($('#data_disk_full').css('display') != 'none') {
            if ($('#data_disk_full h2').text() == 'DATA DISK FULL') {
                if(fishingArea == 'GM') {
                    $('#data_disk_full h2').text('NEW DISK REQUIRED');
                } else {
                    $('#data_disk_full h2').text('DFO REQUIRED SWAP');
                }
            } else {
                $('#data_disk_full h2').text('DATA DISK FULL');
            }
        }
    }

    $('nav li').click(function (e) {
        $('nav li').removeClass('active');
        $(this).addClass('active');

        $TABS.hide().eq($('nav ul').children().index(e.target)).show();

        if($(e.target).text() == "ELog") {
            $('#sensors').hide();
        } else {
            $('#sensors').show();
        }
    });

    $('#toggle_brightness').click(function () {
        if($('body').css("opacity") == "1") {
            $('body').css({
                "opacity": "0.5",
                'background-image': 'none'
            });
        } else {
            $('body').css({
                "opacity": "1",
                'background-image': 'url(/wood.jpg)'
            });
        }
    });

    $('.tab-cam').click(function () {
        if (numCams > 1) {
            if (zoomedCam == numCams) zoomedCam = 0;
            else if (zoomedCam < numCams) zoomedCam++;

            $('.tab-cam .cameras').replaceWith(getCameraEmbeds());
        }
    });

    $('.GPS .value').click(function () {
        if($('#latlon_mode').val() == 'dec') {
            $('#latlon_mode').val('deg');
        } else {
            $('#latlon_mode').val('dec');
        }
    });

    $('#reset_string').click(function () {
        $.post("/reset_string", {}, function(rsp) {
            if(rsp.success)
                $('.RFID .string_scans').text('0');
        });
    });
    
    $('#reset_trip').click(function () {
        $.post("/reset_trip", {}, function(rsp) {
            if(rsp.success)
                $('.RFID .trip_scans').text('0');
        });
    });

    $('button.submit_report').click(function () {
        var formdata = {};
        $('form.report textarea, form.report select').each(function () {
            formdata[this.name] = $(this).val() || $(this).text();
        });

        $.post("/report", formdata, function (rsp) {
            if (rsp.success) {
                alert("Thank you, your report has been saved");
                $('form.report textarea').val(' ');
            } else {
                alert("Failed to save report.");
            }
        });
    });

    $('button.submit_rfid').click(function () {
        var formdata = {};
        $('form.search_rfid input').each(function () {
            formdata[this.name] = $(this).val() || $(this).text();
        });

        $.post("/search_rfid", formdata, function (rsp) {
            $('.search_result').html("<tr> <th>RFID</th> <th>Location</th> <th>Date</th> <th>Soak Time</th> </tr>");
            if (rsp.success == true) {
                $('#search_rfid_error').hide();

                for(id in rsp.rfidTags) {
                    $('.search_result').append("<tr> <td>" + id
                        + "</td> <td>" + gpsKit.decimalLatToDMS(rsp.rfidTags[id].lat) + ", " + gpsKit.decimalLongToDMS(rsp.rfidTags[id].lon)
                        + "</td> <td>" + rsp.rfidTags[id].date.substring(0, rsp.rfidTags[id].date.indexOf('.'))
                        + "</td> <td>" + rsp.rfidTags[id].diff
                        + "</td> </tr>");
                }
            } else if(rsp.success == "FORMAT_ERROR") {
                $('#search_rfid_error').show();
            }            
        });
    });

    $('#system-info-button').click(function () {
        $('#system-info-button').hide();
        $('#system-info').show();

        var viewportDims = getAvailableDimensions();

        if($('.tab-cam .cameras').width() /*+ 4*/ != viewportDims[0] || $('.tab-cam .cameras').height() /*+ 2*/ != viewportDims[1]) {
            $('.tab-cam .cameras').replaceWith(getCameraEmbeds());
        }
    });

    $('#system-info').click(function () {
        $('#system-info').hide();
        $('#system-info-button').show();

        var viewportDims = getAvailableDimensions();

        if($('.tab-cam .cameras').width() /*+ 4*/ != viewportDims[0] || $('.tab-cam .cameras').height() /*+ 2*/ != viewportDims[1]) {
            $('.tab-cam .cameras').replaceWith(getCameraEmbeds());
        }
    });

    $('.SYS .available').click(function (e) {
        if (fishingArea == 'A' && e.ctrlKey) {
            if($('#diskavail_mode').val() == 'fake') {
                $('#diskavail_mode').val('real');
            } else {
                $('#diskavail_mode').val('fake');
            }
        }

        VMS.SYS.update(null, true);
    });
    
    // focus the leftmost tab
    $('nav li:nth(0)').click();
    
    setInterval(updateUI, 1010);
});
