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
_ = require('underscore')._;

var Class = function (constructor, proto) {
    f = function (spec) {
        spec.__proto__ = proto;
        constructor.apply(spec);
        return spec;
    };
    _.extend(f, Class._static);
    return f;
};

Class._static = {
    List: function () {
        return List({
            items: _.map(arguments, this)
        });
    }
};

List = Class(function () {}, {
    each: function (fn) {
        for(var i = 0; i < this.items.length; i++) {
            if(fn(this.items[i]) === false) return;
        }
    },
    getByName: function (name) {
        var match;
        this.each(function (item) {
            if(name == item.name) {
                match = item;
                return false;
            }
            return true;
        });
        return match;
    }
});

module.exports = {
    Class: Class,
    List: List
};