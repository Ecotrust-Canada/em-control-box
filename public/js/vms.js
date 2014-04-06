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

// I don't expect these to change throughout a run of the software so it's
// safe to make them global (they come from SYS{} in the state file)
var fishingArea,
    numCams,
    zoomedCam = 1,
    aspectH,
    aspectV,
    noResponseCount = 0,
    noRecorderCount = 0,
    lastIteration = 0,
    video_available = false,
    recorderResponding = false,
    serverResponding = false,
    eLogURL = "http://" + window.location.hostname + ":1337/";

// List of selectors (or elements) that subscribe to post messages of sensor data.
VMS.subscribers = {
    ".tab-elog iframe": eLogURL
};

/**
 * Initialization after DOM loads.
 */
$(function (undef) {
    $TABS = $('.tab-body');

    $('.tab-elog').append("<iframe src=\"" + eLogURL + "\" frameborder=\"0\" style='overflow:auto;height:100%;width:100%' height=\"100%\" width=\"100%\"></iframe>");
    
    $.getJSON('/em_state.json', function (state) {
        if(state) {
            fishingArea = state.SYS.fishingArea;
            numCams = state.SYS.numCams;

            if(fishingArea == "A") {
                $('#diskavail_mode').val('fake');
                aspectH = 4;
                aspectV = 3;
            } else {
                $('.RFID').hide();
                $('#diskavail_mode').val('real');
                aspectH = 16;
                aspectV = 9;
            }
        }
    });

    $.getJSON('/states.json', function (states) {
        if (states) VMS.sensorStates = states;
    });

    function getAvailableDimensions() {
        var videoWidthMax = $(window).width() - 282;
        var videoHeightMax = $(window).height();
        
        // Ugly hack for when sys info box was below the videos
        /*if ($('#system-info').css('display') != 'none') {
            var videoHeightMax = $(window).height() - 162;
        } else {
            var videoHeightMax = $(window).height() - 86;
        }*/

        if (Math.floor(videoWidthMax / aspectH * aspectV) > videoHeightMax) {
            return [Math.floor(videoHeightMax / aspectV * aspectH), videoHeightMax];
        } else {
            return [videoWidthMax, Math.floor(videoWidthMax / aspectH * aspectV)];
        }
    }

    function getCameraEmbeds() {
        var viewportDims = getAvailableDimensions();
        var divOpen = '<div class="cameras" style="margin: 0;">';
        var divClose = '</div>';

        if(fishingArea == "A") {
            return divOpen + '<embed src="file:///dev/cam0" type="video/raw" width="' + viewportDims[0] + '" height="' + viewportDims[1] + '" loop=999 />' + divClose;
        } else {
            var content = '<embed src="rtsp://1.1.1.' + zoomedCam + ':7070/track1" type="video/mp4" width="' + (viewportDims[0]) + '" height="' + viewportDims[1] + '" loop=999 id=' + zoomedCam + ' />';

            for (var i = 1; i <= numCams; i++) {
                if(i == zoomedCam) continue;

                content = content + '<embed class="thumbnail" src="rtsp://1.1.1.' + i + ':7070/track1" type="video/mp4" width="' + Math.ceil(viewportDims[0] / 2) + '" height="' + Math.ceil(viewportDims[1] / 2) + '" loop=999 id=' + i + ' />';
                //content = content + '';
            }
            
            return divOpen + content + divClose;
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
    
    function updateUI() {
        // TODO: check for memory leaks
        $.getJSON('/em_state.json', function (status) {
            var out, err;

            if (status) {
                $('#no_response').hide();
                serverResponding = true;

                /*if(status.SYS.state & VMS.sensorStates.SYS_VIDEO_AVAILABLE.flag) {
                    if(!video_available) {
                        video_available = true;
                        $('.tab-cam .cameras').replaceWith(getCameraEmbeds());
                    }
                } else {
                    video_available = false;
                    $('.tab-cam .cameras').replaceWith('<div class="cameras"><p>Waiting for cameras to respond ...</p></div>');
                }*/

                if (parseInt('' + status.runIterations) == parseInt('' + lastIteration)) {
                    if (noRecorderCount >= 2) {
                        $("#no_recorder").show();
                        recorderResponding = false;
                    }

                    noRecorderCount++;
                } else {
                    $("#no_recorder").hide();
                    recorderResponding = true;

                    status.GPS.datetime = status.currentDateTime;
                    lastIteration = status.runIterations;
                    noResponseCount = 0;
                    noRecorderCount = 0;
                }

                if(!video_available) {
                    $('.tab-cam .cameras').replaceWith(getCameraEmbeds());
                    video_available = true;
                }

                // Send to subscribers (currently only the Elog)
                for (var sub_key in VMS.subscribers) {
                    var win = $(sub_key).get(0).contentWindow;
                    win.postMessage(
                        status,
                        $(sub_key).attr('src')
                    );
                }
            } else {
                if(noResponseCount >= 2) {
                    $('#no_response').show();
                    serverResponding = false;
                }
                noResponseCount++;

                status.GPS.state = 0;
                status.GPS.datetime = 0;
                status.RFID.state = 0;
                status.AD.state = 0;
                status.SYS.state = 0;
            }

            VMS.GPS.update(status.GPS);
            if(status.SYS.fishingArea != "GM") VMS.RFID.update(status.RFID);
            VMS.AD.update(status.AD);
            VMS.SYS.update(status.SYS);

            if(!recorderResponding || !serverResponding) {
                //$('#recording_status').hide();
                $('#estimated').hide();
                $('#inside_ferry_area').hide();
                $('#recording_status').hide();

                if(!recorderResponding) {
                    $('#recording_status').text(VMS.sensorStates[SYS_VIDEO_NOT_RECORDING].msg.toUpperCase());
                    $('#recording_status').show();
                }
            }
        });
    }

    $('nav li').click(function (e) {
        $('nav li').removeClass('active');
        $(this).addClass('active');

        $TABS.hide().eq($('nav ul').children().index(e.target)).show();

        if($(e.target).text() == "ELog" && $(window).width() <= 1024) {
            $('#sensors').hide();
            $('#night-mode').hide();
            $('#reload-video').hide();
        } else {
            $('#sensors').show();
            $('#night-mode').show();
            $('#reload-video').show();
        }
    });

    $('#night-mode').click(function () {
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

    $('#reload-video').click(function () {
        $('.tab-cam .cameras').replaceWith(getCameraEmbeds());
    });

    $('.tab-cam').click(function () {
        if (numCams > 1) {
            if (zoomedCam == numCams) zoomedCam = 1;
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
    });

    $('#system-info').click(function () {
        $('#system-info').hide();
        $('#system-info-button').show();
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
