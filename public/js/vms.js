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
VMS.stateDefinitions = {};
VMS.optionDefinitions = {};
VMS.lastIteration = 0;
VMS.recorderResponding = false;
VMS.serverResponding = false;
VMS.OPTIONS = {};
VMS.SYS = {};
VMS.GPS = {};
VMS.AD = {};
VMS.RFID = {};
VMS.UISensors = {};

function isSet(state) {
    var component = state.substr(0, state.indexOf('_'));

    if (component == "OPTIONS") {
        if(VMS.OPTIONS.state & VMS.optionDefinitions[state].flag) return true;
    } else {
        if(VMS[component].state & VMS.stateDefinitions[state].flag) return true;
    }

    return false;
}

function getMsg(state, isset) {
    var msg;
    var msgIndex = VMS.lastIteration % 2;

    if (isset) {
        if (VMS.stateDefinitions[state].msg_set.length == 1) msgIndex = 0;
        msg = VMS.stateDefinitions[state].msg_set[msgIndex];
    } else {
        if (VMS.stateDefinitions[state].msg_unset.length == 1) msgIndex = 0;
        msg = VMS.stateDefinitions[state].msg_unset[msgIndex];
    }

    return msg;
}

/**
 * Initialization after DOM loads.
 */
$(function (undef) {
    var zoomedCam = 1,
        aspectH,
        aspectV,
        noResponseCount = 0,
        noRecorderCount = 0,
        videoPreviewLoaded = false,
        eLogURL = "http://" + window.location.hostname + ":1337/";

    // dials setup
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

    $TABS = $('.tab-body');
    
    $.getJSON('/states.json', function (definitions) {
        if (definitions) {
            VMS.stateDefinitions = definitions;
        }
    });

    $.getJSON('/options.json', function (options) {
        if (options) {
            VMS.optionDefinitions = options;
        }

        $.getJSON('/em_state.json', function (state) {
            if(state) {
                VMS.OPTIONS = state.OPTIONS;

                // List of selectors (or elements) that subscribe to post messages of sensor data.
                VMS.subscribers = {
                    ".tab-elog iframe": eLogURL
                };

                // SYS
                VMS.SYS = state.SYS;
                VMS.UISensors.SYS = new(VMS.SENSOR_CLASSES.SYS)({
                    name: "SYS"
                });
                var disk_paper = Raphael("disk_holder", 124, 124);
                VMS.UISensors.SYS.dial = Dial($.extend({},SML_DIAL,{
                    paper:disk_paper,
                    label_text:"%",
                    range: 100,
                    danger: 90,
                    tick_size: 20
                })).draw();

                // GPS
                VMS.UISensors.GPS = new(VMS.SENSOR_CLASSES.GPS)({
                    name: "GPS"
                });
                var gps_paper = Raphael("gps_holder", 246, 132);
                VMS.UISensors.GPS.speedometer = Dial($.extend({},MED_DIAL,{
                    paper: gps_paper,
                    label_text: "SPD",
                    range: 15,
                    danger: 10,
                    tick_size: 5,
                    y: 10,
                })).draw();
                VMS.UISensors.GPS.compass = Dial($.extend({},LRG_DIAL,{
                    paper: gps_paper,
                    label_text:"DIR",
                    x:125,
                    range: 360,
                    angle0:0,
                    arange:360,
                    tick_size: 90,
                    y: 10,
                })).draw();

                if (isSet("OPTIONS_USING_AD")) {
                    VMS.UISensors.AD = new(VMS.SENSOR_CLASSES.AD)({
                        name: "AD"
                    });
                    var psi_paper = Raphael("psi_holder", 124, 124);
                    VMS.UISensors.AD.dial = Dial($.extend({},MED_DIAL,{
                        paper: psi_paper,
                        label_text:"PSI",
                        range: 2500,
                        danger: 2000,
                        tick_size: 1250
                    })).draw();
                }

                if (isSet("OPTIONS_USING_RFID")) {
                    VMS.RFID = state.RFID;
                    VMS.UISensors.RFID = new(VMS.SENSOR_CLASSES.RFID)({
                        name: "RFID"
                    });
                } else {
                    $('.RFID').hide();
                }

                if (isSet("OPTIONS_USING_DIGITAL_CAMERAS")) {
                    aspectH = 16;
                    aspectV = 9;
                } else if (isSet("OPTIONS_USING_ANALOG_CAMERAS")) {
                    aspectH = 4;
                    aspectV = 3;
                }

                if (VMS.SYS.fishingArea == "A") {
                    $('#diskavail_mode').val('fake');
                } else {
                    $('#diskavail_mode').val('real');
                }
            }
        });
    });

    $('.tab-elog').append("<iframe src=\"" + eLogURL + "\" frameborder=\"0\" style='overflow:auto;height:100%;width:100%' height=\"100%\" width=\"100%\"></iframe>");

    function getAvailableDimensions(extra_height_subtract) {
        extra_height_subtract = extra_height_subtract || 0;
        var videoWidthMax = $(window).width() - 270; //orig 280
        var videoHeightMax = $(window).height() - 34 - extra_height_subtract;

        //alert(videoWidthMax + ' ' + videoHeightMax);

        // ROUND or FLOOR?
        if (Math.round(videoWidthMax / aspectH * aspectV) > videoHeightMax) {
            return [Math.round(videoHeightMax / aspectV * aspectH), videoHeightMax];
        } else {
            return [videoWidthMax, Math.round(videoWidthMax / aspectH * aspectV)];
        }
    }

    function getCameraEmbeds() {
        var viewportDims = getAvailableDimensions();
        var thumbDims = getAvailableDimensions(viewportDims[1]);
        var divOpen = '<div class="cameras" style="margin: 0; width: ' + viewportDims[0] + 'px; height: ' + viewportDims[1] + 'px;">';
        var divClose = '</div>';

        if (isSet("OPTIONS_USING_ANALOG_CAMERAS")) {
            return divOpen + '<embed src="file:///dev/cam0" type="video/raw" width="' + viewportDims[0] + '" height="' + viewportDims[1] + '" loop=999 />' + divClose;

        } else if (isSet("OPTIONS_USING_DIGITAL_CAMERAS")) {
            var content = '<embed src="rtsp://1.1.1.' + zoomedCam + ':7070/track1" type="video/mp4" width="' + (viewportDims[0]) + '" height="' + viewportDims[1] + '" loop=999 id=' + zoomedCam + ' />';

            for (var i = 1; i <= VMS.SYS.numCams; i++) {
                if(i == zoomedCam) continue;

                content = content + '<embed class="thumbnail" src="rtsp://1.1.1.' + i + ':7070/track1" type="video/mp4" width="' + Math.round(thumbDims[0]) + '" height="' + Math.round(thumbDims[1]) + '" loop=999 id=' + i + ' />';
                //content = content + '';
            }
            
            // hack to give us two more cams b/c we don't have 4 in the lab
            /*
            content = content + '<embed class="thumbnail" src="rtsp://1.1.1.1:7070/track1" type="video/mp4" width="' + Math.round(thumbDims[0]) + '" height="' + Math.round(thumbDims[1]) + '" loop=999 id=' + i + ' />';

            content = content + '<embed class="thumbnail" src="rtsp://1.1.1.2:7070/track1" type="video/mp4" width="' + Math.round(thumbDims[0]) + '" height="' + Math.round(thumbDims[1]) + '" loop=999 id=' + i + ' />';
            */

            return divOpen + content + divClose;
        }
    }

    /*
    var _components = ["SYS", "GPS", "AD", "RFID" ];
    $.each(_components, function (i, sys) {
        VMS.UISensors[sys] = new(VMS.SENSOR_CLASSES[sys] || VMS.Component)({
            name: sys
        });
    });
*/

    function updateUI() {
        // TODO: check for memory leaks
        $.getJSON('/em_state.json', function (em_state) {
            //var out, err;

            if (em_state) {
                $('#no_response').hide();
                serverResponding = true;

                if (parseInt('' + em_state.runIterations) == parseInt('' + VMS.lastIteration)) {
                    if (noRecorderCount >= 2) {
                        $("#no_recorder").show();
                        recorderResponding = false;

                        VMS.UISensors.GPS.disable(VMS.GPS);
                        VMS.UISensors.SYS.disable(VMS.SYS);
                        if (isSet("OPTIONS_USING_RFID")) VMS.UISensors.RFID.disable(VMS.RFID);
                        if (isSet("OPTIONS_USING_AD")) VMS.UISensors.AD.disable(VMS.AD);
                    }

                    noRecorderCount++;
                } else {
                    $("#no_recorder").hide();
                    recorderResponding = true;

                    em_state.GPS.datetime = em_state.currentDateTime;
                    VMS.lastIteration = em_state.runIterations;
                    noResponseCount = 0;
                    noRecorderCount = 0;
                }

                // Send to subscribers (currently only the Elog)
                for (var sub_key in VMS.subscribers) {
                    var win = $(sub_key).get(0).contentWindow;
                    win.postMessage(
                        em_state,
                        $(sub_key).attr('src')
                    );
                }
            } else {
                if(noResponseCount >= 2) {
                    $('#no_response').show();
                    serverResponding = false;

                    VMS.UISensors.GPS.disable(VMS.GPS);
                    VMS.UISensors.SYS.disable(VMS.SYS);
                    if (isSet("OPTIONS_USING_RFID")) VMS.UISensors.RFID.disable(VMS.RFID);
                    if (isSet("OPTIONS_USING_AD")) VMS.UISensors.AD.disable(VMS.AD);
                }

                noResponseCount++;
            }

            if(serverResponding && recorderResponding) {
                VMS.SYS = em_state.SYS;
                VMS.UISensors.SYS.update(VMS.SYS);

                VMS.GPS = em_state.GPS;
                VMS.UISensors.GPS.update(VMS.GPS);

                if (isSet("OPTIONS_USING_RFID")) {
                    VMS.RFID = em_state.RFID;
                    VMS.UISensors.RFID.update(VMS.RFID);
                }

                if (isSet("OPTIONS_USING_AD")) {
                    VMS.AD = em_state.AD;
                    VMS.UISensors.AD.update(VMS.AD);
                }

                if (!videoPreviewLoaded && (VMS.SYS.uptime.match(/(\d+)m/)[1] >= 2 || VMS.lastIteration >= 40)) {
                    videoPreviewLoaded = true;
                    $('.tab-cam .cameras').replaceWith(getCameraEmbeds());
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
            //$('#reload-video').hide();
        } else if($(e.target).text() == "Search") {
            $('#sensors').show();
            $('#night-mode').show();
            $('#system-info').hide();
            $('#system-info-button').show();

            if(!isSet("OPTIONS_USING_RFID")) {
                $('.tab-search').hide();
            }
        } else {
            $('#sensors').show();
            $('#night-mode').show();
            //$('#reload-video').show();
            $('#system-info').hide();
            $('#system-info-button').hide();
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

    //$('#reload-video').click(function () {
    //    $('.tab-cam .cameras').replaceWith(getCameraEmbeds());
    //});

    $('.tab-cam').click(function () {
        if (VMS.SYS.numCams > 1) {
            if (zoomedCam == VMS.SYS.numCams) zoomedCam = 1;
            else if (zoomedCam < VMS.SYS.numCams) zoomedCam++;

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
        if (VMS.SYS.fishingArea == 'A' && e.ctrlKey) {
            if($('#diskavail_mode').val() == 'fake') {
                $('#diskavail_mode').val('real');
            } else {
                $('#diskavail_mode').val('fake');
            }
        }

        VMS.UISensors.SYS.update(null, true);
    });
    
    // focus the leftmost tab
    $('nav li:nth(0)').click();
    
    setInterval(updateUI, 1010);
});
