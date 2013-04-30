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

window.VMS = {
	// camera state enum:
	ALL_CAMS: 4,

    //Display the non-editable elog top info.
    display_elog_top: function(ELOG_TOP){
        $('#skipper').val(ELOG_TOP.skipper);
        $('#fin').val(ELOG_TOP.fin);
//        $('td[display="to_disappear"]').html('');
        $('#grounding_lines').get(0).checked=ELOG_TOP.fm_grounding_lines;
        $('#singles').get(0).checked=ELOG_TOP.fm_singles;
        $('#jars').get(0).checked=ELOG_TOP.bf_jars;
        $('#clips').get(0).checked=ELOG_TOP.bf_clips;
        $('#cages').get(0).checked=ELOG_TOP.bf_cages;
        
        $.fn.setIfVal = function(val) {
            $(this).each(function(){
                $(this).get(0).checked = (val == $(this).val());
            });
        }

        $('#bait_type').val(ELOG_TOP.bait_type);
        $('#compound').val(ELOG_TOP.compound).change();

        $('#another_bait_type').val(ELOG_TOP.another_bait_type);
        $('#depths1 input').setIfVal(ELOG_TOP.depths);
        $('#depths2 input').setIfVal(ELOG_TOP.depths);
        $('#catch_weights1 input').setIfVal(ELOG_TOP.catch_weights);
        $('#catch_weights2 input').setIfVal(ELOG_TOP.catch_weights);
//        $('#save_top').hide();
    },

    //Display elog in the table.
    display_elog: function(){
        $('#elog_data_table tr').slice(4).remove();
        var species_index = 6;
        var toSpeciesName = function (code) {
            var name = "";
            switch(code)
            {
                case "XKG":
                    name = "Dungeness";
                    break;
                case "XLA":
                    name = "Red Rock";
                    break;
                case "VIA":
                    name = "KingCrab";
                    break;
                case "VNH":
                    name = "Red King";
                    break;
                case "VMC":
                    name = "Brown (Golden) King";
                    break;
                case "ZAD":
                    name = "Tanner Crab";
                    break;
                default:
                    name = "Unknown";
                    break;
            };
            return name;
        };
        $.post("/display_elog", {}, function (rsp) {
            if(rsp.success) {
                var months = ["", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];
                for(i in rsp.elog)
                {
                    var line = rsp.elog[i];
                    if(line.length < 10)
                        continue;
                    var logs = line.split(",");
                    if(logs.length < 15)
                        continue;
                    var tr = "<tr><td colspan='4'>";
                    tr += months[parseInt(logs[0])] + " " + logs[1] + ", " + logs[2] + ":" + logs[3];
                    tr += "</td>";
                    for(var j = 4; j< logs.length; j++)
                    {
                        var log = logs[j];
                        if(j == species_index)
                            log = toSpeciesName(log);
                        tr += "<td>" + log  +"</td>";
                    }
                    tr += "</tr>\n";
                    $('#elog_data_table tr:last').before(tr);
                }
            }
        });
    }
};

/**
 * Initialization after DOM loads.
 */
