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


function set_cookie(name,val) {
	document.cookie=name + "=" + escape(val);
}

function get_cookie(name,val) {
	var cookies = document.cookie.replace(" ","").split(";");
	for (var i = 0; i < cookies.length; i++) {
		if (name == cookies[i].split("=")[0]) {
			return unescape(cookies[i].split("=")[1]);
		}
	}
}

function tryParseInt(val) {
	try {
		return parseInt(val);
	} catch (e) {
		return 0;
	}
}

function gpsKit() {};

gpsKit.NORTH = 'N';
gpsKit.SOUTH = 'S';
gpsKit.EAST = 'E';
gpsKit.WEST = 'W';

gpsKit.roundToDecimal = function( inputNum, numPoints ) {
	var multiplier = Math.pow( 10, numPoints );
	return Math.round( inputNum * multiplier ) / multiplier;
};

gpsKit.decimalToDMS = function( location, hemisphere ){
	if( location < 0 ) location *= -1; // strip dash '-'

	var degrees = Math.floor( location );          // strip decimal remainer for degrees
	var minutesFromRemainder = ( location - degrees ) * 60;       // multiply the remainer by 60
	var minutes = Math.floor( minutesFromRemainder );       // get minutes from integer
	var secondsFromRemainder = ( minutesFromRemainder - minutes ) * 60;   // multiply the remainer by 60
	var seconds = gpsKit.roundToDecimal( secondsFromRemainder, 2 ); // get minutes by rounding to integer

	return degrees + '° ' + minutes + "' " + seconds + '" ' + hemisphere;
};

gpsKit.decimalLatToDMS = function( location ){
	var hemisphere = ( location < 0 ) ? gpsKit.SOUTH : gpsKit.NORTH; // south if negative
	return gpsKit.decimalToDMS( location, hemisphere );
};

gpsKit.decimalLongToDMS = function( location ){
	var hemisphere = ( location < 0 ) ? gpsKit.WEST : gpsKit.EAST;  // west if negative
	return gpsKit.decimalToDMS( location, hemisphere );
};