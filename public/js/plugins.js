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

// remap jQuery to $
(function ($) {























})(window.jQuery);







// usage: log('inside coolFunc',this,arguments);
// paulirish.com/2009/log-a-lightweight-wrapper-for-consolelog/
window.log = function () {

    log.history = log.history || []; // store logs to an array for reference
    log.history.push(arguments);

    if (this.console) {

        console.log(Array.prototype.slice.call(arguments));

    }

};







// catch all document.write() calls
(function (doc) {

    var write = doc.write;

    doc.write = function (q) {

        log('document.write(): ', arguments);

        if (/docwriteregexwhitelist/.test(q)) write.apply(doc, arguments);

    };

})(document);