$(function (undef) {

    $.post("/get_trip_and_string", {}, function(rsp) {
        if(rsp.success)
        {
            $('#string_scans').text('' + rsp.string_scans);
            $('#trip_scans').text('' + rsp.trip_scans);
        }
    });

    $('#reset_string').click(function () {
        $.post("/reset_string", {}, function(rsp) {
            if(rsp.success)
                $('#string_scans').text('0');
        });
    });
    
    $('#reset_trip').click(function () {
        $.post("/reset_trip", {}, function(rsp) {
            if(rsp.success)
                $('#trip_scans').text('0');
        });
    });

    $TABS = $('.tab-body');

    $('.tabs li').click(function (e) {
        $('.tabs li').removeClass('active');
        $(this).addClass('active');

        $TABS.hide().eq($('.tabs').children().index(e.target)).show();
        if($(e.target).text() == "Elog")
        {
            $('#main-panel').hide();
            $('#overall').show();
        }
        else
        {
            $('#main-panel').show();
            $('#overall').hide();
        }
    });

    $('#compound').change(function () {
        if($('#compound').val() != "Single Bait Type") {
                $('#another_bait_type').show();
            } else {
                $('#another_bait_type').hide();
            }
    });

    //Display elog top.
    $.post("/display_elog_top", {}, function(rsp) {
        if(rsp.success)
            VMS.display_elog_top(rsp.ELOG_TOP);
    });

    //Display elog table.
    VMS.display_elog();

    $('#save_top').click(function () {
        var ELOG_TOP = {};
        ELOG_TOP.skipper = $('#skipper').val();
        ELOG_TOP.fin = $('#fin').val();
        ELOG_TOP.fm_grounding_lines = $('#grounding_lines:checked').val();
        ELOG_TOP.fm_singles = $('#singles:checked').val();
        ELOG_TOP.bf_jars = $('#jars:checked').val();
        ELOG_TOP.bf_cages = $('#cages:checked').val();
        ELOG_TOP.bf_clips = $('#clips:checked').val();
        ELOG_TOP.bait_type = $('#bait_type').val();
        ELOG_TOP.compound = $('#compound').val();
        ELOG_TOP.another_bait_type = $('#another_bait_type').val();
        ELOG_TOP.depths = $('input:radio[name=depths]:checked').val();
        ELOG_TOP.catch_weights = $('input:radio[name=catch_weights]:checked').val();

        $.post("/save_elog_top", ELOG_TOP, function (rsp) {
            if(rsp.success) {
                $('#elog_top_error').hide();
                VMS.display_elog_top(ELOG_TOP);
                alert('Saved new ELOG page. This information will apply to all new entries.');
            }
            else if(rsp.error)
            {
                $('#elog_top_error').text(rsp.error);
                $('#elog_top_error').show();
            }
        });
    });

    //Update elog info. 
    $.post("/retrieve_elog_info", {}, function (rsp) {
        if(rsp.success) {
            $('#vrn').text(rsp.vrn);
            $('#vessel').text(rsp.vessel);
            $('#year').text(rsp.year);
        }
    });

    $('#toggle_brightness').click(function () {

        if($('body').css("opacity") == "1") {
            $('body').css({
                "opacity": "0.5",
                'background-image': 'none'
            });
            $('#toggle_brightness').text('day mode');
        } else {
            $('body').css({
                "opacity": "1",
                'background-image': 'url(/wood_07.JPG)'
            });
            $('#toggle_brightness').text('night mode');
        }
    });

    $('#import_date').click(function () {
        $.post("/import_date", {}, function (rsp) {
            if(rsp.success) {
                $('#date_hauled_month option[value=' + rsp.month  + ']').attr('selected', 'selected');
                $('#date_hauled_day').val(rsp.day);
                $('#date_hauled_hour').val(rsp.hour);
                $('#date_hauled_minute').val(rsp.minute);
            }
        });
    });


    $('#add_elog_row').click(function () {
        var formdata = {};
        formdata.date_hauled_month = $('#date_hauled_month').val();
        formdata.date_hauled_day = $('#date_hauled_day').val();
        formdata.date_hauled_hour = $('#date_hauled_hour').val();
        formdata.date_hauled_minute = $('#date_hauled_minute').val();
        formdata.depth_min = $('#depth_min').val();
        formdata.depth_max = $('#depth_max').val();
        formdata.species = $('#species').val();
        formdata.number_of_pieces = $('#number_of_pieces').val();
        formdata.weight_of_catch = $('#weight_of_catch').val();
        formdata.released_num = $('#released_num').val();
        formdata.released_wt = $('#released_wt').val();
        formdata.kept_num = $('#kept_num').val();
        formdata.kept_wt = $('#kept_wt').val();
        formdata.pbs_code = $('#pbs_code').val();
        formdata.remarks = $('#remarks').val();
        $.post("/log_elog_row", formdata, function (rsp) {
            if(rsp.success) {
                $('#elog_data_table td').removeClass('fail');
                alert("Log added successfully.");
                $('#elog_table_row input').val('');
                VMS.display_elog();
            }
            else if(rsp.error)
            {
                $('#elog_data_table td').removeClass('fail');
                $('#' + rsp.error).parent().addClass('fail');
            }
        });
    });

    $('button.submit_report').click(function () {
        var formdata = {};
        $('form.report textarea, form.report select').each(function () {
            formdata[this.name] = $(this).val() || $(this).text();
        });
        $.post("/report", formdata, function (rsp) {
            if (rsp.success) {
                alert("Thankyou, your report has been saved.");
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
        var gpsFix = function (gps) {
            var fgps = parseFloat(gps);
            var sgn = Math.abs(gps)/gps;
            var dgps = sgn * Math.floor(sgn*gps);
            var fgps = Math.abs((fgps - dgps)*60).toFixed(2);
            var sfgps = '';
            if(fgps < 10)
                sfgps = '0' + fgps;
            else
                sfgps = '' + fgps;
            return '' + dgps + ',' + sfgps;
        };
        $.post("/search_rfid", formdata, function (rsp) {
            $('.search_result').html("<tr> <th>RFID</th> <th>Location</th> <th>Date</th> <th>Soak Time</th> </tr>");
            if (rsp.success == true) {
                $('#search_rfid_error').hide();
                for(id in rsp.rfids)
                {
                    ddate = new Date(rsp.rfids[id].ddate);
                    var days_between = function(d1, d2){
                        var one_day = 1000*60*60*24;
                        var ms_between = Math.abs(d1.getTime() - d2.getTime());
                        return parseInt(ms_between / one_day);
                    };
                    $('.search_result').append("<tr> <td>" + id  + "</td> <td>" + gpsFix(rsp.rfids[id].lat) + "," + gpsFix(rsp.rfids[id].lng) + "</td> <td>" + ddate.toDateString() + "</td> <td>" + days_between(new Date(), ddate) + "</td> </tr>");
                }
            }
            else if(rsp.success == "FORMAT_ERROR")
            {
                $('#search_rfid_error').show();
            }
            else
                $('#search_rfid_error').hide();
        });
    });

    var _components = ["PSI", "RFID", "GPS", "VIDEO", "CPU", "MEM", "OSDISK", "DISK", "BATTERY", "TEMP"];
    $.each(_components, function (i, sys) {
        VMS[sys] = new(VMS.SENSOR_CLASSES[sys] || VMS.Component)({
            name: sys
        });
    });

    // DEPRECATED
    var OVERALL = new VMS.Component({
        size: 48,
        name: "OVERALL"
    });
    
    /**
     * Heartbeat
     */
    noResponseCount = 0;
    outFerryAreaCount = 0;
    console.log ("getting heartbeat");
    setInterval(function () {
	// TODO: check for memory leaks
        $.getJSON('/em_state.json', function (status) {
            var out, err;
            if (status) {
                noResponseCount = 0;
                $('#no_response').hide();
                var overall = 'OK';
                var ferry = false;
                if (status.sensors) {
                    for (var sensor in status.sensors) {
                        if (_components.indexOf(sensor) != -1) {
                            VMS[sensor].update(status.sensors[sensor]);
                            if(sensor != 'BATTERY' && status.sensors[sensor].status && status.sensors[sensor].status!= 'OK')
                                overall = sensor + ':' + status.sensors[sensor].status;
                        }
                        if (sensor == "FERRY" && status.sensors[sensor].status)
                        {
                            outFerryAreaCount = 0;
                            ferry = true;
                            $('#inside_ferry_area').show();
                        }
                    }
                    if (!ferry)
                    {
                        outFerryAreaCount = outFerryAreaCount + 1;
                        if(outFerryAreaCount >= 3)
                            $('#inside_ferry_area').hide();
                    }
                }
                if (status.messages) {
                    for (var component in status.messages) {
                        if (_components.indexOf(component) != -1) {
                            VMS[component].update(status.messages[component]);
                        }
                    }
                }
                if (status.video) {
                    VMS.VIDEO.update(status.video);
                    if(status.video.status && status.video.status != 'OK')
                        overall = 'VID:' + status.video.status;
                    if(!status.video.awayFromHomePort)
                    {
                        $('#homePortMsg').show();
                    }
                    else
                    {
                        $('#iframe').show();
                        $('#homePortMsg').hide();
                    }
                }
                OVERALL.update({status:overall});
                
                // system-wide errors.
                if (status.sensors.errors) {
                    for(k in status.sensors.errors)
                    {
                        var content = status.sensors.errors[k];
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
            }
            else
            {
                noResponseCount = noResponseCount + 1;
                if(noResponseCount >= 3)
                    $('#no_response').show();
            }

        });
    }, 1000);
   
    // focus the leftmost tab.
	$('.tabs li:nth(0)').click();
    var resetCamFrame = function() {
        // copy existing iframe settings
        var iframe = $('#iframe iframe').get(0);
            full = iframe ? iframe.contentWindow.full : VMS.ALL_CAMS,
            src = '/camFrame.html?full=' + full,

            w = $(window).width() - 254,
            h = $(window).height() - 27;

	var new_iframe = $('<iframe src="'+src+'" width="'+w+'"px" height="'+h+'px" SCROLLING=NO> <p>Your browser does not support iframes.</p> </iframe>');
        $('#iframe').width(w).prepend(new_iframe);
        
        // remove old iframe if it exists
        if (iframe) {
            setTimeout(function() {
		new_iframe.get(0).contentWindow.full = full;
                $('#iframe iframe:last').remove();
            }, 1000);
        }
    };
    
    resetCamFrame();

    $(window).resize(function(){
        w = $(window).width() - 254;
        h = $(window).height() - 27;
        $('#iframe').width(w).find('iframe').width(w).height(h);        
    }).resize();

    setInterval(resetCamFrame,30000);

	// dials setup
    var paper = Raphael("holder",500,100);
    var psi_paper = Raphael("psi_holder",120,100);
    var gps_paper = Raphael("gps_holder",225,100);
    var disk_paper = Raphael("disk_holder",120,100);
	var ALL_DIALS = {
            paper:paper,
			x:1,y:1,
			angle0: 230,
            r:48,
			arange: 260
		},
		SML_DIAL = $.extend({},ALL_DIALS,{
		}),
		MED_DIAL = $.extend({},ALL_DIALS,{
		}),
		LRG_DIAL = $.extend({},ALL_DIALS,{
		});
	
	VMS.BATTERY.dial = Dial($.extend({},SML_DIAL,{
        label_text: "dc in",
		range: 16,
		danger: 14,
		tick_size: 4
	})).draw();

	VMS.GPS.speedometer = Dial($.extend({},MED_DIAL,{
        paper:gps_paper,
        label_text:"SPD",
        x:20, y:3,
		range: 15,
		danger: 10,
		tick_size: 5
	})).draw();

	VMS.GPS.compass = Dial($.extend({},LRG_DIAL,{
        paper:gps_paper,
        label_text:"DIR",
        x:120, y:3,
		range: 360,
		angle0:0,
		arange:360,
		tick_size: 90
	})).draw();

	VMS.PSI.dial = Dial($.extend({},MED_DIAL,{
        x:20,
        paper: psi_paper,
        //label_text:"PSI",
		range: 2500,
		danger: 2000,
		tick_size: 2500
	})).draw();

    VMS.DISK.dial = Dial($.extend({},SML_DIAL,{
        x:20,
        paper:disk_paper,
        label_text:"used",
	    range: 100,
	    danger: 90,
	    tick_size: 25
    })).draw();

});
