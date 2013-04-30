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



var testLat = 495;
var testMax = 505;
var testMin = 495;
var counter = 0;
    
if (config.TEST) {
        if(testLat == testMax)
        {
                counter = 1;
        }
        if(testLat == testMin)
        {
                counter = 0;
        }
        if(counter== 0)
        {
                out.lat = testLat++;
                out.lng = 0;
        }
        else
        {
                out.lat = testLat --;
                out.lng = 0;
        }
}
