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

(function(undef){

var gold = "0-#fff-#fb0:20-#000";
function zpad(s,d) {
	s+='';
	d -= s.length;
	while (d-- > 0) s = "0" + s;
	return s;
}

/**
 * RaphaelJS vector graphic dial.
 */
window.Dial = function(config) {
	// required arguments.
	if (config.x !== undef && config.y !== undef && config.r && config.range) {

		var defaults = {
			rim_attrs:{
                stroke: "black",
				'stroke-width': 2,
				fill: "r(0.5, 0.8)#ffffff-#c0c0c0"
			},
			tick_size: config.range/8,
			angle0: 0,
			arange: 360
		}
		
		var o =	$.extend({},defaults,config);
		o.__proto__ = Dial;

		o.vlen = (''+o.range).length;
		o.markfontsize = 14;
		return o;

	} else {
        console.log(config);
		throw "Missing required argument to Dial class.";
	}
}

/**
 * @chainable
 */
Dial.draw = function() {

	var that=this;
	function pt(x,y) {
		return (that.center.x + that.r*x) + " " + (that.center.y + that.r*y);
	}

	this.center = {x:this.x+this.r, y:this.y+this.r};
	this.value = 0;
	this.rim = this.paper.circle(this.center.x, this.center.y, this.r);

	this.rim.attr(this.rim_attrs);
	
	// draw value ticks.
	var tick = 0;
	while (tick <= this.range) {
	
	    (function (t) {
		var a = this.angle0 + this.arange*tick/this.range
		that.paper.path(
				"M" + pt(0,-1)
			  	+"L" + pt(0,-(this.r-5)/this.r)
			)
			.attr({stroke: t>this.danger ? '#f00' : this.rim_attrs.stroke, rotation: a + " " + pt(0,0)})

		if (a<this.angle0+360) that.paper.text(this.center.x, this.center.y - this.r + this.markfontsize + 5, tick, this.vlen)
			.attr({
				'font-size': this.markfontsize,
				rotation: Math.floor(a /*((''+tick).length) * this.markfontsize / 3*/) + " " + pt(0,0)
			})

	    }).apply(this,[tick]);
	    tick += this.tick_size;
	}

	this.text = this.paper.text(this.center.x, this.center.y - this.r*0.2, 0);
    if (this.label_text) {
        this.label = this.paper.text(this.center.x, this.center.y + this.r*0.3, this.label_text);

        this.label.attr({
            'font-weight':'bold',
            'font-size':Math.floor((this.r+100)/10)
        });
    }
	this.text.attr({
        'font-weight':'bold',
		'font-size':Math.floor((this.r+100)/9)
	});

	/**
	 * Draw the needle.
	 */
	this.needle = this.paper.path(
		  "M" + pt(.05 , .2)
		+ "L" + pt(0   , .07)
		+ "L" + pt(-.05, .2)
		+ "L" + pt(0   , .9)
		+ "L" + pt(.05 , .2)
                + "Z"
	);
	this.needle.attr({
		fill: gold,
        opacity: 0.1
	});
	this.needle_pt = this.paper.circle(this.center.x, this.center.y, this.r*0.07).attr({
		fill: gold,
	});

	this.update(0);
	return this;
}

/**
 * @chainable
 */
Dial.update = function(vl) {
	vl = Math.min(vl, this.range)
	var a = this.angle0 + this.arange*vl/this.range;
	this.needle.animate({
		rotation: (180+a) + " " + this.center.x + " " + this.center.y
	},1);
	this.text.node.textContent = zpad(vl, this.vlen);
	return this;
}

})();
