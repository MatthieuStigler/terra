// Copyright (c) 2018-2021  Robert J. Hijmans
//
// This file is part of the "spat" library.
//
// spat is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// spat is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with spat. If not, see <http://www.gnu.org/licenses/>.

#include "spatRaster.h"
#include "distance.h"
#include <limits>
#include <cmath>
#include "geodesic.h"
#include "recycle.h"
#include "math_utils.h"
#include "vecmath.h"
#include "file_utils.h"


void shortDistPoints(std::vector<double> &d, const std::vector<double> &x, const std::vector<double> &y, const std::vector<double> &px, const std::vector<double> &py, const bool& lonlat, const double &lindist) {
	if (lonlat) {
		distanceToNearest_lonlat(d, x, y, px, py);
	} else {
		distanceToNearest_plane(d, x, y, px, py, lindist);
	}
}

void shortDirectPoints(std::vector<double> &d, const std::vector<double> &x, const std::vector<double> &y, const std::vector<double> &px, const std::vector<double> &py, const bool& lonlat, bool &from, bool &degrees) {
	if (lonlat) {
		directionToNearest_lonlat(d, x, y, px, py, degrees, from);
	} else {
		directionToNearest_plane(d, x, y, px, py, degrees, from);
	}
}



SpatRaster SpatRaster::disdir_vector_rasterize(SpatVector p, bool align_points, bool distance, bool from, bool degrees, SpatOptions &opt) {

	SpatRaster out = geometry();
	if (source[0].srs.wkt == "") {
		out.setError("CRS not defined");
		return(out);
	}
	if (!source[0].srs.is_same(p.srs, false)) {
		out.setError("CRS does not match");
		return(out);
	}
	


	double m = source[0].srs.to_meter();
	m = std::isnan(m) ? 1 : m;

	SpatRaster x;
	std::string gtype = p.type();
	std::vector<std::vector<double>> pxy;
	if (gtype == "points") {
		pxy = p.coordinates();
		if (pxy.size() == 0) {
			out.setError("no locations to compute from");
			return(out);
		}

		if (align_points) {
			std::vector<double> cells = cellFromXY(pxy[0], pxy[1]);
			cells.erase(std::unique(cells.begin(), cells.end()), cells.end());
			pxy = xyFromCell(cells);
		}
	} else {
		SpatOptions ops(opt);
		std::vector<double> feats(p.size(), 1) ;
		x = out.rasterize(p, "", feats, NAN, false, false, false, false, false, ops);
		if (gtype == "polygons") {
			std::string etype = "inner";
			x = x.edges(false, etype, 8, 0, ops);
		}
		p = x.as_points(false, true, false, opt);
		pxy = p.coordinates();
	}

	if (pxy.size() == 0) {
		out.setError("no locations to compute from");
		return(out);
	}

	bool lonlat = is_lonlat(); // m == 0
	//double torad = 0.0174532925199433;
	//if (!lonlat) {
	//	for (size_t i=0; i<pxy[0].size(); i++) {
	//		pxy[0][i] *= torad;
	//		pxy[1][i] *= torad;
	//	}
	//}

	unsigned nc = ncol();
	if (!readStart()) {
		out.setError(getError());
		return(out);
	}

 	if (!out.writeStart(opt)) {
		readStop();
		return out;
	}

	for (size_t i = 0; i < out.bs.n; i++) {
		std::vector<double> v, cells;
		cells.resize(out.bs.nrows[i] * nc) ;
		std::iota(cells.begin(), cells.end(), out.bs.row[i] * nc);

		if (gtype == "points") {
			readBlock(v, out.bs, i);
			for (size_t j=0; j<v.size(); j++) {
				if (!std::isnan(v[j])) {
					cells[j] = NAN;
				}
			}			
		} else {
			x.readBlock(v, out.bs, i);
			for (size_t j=0; j<v.size(); j++) {
				if (!std::isnan(v[j])) {
					cells[j] = NAN;
				}
			}
		} 
		std::vector<std::vector<double>> xy = xyFromCell(cells);
		if (distance) {
			for (double& d : cells) d = 0;
			shortDistPoints(cells, xy[0], xy[1], pxy[0], pxy[1], lonlat, m);
		} else {
			for (double& d : cells) d = NAN;
			shortDirectPoints(cells, xy[0], xy[1], pxy[0], pxy[1], lonlat, from, degrees);
		}
		if (!out.writeBlock(cells, i)) return out;
	}

	out.writeStop();
	readStop();
	return(out);
}



SpatRaster SpatRaster::distance_vector(SpatVector p, SpatOptions &opt) {

	SpatRaster out = geometry();
	if (source[0].srs.wkt == "") {
		out.setError("CRS not defined");
		return(out);
	}
	if (!source[0].srs.is_same(p.srs, false) ) {
		out.setError("CRS does not match");
		return(out);
	}

	double m = source[0].srs.to_meter();
	m = std::isnan(m) ? 1 : m;


	if (p.size() == 0) {
		out.setError("no locations to compute distance from");
		return(out);
	}
	p = p.aggregate(false);

//	bool lonlat = is_lonlat(); // m == 0
	unsigned nc = ncol();

 	if (!out.writeStart(opt)) {
		readStop();
		return out;
	}
	std::vector<double> cells;

	for (size_t i = 0; i < out.bs.n; i++) {
		double s = out.bs.row[i] * nc;
		cells.resize(out.bs.nrows[i] * nc) ;
		std::iota(cells.begin(), cells.end(), s);
		std::vector<std::vector<double>> xy = xyFromCell(cells);
		SpatVector pv(xy[0], xy[1], points, "");
		pv.srs = source[0].srs;
		std::vector<double> d = p.distance(pv, false);
		if (p.hasError()) {
			out.setError(p.getError());
			out.writeStop();
			return(out);
		}
		if (!out.writeBlock(d, i)) return out;
	}
	out.writeStop();
	return(out);
}



SpatRaster SpatRaster::distance(SpatOptions &opt) {
	SpatRaster out = geometry(1);
	if (!hasValues()) {
		out.setError("SpatRaster has no values");
		return out;
	}

	SpatOptions ops(opt);
	size_t nl = nlyr();
	if (nl > 1) {
		std::vector<std::string> nms = getNames();
		if (ops.names.size() == nms.size()) {
			nms = opt.names;
		}		
		out.source.resize(nl);
		for (unsigned i=0; i<nl; i++) {
			std::vector<unsigned> lyr = {i};
			SpatRaster r = subset(lyr, ops);
			ops.names = {nms[i]};
			r = r.distance(ops);
			out.source[i] = r.source[0];
		}
		if (opt.get_filename() != "") {
			out = out.writeRaster(opt);
		}
		return out;
	}

	out = edges(false, "inner", 8, NAN, ops);
	SpatVector p = out.as_points(false, true, false, ops);
	if (p.size() == 0) {
		return out.init({0}, opt);
	}
	out = disdir_vector_rasterize(p, false, true, false, false, opt);
	return out;
}

SpatRaster SpatRaster::direction(bool from, bool degrees, SpatOptions &opt) {
	SpatRaster out = geometry(1);
	if (!hasValues()) {
		out.setError("SpatRaster has no values");
		return out;
	}

	SpatOptions ops(opt);
	size_t nl = nlyr();
	if (nl > 1) {
		out.source.resize(nl);
		std::vector<std::string> nms = getNames();
		if (ops.names.size() == nms.size()) {
			nms = opt.names;
		}		
		for (unsigned i=0; i<nl; i++) {
			std::vector<unsigned> lyr = {i};
			SpatRaster r = subset(lyr, ops);
			ops.names = {nms[i]};
			r = r.direction(from, degrees, ops);
			out.source[i] = r.source[0];
		}
		if (opt.get_filename() != "") {
			out = out.writeRaster(opt);
		}
		return out;
	}

	out = edges(false, "inner", 8, NAN, ops);
	SpatVector p = out.as_points(false, true, false, opt);
	if (p.size() == 0) {
		out.setError("no cells to compute direction from or to");
		return(out);
	}
	out = disdir_vector_rasterize(p, false, false, from, degrees, opt);
	return out;
}





std::vector<double> SpatVector::distance(bool sequential) {
	std::vector<double> d;
	if (srs.is_empty()) {
		setError("crs not defined");
		return(d);
	}
	double m = srs.to_meter();
	m = std::isnan(m) ? 1 : m;
	bool lonlat = is_lonlat(); // m == 0

//	if ((!lonlat) || (gtype != "points")) {
	std::string gtype = type();
	if (gtype != "points") {
		d = geos_distance(sequential);
		if ((!lonlat) && (m != 1)) {
			for (double &i : d) i *= m;
		}
		return d;
	} else {
		if (sequential) {
			std::vector<std::vector<double>> p = coordinates();
			size_t n = p[0].size();
			d.reserve(n);
			d.push_back(0);
			n -= 1;
			if (lonlat) {
				for (size_t i=0; i<n; i++) {
					d.push_back(
						distance_lonlat(p[0][i], p[1][i], p[0][i+1], p[1][i+1])
					);
				}
			} else {
				for (size_t i=0; i<n; i++) {
					d.push_back(
						distance_plane(p[0][i], p[1][i], p[0][i+1], p[1][i+1]) * m
					);
				}
			}

		} else {
			size_t s = size();
			size_t n = ((s-1) * s)/2;
			d.reserve(n);
			std::vector<std::vector<double>> p = coordinates();
			if (lonlat) {
				for (size_t i=0; i<(s-1); i++) {
					for (size_t j=(i+1); j<s; j++) {
						d.push_back(
							distance_lonlat(p[0][i], p[1][i], p[0][j], p[1][j])
						);
					}
				}
			} else {
				for (size_t i=0; i<(s-1); i++) {
					for (size_t j=(i+1); j<s; j++) {
						d.push_back(
							distance_plane(p[0][i], p[1][i], p[0][j], p[1][j]) * m
						);
					}
				}
			}
		}
	}

	return d;
}


std::vector<double>  SpatVector::distance(SpatVector x, bool pairwise) {

	std::vector<double> d;

	if (srs.is_empty() || x.srs.is_empty()) {
		setError("SRS not defined");
		return(d);
	}
	if (! srs.is_same(x.srs, false) ) {
		setError("SRS do not match");
		return(d);
	}

	size_t s = size();
	size_t sx = x.size();
	if ((s == 0) || (sx == 0)) {
		setError("empty SpatVector");
		return(d);
	}
	
	if (pairwise && (s != sx ) && (s > 1) && (sx > 1))  {
		setError("Can only do pairwise distance if geometries match, or if one is a single geometry");
		return(d);
	}

	double m = srs.to_meter();
	m = std::isnan(m) ? 1 : m;
	bool lonlat = is_lonlat();

	std::string gtype = type();
	std::string xtype = x.type();

/*
	int ispts = (gtype == "points") + (xtype == "points");
	if ((lonlat) && (ispts == 1)) {
		if (xtype == "points") {
			return linedistLonLat(x);
		} else {
			return x.linedistLonLat(*this);			
		}
	}
*/		
	
	
	if ((!lonlat) || (gtype != "points") || (xtype != "points")) {
		d = geos_distance(x, pairwise);
		if ((!lonlat) && (m != 1)) {
			for (double &i : d) i *= m;
		}
		return d;
	}
	std::vector<std::vector<double>> p = coordinates();
	std::vector<std::vector<double>> px = x.coordinates();

/*  recycling, not a good idea.
	if (pairwise) {
		if (s < sx) {
			recycle(p[0], sx);
			recycle(p[1], sx);
			s = sx;
		} else if (s > sx) {
			recycle(px[0], s);
			recycle(px[1], s);
			sx = s;
		}
	}
*/


	size_t n = pairwise ? std::max(s,sx) : s*sx;
	d.resize(n);

	if (pairwise) {
		if (s == sx) {
			if (lonlat) {
				for (size_t i = 0; i < s; i++) {
					d[i] = distance_lonlat(p[0][i], p[1][i], px[0][i], px[1][i]);
				}
			} else { // not reached
				for (size_t i = 0; i < s; i++) {
					d[i] = distance_plane(p[0][i], p[1][i], px[0][i], px[1][i]) * m;
				}
			} 
		} else if (s == 1) {  // to avoid recycling. 
			if (lonlat) {
				for (size_t i = 0; i < sx; i++) {
					d[i] = distance_lonlat(p[0][0], p[1][0], px[0][i], px[1][i]);
				}
			} else { // not reached
				for (size_t i = 0; i < sx; i++) {
					d[i] = distance_plane(p[0][0], p[1][0], px[0][i], px[1][i]) * m;
				}
			}
		} else { // if (sx == 1) {
			if (lonlat) {
				for (size_t i = 0; i < s; i++) {
					d[i] = distance_lonlat(p[0][i], p[1][i], px[0][0], px[1][0]);
				}
			} else { // not reached
				for (size_t i = 0; i < s; i++) {
					d[i] = distance_plane(p[0][i], p[1][i], px[0][0], px[1][0]) * m;
				}
			} 
		}
	} else {
		if (lonlat) {
			for (size_t i=0; i<s; i++) {
				size_t k = i * sx;
				for (size_t j=0; j<sx; j++) {
					d[k+j] = distance_lonlat(p[0][i], p[1][i], px[0][j], px[1][j]);
				}
			}
		} else { // not reached
			for (size_t i=0; i<s; i++) {
				size_t k = i * sx;
				for (size_t j=0; j<sx; j++) {
					d[k+j] = distance_plane(p[0][i], p[1][i], px[0][j], px[1][j]) * m;
				}
			}
		} 
	}

	return d;
}

inline double minCostDist(std::vector<double> &d) { 
	d.erase(std::remove_if(d.begin(), d.end(),
		[](const double& v) { return std::isnan(v); }), d.end());
	std::sort(d.begin(), d.end());
	return d.size() > 0 ? d[0] : NAN;
}
	

inline void DxDxyCost(const double &lat, const int &row, double xres, double yres, const int &dir, double &dx,  double &dy, double &dxy, double distscale, const double mult=2) {
	double rlat = lat + row * yres * dir;
	dx  = distance_lonlat(0, rlat, xres, rlat) / (mult * distscale);
	yres *= -dir;
	dy  = distance_lonlat(0, 0, 0, yres);
	dxy = distance_lonlat(0, rlat, xres, rlat+yres);
	dy = std::isnan(dy) ? NAN : dy / (mult * distscale);
	dxy = std::isnan(dxy) ? NAN : dxy / (mult * distscale);
}

/*
void printit(std::vector<double>&x, size_t nc, std::string s) {
	size_t cnt = 0;
	Rcpp::Rcout << s << std::endl;
	for (size_t i=0; i<x.size(); i++) {
		Rcpp::Rcout << x[i] << " ";
		cnt++;
		if (cnt == nc) {
			Rcpp::Rcout << std::endl;
			cnt = 0;
		}
	}
}
*/

void cost_dist(std::vector<double> &dist, std::vector<double> &dabove, std::vector<double> &v, std::vector<double> &vabove, std::vector<double> res, size_t nr, size_t nc, double lindist, bool geo, double lat, double latdir, bool global, bool npole, bool spole) {

	std::vector<double> cd;

	double dx, dy, dxy;	
	if (geo) {
		DxDxyCost(lat, 0, res[0], res[1], latdir, dx, dy, dxy, lindist);
	} else {
		dx = res[0] * lindist / 2;
		dy = res[1] * lindist / 2;
		dxy = sqrt(dx*dx + dy*dy);
	}
	
	//top to bottom
    //left to right
	//first cell, no cell left of it
	if (!std::isnan(v[0])) { 
		if (global) {
			cd = {dist[0], dabove[0] + (v[0]+vabove[0]) * dy, 
				dist[nc-1] + (v[0] + v[nc-1]) * dx, 
				dabove[nc-1] + dxy * (vabove[nc-1]+v[0])}; 
		} else {
			cd = {dist[0], dabove[0] + (v[0]+vabove[0]) * dy}; 
		}
		dist[0] = minCostDist(cd);
	}
	for (size_t i=1; i<nc; i++) { //first row, no row above it, use "above"
		if (!std::isnan(v[i])) {
			cd = {dist[i], dabove[i]+(vabove[i]+v[i])*dy, dabove[i-1]+(vabove[i-1]+v[i])*dxy, dist[i-1]+(v[i-1]+v[i])*dx};
			dist[i] = minCostDist(cd);
		}
	}
	if (npole) {
		double minp = *std::min_element(dist.begin(), dist.begin()+nc);
		minp += dy;
		for (size_t i=0; i<nc; i++) { 
			dist[i] = std::min(dist[i], minp);
		}
	}


	for (size_t r=1; r<nr; r++) { //other rows
		if (geo) DxDxyCost(lat, r, res[0], res[1], latdir, dx, dy, dxy, lindist);
		size_t start=r*nc;
		if (!std::isnan(v[start])) {
			if (global) {
				cd = {dist[start-nc] + (v[start] + v[start-nc]) * dy, dist[start], 
					dist[start+nc-1] + (v[start] + v[start+nc-1]) * dx,
					dist[start-1] + (v[start] + v[start-1]) * dxy};
			} else {
				cd = {dist[start-nc] + (v[start] + v[start-nc]) * dy, dist[start]};				
			}
			dist[start] = minCostDist(cd);
		}
		size_t end = start+nc;
		for (size_t i=(start+1); i<end; i++) {
			if (!std::isnan(v[i])) {
				cd = {dist[i], dist[i-1]+(v[i]+v[i-1])*dx, dist[i-nc]+(v[i]+v[i-nc])*dy, dist[i-nc-1]+(v[i]+v[i-nc-1])*dxy};
				dist[i] = minCostDist(cd);
			}
		}
	}
	if (spole) {
		double minp = *std::min_element(dist.end()-nc, dist.end());
		minp += dy;
		size_t ds = dist.size();
		for (size_t i=ds-nc; i<ds; i++) { 
			dist[i] = std::min(dist[i], minp);
		}
	}

	//right to left
	// first row, no need for first (last) cell (unless is global)
	if (geo) DxDxyCost(lat, 0, res[0], res[1], latdir, dx, dy, dxy, lindist);
	if (global) {
		size_t i=(nc-1);
		cd = {dist[i],  
			dist[0] + (v[0] + v[i]) * dx, 
			dabove[0] + dxy * (vabove[0]+v[i])}; 
		dist[i] = minCostDist(cd);
	}
	if (npole) {
		double minp = *std::min_element(dist.begin(), dist.begin()+nc);
		minp += dy;
		for (size_t i=0; i<nc; i++) { 
			dist[i] = std::min(dist[i], minp);
		}
	}
	
	for (int i=(nc-2); i > -1; i--) { // other cells on first row
		if (!std::isnan(v[i])) {
			cd = {dabove[i]+(vabove[i]+v[i])*dy, dabove[i+1]+(vabove[i+1]+v[i])*dxy, dist[i+1]+(v[i+1]+v[i])*dx, dist[i]};
			dist[i] = minCostDist(cd);
		}
	}

	for (size_t r=1; r<nr; r++) { // other rows
		if (geo) DxDxyCost(lat, r, res[0], res[1], latdir, dx, dy, dxy, lindist);
		size_t start=(r+1)*nc-1;
	
		if (!std::isnan(v[start])) {
			if (global) {
				cd = { dist[start], dist[start-nc] + (v[start-nc]+v[start])* dy, 
					dist[start-nc+1] + (v[start-nc+1] + v[start]) * dx,
					dist[start-(2*nc)+1] + (v[start-(2*nc)+1] + v[start]) * dxy						
				};
				
			} else {
				cd = { dist[start], dist[start-nc] + (v[start-nc]+v[start])* dy };
			}
			dist[start] = minCostDist(cd);
		}
		
		size_t end=r*nc;
		start -= 1;
		for (size_t i=start; i>=end; i--) {
			if (!std::isnan(v[i])) {
				cd = { dist[i+1]+(v[i+1]+v[i])*dx, dist[i-nc+1]+(v[i]+v[i-nc+1])*dxy, dist[i-nc]+(v[i]+v[i-nc])*dy, dist[i]};
				dist[i] = minCostDist(cd);
			}
		}
	}
	if (spole) {
		double minp = *std::min_element(dist.end()-nc, dist.end());
		minp += dy;
		size_t ds = dist.size();
		for (size_t i=ds-nc; i<ds; i++) { 
			dist[i] = std::min(dist[i], minp);
		}
	}
	
	size_t off = (nr-1) * nc;
	dabove = std::vector<double>(dist.begin()+off, dist.end());
	vabove = std::vector<double>(v.begin()+off, v.end());
	
}


void grid_dist(std::vector<double> &dist, std::vector<double> &dabove, std::vector<double> &v, std::vector<double> &vabove, std::vector<double> res, size_t nr, size_t nc, double lindist, bool geo, double lat, double latdir, bool global, bool npole, bool spole) {

	std::vector<double> cd;

	double dx, dy, dxy;	
	if (geo) {
		DxDxyCost(lat, 0, res[0], res[1], latdir, dx, dy, dxy, lindist, 1);
	} else {
		dx = res[0] * lindist;
		dy = res[1] * lindist;
		dxy = sqrt(dx*dx + dy*dy);
	}
	
	//top to bottom
    //left to right
	//first cell, no cell left of it
	if (!std::isnan(v[0])) { 
		if (global) {
			cd = {dist[0], dabove[0] + dy, 
				dist[nc-1] + dx, 
				dabove[nc-1] + dxy}; 
		} else {
			cd = {dist[0], dabove[0] + dy}; 
		}
		dist[0] = minCostDist(cd);
	}
	for (size_t i=1; i<nc; i++) { //first row, no row above it, use "above"
		if (!std::isnan(v[i])) {
			cd = {dist[i], dabove[i]+dy, dabove[i-1]+dxy, dist[i-1]+dx};
			dist[i] = minCostDist(cd);
		}
	}
	if (npole) {
		double minp = *std::min_element(dist.begin(), dist.begin()+nc);
		minp += dy;
		for (size_t i=0; i<nc; i++) { 
			dist[i] = std::min(dist[i], minp);
		}
	}


	for (size_t r=1; r<nr; r++) { //other rows
		if (geo) DxDxyCost(lat, r, res[0], res[1], latdir, dx, dy, dxy, lindist);
		size_t start=r*nc;
		if (!std::isnan(v[start])) {
			if (global) {
				cd = {dist[start-nc] + dy, dist[start], 
					dist[start+nc-1] + dx,
					dist[start-1] + dxy};
			} else {
				cd = {dist[start-nc] + dy, dist[start]};				
			}
			dist[start] = minCostDist(cd);
		}
		size_t end = start+nc;
		for (size_t i=(start+1); i<end; i++) {
			if (!std::isnan(v[i])) {
				cd = {dist[i], dist[i-1]+dx, dist[i-nc]+dy, dist[i-nc-1]+dxy};
				dist[i] = minCostDist(cd);
			}
		}
	}
	if (spole) {
		double minp = *std::min_element(dist.end()-nc, dist.end());
		minp += dy;
		size_t ds = dist.size();
		for (size_t i=ds-nc; i<ds; i++) { 
			dist[i] = std::min(dist[i], minp);
		}
	}

	//right to left
	// first row, no need for first (last) cell (unless is global)
	if (geo) DxDxyCost(lat, 0, res[0], res[1], latdir, dx, dy, dxy, lindist);
	if (global) {
		size_t i=(nc-1);
		cd = {dist[i],  
			dist[0] + dx, 
			dabove[0] + dxy}; 
		dist[i] = minCostDist(cd);
	}
	if (npole) {
		double minp = *std::min_element(dist.begin(), dist.begin()+nc);
		minp += dy;
		for (size_t i=0; i<nc; i++) { 
			dist[i] = std::min(dist[i], minp);
		}
	}
	
	for (int i=(nc-2); i > -1; i--) { // other cells on first row
		if (!std::isnan(v[i])) {
			cd = {dabove[i]+dy, dabove[i+1]+dxy, dist[i+1]+dx, dist[i]};
			dist[i] = minCostDist(cd);
		}
	}

	for (size_t r=1; r<nr; r++) { // other rows
		if (geo) DxDxyCost(lat, r, res[0], res[1], latdir, dx, dy, dxy, lindist);
		size_t start=(r+1)*nc-1;
	
		if (!std::isnan(v[start])) {
			if (global) {
				cd = { dist[start], dist[start-nc] + dy, 
					dist[start-nc+1] + dx,
					dist[start-(2*nc)+1] + dxy						
				};
				
			} else {
				cd = { dist[start], dist[start-nc] + dy };
			}
			dist[start] = minCostDist(cd);
		}
		
		size_t end=r*nc;
		start -= 1;
		for (size_t i=start; i>=end; i--) {
			if (!std::isnan(v[i])) {
				cd = { dist[i+1]+dx, dist[i-nc+1]+dxy, dist[i-nc]+dy, dist[i]};
				dist[i] = minCostDist(cd);
			}
		}
	}
	if (spole) {
		double minp = *std::min_element(dist.end()-nc, dist.end());
		minp += dy;
		size_t ds = dist.size();
		for (size_t i=ds-nc; i<ds; i++) { 
			dist[i] = std::min(dist[i], minp);
		}
	}
	
	size_t off = (nr-1) * nc;
	dabove = std::vector<double>(dist.begin()+off, dist.end());
	vabove = std::vector<double>(v.begin()+off, v.end());
}

void block_is_same(bool& same, std::vector<double>& x,  std::vector<double>& y) {
	if (!same) return;
	for (size_t i=0; i<x.size(); i++) {
		if (!std::isnan(x[i]) && (x[i] != y[i])) {
			same = false;
			break;
		}
	}
}






SpatRaster SpatRaster::costDistanceRun(SpatRaster &old, bool &converged, double target, double m, bool lonlat, bool global, bool npole, bool spole, bool grid, SpatOptions &opt) {

	std::vector<double> res = resolution();
		
	SpatRaster first = geometry();
	SpatRaster second = first;
    std::vector<double> d, v, vv;
	if (!readStart()) {
		first.setError(getError());
		return(first);
	}
	opt.progressbar = false;
 	if (!first.writeStart(opt)) { return first; }

	size_t nc = ncol();
	std::vector<double> dabove(nc, NAN);
	std::vector<double> vabove(nc, 0);
	double lat = 0;
	if (old.hasValues()) {
		if (!old.readStart()) {
			first.setError(getError());
			return(first);
		}
		if (!first.writeStart(opt)) {
			readStop();
			old.readStop();
			return first;
		}

		for (size_t i = 0; i < first.bs.n; i++) {
			readBlock(v, first.bs, i);
			if (lonlat) {
				lat = yFromRow(first.bs.row[i]);
			} 
			bool np = (i==0) && npole;		
			bool sp = (i==first.bs.n-1) && spole;
			if (target != 0) {
				for (size_t j=0; j<v.size(); j++) {
					if (v[j] == target) {
						v[j] = 0;
					} 
				}					
			}
			old.readBlock(d, first.bs, i);
			if (grid) {
				grid_dist(d, dabove, v, vabove, res, first.bs.nrows[i], nc, m, lonlat, lat, -1, global, np, sp);
			} else {
				cost_dist(d, dabove, v, vabove, res, first.bs.nrows[i], nc, m, lonlat, lat, -1, global, np, sp);
			}
			if (!first.writeValues(d, first.bs.row[i], first.bs.nrows[i])) return first;
		}
	} else {
		converged = false;
		for (size_t i = 0; i < first.bs.n; i++) {
			if (lonlat) {
				lat = yFromRow(first.bs.row[i]);
			} 
			bool np = (i==0) && npole;		
			bool sp = (i==first.bs.n-1) && spole;			
			readBlock(v, first.bs, i);
			d.clear();
			d.resize(v.size(), NAN);
			for (size_t j = 0; j < v.size(); j++) {
				if (v[j] == target) {
					v[j] = 0;
					d[j] = 0;
				} else if ((!grid) && (v[j] < 0)) {
					readStop();
					first.writeStop();
					first.setError("negative friction values not allowed");
					return first;
				}
			}
			if (grid) {
				grid_dist(d, dabove, v, vabove, res, first.bs.nrows[i], nc, m, lonlat, lat, -1, global, np, sp);
			} else {
				cost_dist(d, dabove, v, vabove, res, first.bs.nrows[i], nc, m, lonlat, lat, -1, global, np, sp);
			}
			if (!first.writeValuesRect(d, first.bs.row[i], first.bs.nrows[i], 0, nc)) return first;
		}
	}
	first.writeStop();
	
	if (!first.readStart()) {
		return(first);
	}

	dabove = std::vector<double>(nc, NAN);
	vabove = std::vector<double>(nc, 0);
  	if (!second.writeStart(opt)) {
		readStop();
		first.readStop();
		return second;
	}
	for (int i = second.bs.n; i>0; i--) {
		if (lonlat) {
			lat = yFromRow(second.bs.row[i-1] + second.bs.nrows[i-1] - 1);
		} 
		bool sp = (i==1) && spole; //! reverse order		
		bool np = (i==(int)second.bs.n) && npole;			
        readBlock(v, second.bs, i-1);
		if (target != 0) {
			for (size_t j=0; j<v.size(); j++) {
				if (v[j] == target) {
					v[j] = 0;
				} 
			}					
		}
		first.readBlock(d, second.bs, i-1);
		std::reverse(v.begin(), v.end());
		std::reverse(d.begin(), d.end());
		if (grid) {
			grid_dist(d, dabove, v, vabove, res, second.bs.nrows[i-1], nc, m, lonlat, lat, 1, global, np, sp);
		} else {
			cost_dist(d, dabove, v, vabove, res, second.bs.nrows[i-1], nc, m, lonlat, lat, 1, global, np, sp);			
		}
		std::reverse(d.begin(), d.end());
		if (converged) {
			old.readBlock(v, second.bs, i-1);
			block_is_same(converged, d, v);
		}
		if (!second.writeValuesRect(d, second.bs.row[i-1], second.bs.nrows[i-1], 0, nc)) return second;
	}
	second.writeStop();
	first.readStop();
	if (old.hasValues()) {
		old.readStop();
	}
	readStop();
	return(second);
}

SpatRaster SpatRaster::costDistance(double target, double m, size_t maxiter, bool grid, SpatOptions &opt) {

	SpatRaster out = geometry(1);
	if (!hasValues()) {
		out.setError("cannot compute distance for a raster with no values");
		return out;
	}

	std::string filename = opt.get_filename();
	if (filename != "") {
		if (file_exists(filename) && (!opt.get_overwrite())) {
			out.setError("output file exists. You can use 'overwrite=TRUE' to overwrite it");
			return(out);
		}
	}

	SpatOptions ops(opt);
	if (nlyr() > 1) {
		std::vector<unsigned> lyr = {0};
		out = subset(lyr, ops);
		out = out.costDistance(target, m, maxiter, grid, opt);
		out.addWarning("cost distance computations are only done for the first input layer");
		return out;
	}

	bool lonlat = is_lonlat(); 
	bool global = is_global_lonlat();
	int polar = ns_polar();
	bool npole = (polar == 1) || (polar == 2);
	bool spole = (polar == -1) || (polar == 2);

	if (!lonlat) {
		m = source[0].srs.to_meter();
		m = std::isnan(m) ? 1 : m;
	} 
	
	std::vector<double> res = resolution();

	size_t i = 0;
	bool converged=false;	
	while (i < maxiter) {
		out = costDistanceRun(out, converged, target, m, lonlat, global, npole, spole, grid, ops);		
		if (out.hasError()) return out;
		if (converged) break;
		converged = true;
		i++;
	}
	if (filename != "") {
		out = out.writeRaster(opt);
	}
	if (i == maxiter) {
		out.addWarning("costDistance did not converge");
	}
	return(out);
}






std::vector<double> broom_dist_planar(std::vector<double> &v, std::vector<double> &above, std::vector<double> res, size_t nr, size_t nc, double lindist) {

	double dx = res[0] * lindist;
	double dy = res[1] * lindist;
	double dxy = sqrt(dx * dx + dy *dy);

	std::vector<double> dist(v.size(), 0);

    //left to right
	//first cell, no cell left of it
	if ( std::isnan(v[0]) ) { 
		dist[0] = above[0] + dy;
	}
	//first row, no row above it, use "above"
	for (size_t i=1; i<nc; i++) { 
		if (std::isnan(v[i])) {
			dist[i] = std::min(std::min(above[i] + dy, above[i-1] + dxy), dist[i-1] + dx);
		}
	}

	//other rows
	for (size_t r=1; r<nr; r++) { 
		size_t start=r*nc;
		if (std::isnan(v[start])) {
			dist[start] = dist[start-nc] + dy;
		}

		size_t end = start+nc;
		for (size_t i=(start+1); i<end; i++) {
			if (std::isnan(v[i])) {
				dist[i] = std::min(std::min(dist[i-1] + dx, dist[i-nc] + dy), dist[i-nc-1] + dxy);
			}
		}
	}

	//right to left
	//first cell
	if ( std::isnan(v[nc-1])) { 
		dist[nc-1] = std::min(dist[nc-1], above[nc-1] + dy);
	}

	// other cells on first row
	for (int i=(nc-2); i > -1; i--) { 
		if (std::isnan(v[i])) {
			dist[i] = std::min(std::min(std::min(dist[i+1] + dx, above[i+1] + dxy), above[i] + dy), dist[i]);
		}
	}

	// other rows
	for (size_t r=1; r<nr; r++) { 
		size_t start=(r+1)*nc-1;
		if (std::isnan(v[start])) {
			dist[start] = std::min(dist[start], dist[start-nc] + dy);
		}
		for (size_t i=start-1; i>=(r*nc); i--) {
			if (std::isnan(v[i])) {
				dist[i] = std::min(std::min(std::min(dist[i], dist[i+1] + dx), dist[i-nc] + dy), dist[i-nc+1] + dxy);
			}
		}
	}
	size_t off = (nr-1) * nc;
	above = std::vector<double>(dist.begin()+off, dist.end());
	return dist;
}

/*
inline double minNArm(const double &a, const double &b) {
	if (std::isnan(a)) return b;
	if (std::isnan(b)) return a;
	return std::min(a, b);
}
*/

inline void DxDxy(const double &lat, const int &row, double xres, double yres, const int &dir, double &dx,  double &dy, double &dxy) {
	double rlat = lat + row * yres * dir;
	dx  = distance_lonlat(0, rlat     , xres, rlat);
	yres *= -dir;
	dy  = distance_lonlat(0, rlat, 0, rlat+yres);
	dxy = distance_lonlat(0, rlat, xres, rlat+yres);
	dy = std::isnan(dy) ? std::numeric_limits<double>::infinity() : dy;
	dxy = std::isnan(dxy) ? std::numeric_limits<double>::infinity() : dxy;
}


void broom_dist_geo(std::vector<double> &dist, std::vector<double> &v, std::vector<double> &above, std::vector<double> res, size_t nr, size_t nc, double lat, double latdir, bool npole, bool spole) {

	double dx, dy, dxy;
	//top to bottom
    //left to right
	DxDxy(lat, 0, res[0], res[1], latdir, dx, dy, dxy);
	//first cell, no cell left of it
	if ( std::isnan(v[0]) ) { 
		dist[0] = std::min(above[0] + dy, dist[0]);
	} 
	//first row, no row above it, use "above"
	for (size_t i=1; i<nc; i++) { 
		if (std::isnan(v[i])) {
			dist[i] = std::min(std::min(std::min(above[i] + dy, above[i-1] + dxy), dist[i-1] + dx), dist[i]);
		} 
	}
	if (npole) {
		double minp = *std::min_element(dist.begin(), dist.begin()+nc);
		minp += dy;
		for (size_t i=0; i<nc; i++) { 
			dist[i] = std::min(dist[i], minp);
		}
	}

	//other rows
	for (size_t r=1; r<nr; r++) { 
		DxDxy(lat, r, res[0], res[1], latdir, dx, dy, dxy);
		size_t start=r*nc;
		if (std::isnan(v[start])) {
			dist[start] = std::min(dist[start], dist[start-nc] + dy);
		} 
		size_t end = start+nc;
		for (size_t i=(start+1); i<end; i++) {
			if (std::isnan(v[i])) {
				dist[i] = std::min(dist[i], std::min(std::min(dist[i-1] + dx, dist[i-nc] + dy), dist[i-nc-1] + dxy));
			}
		}
	}
	if (spole) {
		double minp = *std::min_element(dist.end()-nc, dist.end());
		minp += dy;
		size_t ds = dist.size();
		for (size_t i=ds-nc; i<ds; i++) { 
			dist[i] = std::min(dist[i], minp);
		}
	}

	//right to left
	DxDxy(lat, 0, res[0], res[1], latdir, dx, dy, dxy);
	 //first cell
	if ( std::isnan(v[nc-1])) {
		dist[nc-1] = std::min(dist[nc-1], above[nc-1] + dy);
	}

	// other cells on first row
	for (int i=(nc-2); i > -1; i--) { 
		if (std::isnan(v[i])) {
			dist[i] = std::min(std::min(std::min(dist[i+1] + dx, above[i+1] + dxy), above[i] + dy), dist[i]);
		}
	}
	if (npole) {
		double minp = *std::min_element(dist.begin(), dist.begin()+nc);
		minp += dy;
		for (size_t i=0; i<nc; i++) { 
			dist[i] = std::min(dist[i], minp);
		}
	}
	// other rows
	for (size_t r=1; r<nr; r++) { 
		DxDxy(lat, r, res[0], res[1], latdir, dx, dy, dxy);

		size_t start=(r+1)*nc-1;
		if (std::isnan(v[start])) {
			dist[start] = std::min(dist[start], dist[start-nc] + dy);
		}
		for (size_t i=start-1; i>=(r*nc); i--) {
			if (std::isnan(v[i])) {
				dist[i] = std::min(std::min(std::min(dist[i], dist[i+1] + dx), dist[i-nc] + dy), dist[i-nc+1] + dxy);
			}
		}
	}
	if (spole) {
		double minp = *std::min_element(dist.end()-nc, dist.end());
		minp += dy;
		size_t ds = dist.size();
		for (size_t i=ds-nc; i<ds; i++) { 
			dist[i] = std::min(dist[i], minp);
		}
	}

	size_t off = (nr-1) * nc;
	above = std::vector<double>(dist.begin()+off, dist.end());
}


void broom_dist_geo_global(std::vector<double> &dist, std::vector<double> &v, std::vector<double> &above, std::vector<double> res, size_t nr, size_t nc, double lat, double latdir, bool npole, bool spole) {

//Rcpp::Rcout << npole << " " << spole << std::endl;

//	double dy = distance_lonlat(0, 0, 0, res[1]);
	double dx, dy, dxy;
	size_t stopnc = nc - 1;
	//top to bottom
    //left to right
	DxDxy(lat, 0, res[0], res[1], latdir, dx, dy, dxy);
	//first cell, no cell left of it
	if ( std::isnan(v[0]) ) { 
		dist[0] = std::min(std::min(std::min(above[0] + dy, above[stopnc] + dxy), dist[stopnc] + dx), dist[0]);
	} 
	//first row, no row above it, use "above"
	for (size_t i=1; i<nc; i++) { 
		if (std::isnan(v[i])) {
			dist[i] = std::min(std::min(std::min(above[i] + dy, above[i-1] + dxy), 
								dist[i-1] + dx), dist[i]);
		}
	}
	if (npole) {
		double minp = *std::min_element(dist.begin(), dist.begin()+nc);
		minp += dy;
		for (size_t i=0; i<nc; i++) { 
			dist[i] = std::min(dist[i], minp);
		}
	}

	//other rows
	for (size_t r=1; r<nr; r++) { 
		DxDxy(lat, r, res[0], res[1], latdir, dx, dy, dxy);
		size_t start=r*nc;
		if (std::isnan(v[start])) {
			dist[start] = std::min(std::min(std::min(dist[start-nc] + dy, dist[start-1] + dxy), 
									dist[start+stopnc] + dx), dist[start]);
		}

		size_t end = start+nc;
		for (size_t i=(start+1); i<end; i++) {
			if (std::isnan(v[i])) {
				dist[i] = std::min(std::min(std::min(dist[i-1] + dx, dist[i-nc] + dy), 
						dist[i-nc-1] + dxy), dist[i]);			
			}
		}
	}

	if (spole) {
		double minp = *std::min_element(dist.end()-nc, dist.end());
		minp += dy;
		size_t ds = dist.size();
		for (size_t i=ds-nc; i<ds; i++) { 
			dist[i] = std::min(dist[i], minp);
		}
	}

	//right to left
	DxDxy(lat, 0, res[0], res[1], latdir, dx, dy, dxy);
	 //first cell
	if ( std::isnan(v[stopnc])) {
		dist[stopnc] = std::min(std::min(std::min(dist[stopnc], above[stopnc] + dy), above[0] + dxy), dist[0] + dx);
	}

	// other cells on first row
	for (int i=(nc-2); i > -1; i--) { 
		if (std::isnan(v[i])) {
			dist[i] = std::min(std::min(std::min(dist[i+1] + dx, above[i+1] + dxy), 
						above[i] + dy), dist[i]);
		}
	}
	if (npole) {
		double minp = *std::min_element(dist.begin(), dist.begin()+nc);
		minp += dy;
		for (size_t i=0; i<nc; i++) { 
			dist[i] = std::min(dist[i], minp);
		}
	}

	// other rows
	for (size_t r=1; r<nr; r++) { 
		DxDxy(lat, r, res[0], res[1], latdir, dx, dy, dxy);

		size_t start=(r+1)*nc-1;
		if (std::isnan(v[start])) {
			//dist[start] = std::min(dist[start], dist[start-nc] + dy);
			dist[start] = std::min(std::min(std::min(dist[start], dist[start-nc] + dy), dist[start-nc-stopnc] + dxy)
							, dist[start-stopnc] + dx);
		}
		size_t end = r*nc;
		for (size_t i=start-1; i>=end; i--) {
			if (std::isnan(v[i])) {
			//	dist[i] = std::min(std::min(std::min(dist[i], dist[i+1] + dx), dist[i-nc] + dy), dist[i-nc+1] + dxy);
				dist[i] = std::min(std::min(std::min(dist[i], dist[i+1] + dx), dist[i-nc] + dy), 
						dist[i-nc+1] + dxy);
			}
		}
	}
	if (spole) {
		double minp = *std::min_element(dist.end()-nc, dist.end());
		minp += dy;
		size_t ds = dist.size();
		for (size_t i=ds-nc; i<ds; i++) { 
			dist[i] = std::min(dist[i], minp);
		}
	}

	size_t off = (nr-1) * nc;
	above = std::vector<double>(dist.begin()+off, dist.end());

}


SpatRaster SpatRaster::gridDistance(SpatOptions &opt) {

	SpatRaster out = geometry(1);
	if (!hasValues()) {
		out.setError("cannot compute distance for a raster with no values");
		return out;
	}

	SpatOptions ops(opt);
	size_t nl = nlyr();
	if (nl > 1) {
		out.source.resize(nl);
		for (unsigned i=0; i<nl; i++) {
			std::vector<unsigned> lyr = {i};
			SpatRaster r = subset(lyr, ops);
			r = r.gridDistance(ops);
			out.source[i] = r.source[0];
		}
		if (opt.get_filename() != "") {
			out = out.writeRaster(opt);
		}
		return out;
	}

	SpatRaster first = out.geometry();

	std::vector<double> res = resolution();
	size_t nc = ncol();	
	bool lonlat = is_lonlat(); 
    std::vector<double> d, v;
	std::vector<double> above(nc, std::numeric_limits<double>::infinity());

	if (!readStart()) {
		out.setError(getError());
		return(out);
	}
	
	if (lonlat) {
		bool global = is_global_lonlat();
		int polar = ns_polar();
		bool npole = (polar == 1) || (polar == 2);
		bool spole = (polar == -1) || (polar == 2);
		SpatRaster second = first;
		if (!first.writeStart(ops)) { return first; }
		for (size_t i = 0; i < first.bs.n; i++) {
			readBlock(v, first.bs, i);
			d.resize(v.size(), std::numeric_limits<double>::infinity());
			for (size_t j=0; j<d.size(); j++) {
				if (!std::isnan(v[j])) d[j] = 0;
			}
			double lat = yFromRow(first.bs.row[i]);
			bool np = (i==0) && npole;
			bool sp = (i==first.bs.n-1) && spole;
			if (global) {
				broom_dist_geo_global(d, v, above, res, first.bs.nrows[i], nc, lat, -1, np, sp);				
			} else {
				broom_dist_geo(d, v, above, res, first.bs.nrows[i], nc, lat, -1, np, sp);
			}
			if (!first.writeValues(d, first.bs.row[i], first.bs.nrows[i])) return first;
		}
		first.writeStop();

		if (!first.readStart()) {
			readStop();
			return(first);
		}
		if (!second.writeStart(ops)) {
			readStop();
			return second;
		}
		above = std::vector<double>(ncol(), std::numeric_limits<double>::infinity());
		for (int i = second.bs.n; i>0; i--) {
			readBlock(v, second.bs, i-1);
			first.readBlock(d, second.bs, i-1);
			std::reverse(v.begin(), v.end());
			std::reverse(d.begin(), d.end());
			double lat = yFromRow(second.bs.row[i-1] + second.bs.nrows[i-1] - 1);
			bool sp = (i==1) && spole; //! reverse order
			bool np = (i==(int)second.bs.n) && npole;
			if (global) {
				broom_dist_geo_global(d, v, above, res, second.bs.nrows[i-1], nc, lat, 1, np, sp);
			} else {
				broom_dist_geo(d, v, above, res, second.bs.nrows[i-1], nc, lat, 1, np, sp);				
			}			
			std::reverse(d.begin(), d.end());
			if (!second.writeValuesRect(d, second.bs.row[i-1], second.bs.nrows[i-1], 0, nc)) return second;
		}	
		second.writeStop();

		if (!second.readStart()) {
			readStop();
			return(second);
		}
		if (!out.writeStart(opt)) {
			readStop();
			return out;
		}
		above = std::vector<double>(ncol(), std::numeric_limits<double>::infinity());
		for (size_t i = 0; i < out.bs.n; i++) {
			readBlock(v, out.bs, i);
			second.readBlock(d, out.bs, i);
			double lat = yFromRow(first.bs.row[i]);			
			bool np = (i==0) && npole;
			bool sp = (i==out.bs.n-1) && spole;
			if (global) {
				broom_dist_geo_global(d, v, above, res, out.bs.nrows[i], nc, lat, -1, np, sp);
			} else {
				broom_dist_geo(d, v, above, res, out.bs.nrows[i], nc, lat, -1, np, sp);				
			}
			if (!out.writeValues(d, out.bs.row[i], out.bs.nrows[i])) return out;
		}	
		second.readStop();
		out.writeStop();
		readStop();
		
	} else {
		double m = source[0].srs.to_meter();
		m = std::isnan(m) ? 1 : m;

		if (!first.writeStart(ops)) { return first; }
		std::vector<double> vv;
		for (size_t i = 0; i < first.bs.n; i++) {
			readBlock(v, first.bs, i);
			d = broom_dist_planar(v, above, res, first.bs.nrows[i], nc, m);
			if (!first.writeValues(d, first.bs.row[i], first.bs.nrows[i])) return first;
		}
		first.writeStop();
		
		if (!first.readStart()) {
			out.setError(first.getError());
			return(out);
		}
		above = std::vector<double>(ncol(), std::numeric_limits<double>::infinity());
		if (!out.writeStart(opt)) {
			readStop();
			return out;
		}
		for (int i = out.bs.n; i>0; i--) {
			readBlock(v, out.bs, i-1);
			std::reverse(v.begin(), v.end());
			d = broom_dist_planar(v, above, res, out.bs.nrows[i-1], nc, m);	
			first.readBlock(vv, out.bs, i-1);
			std::transform(d.rbegin(), d.rend(), vv.begin(), vv.begin(), [](double a, double b) {return std::min(a,b);});
			if (!out.writeValuesRect(vv, out.bs.row[i-1], out.bs.nrows[i-1], 0, nc)) return out;
		}	
		out.writeStop();
		readStop();
	}
	return(out);
}


std::vector<double> do_edge(const std::vector<double> &d, const size_t nrow, const size_t ncol, const bool classes, const bool inner, const unsigned dirs, double falseval) {

	size_t n = nrow * ncol;
	std::vector<double> val(n, falseval);

	int r[8] = { -1,0,0,1 , -1,-1,1,1};
	int c[8] = { 0,-1,1,0 , -1,1,-1,1};

	if (!classes) {
		if (inner) { // inner
			for (size_t i = 1; i < (nrow-1); i++) {
				for (size_t j = 1; j < (ncol-1); j++) {
					size_t cell = i*ncol+j;
					val[cell] = NAN;
					if ( !std::isnan(d[cell])) {
						val[cell] = falseval;
						for (size_t k=0; k< dirs; k++) {
							if ( std::isnan(d[cell + r[k] * ncol + c[k]])) {
								val[cell] = 1;
								break;
							}
						}
					}
				}
			}

		} else { //outer
			for (size_t i = 1; i < (nrow-1); i++) {
				for (size_t j = 1; j < (ncol-1); j++) {
					size_t cell = i*ncol+j;
					val[cell] = falseval;
					if (std::isnan(d[cell])) {
						val[cell] = NAN;
						for (size_t k=0; k < dirs; k++) {
							if ( !std::isnan(d[cell+ r[k] * ncol + c[k] ])) {
								val[cell] = 1;
								break;
							}
						}
					}
				}
			}
		} 
	} else { // by class
		for (size_t i = 1; i < (nrow-1); i++) {
			for (size_t j = 1; j < (ncol-1); j++) {
				size_t cell = i*ncol+j;
				double test = d[cell+r[0]*ncol+c[0]];
				val[cell] = std::isnan(test) ? NAN : falseval;
				for (size_t k=1; k<dirs; k++) {
					double v = d[cell+r[k]*ncol +c[k]];
					if (std::isnan(test)) {
						if (!std::isnan(v)) {
							val[cell] = 1;
							break;
						}
					} else if (test != v) {
						val[cell] = 1;
						break;
					}
				}
			}
		}

	}
	return(val);
}



void addrowcol(std::vector<double> &v, size_t nr, size_t nc, bool rowbefore, bool rowafter, bool cols) {

	if (rowbefore) {
		v.insert(v.begin(), v.begin(), v.begin()+nc);
		nr++;
	}
	if (rowafter) {
		v.insert(v.end(), v.end()-nc, v.end());
		nr++;
	}
	if (cols) {
		for (size_t i=0; i<nr; i++) {
			size_t j = i*(nc+2);
			v.insert(v.begin()+j+nc, v[j+nc-1]);
			v.insert(v.begin()+j, v[j]);
		}
	}
}


void striprowcol(std::vector<double> &v, size_t nr, size_t nc, bool rows, bool cols) {
	if (rows) {
		v.erase(v.begin(), v.begin()+nc);
		v.erase(v.end()-nc, v.end());
		nr -= 2;
	}
	if (cols) {
		nc -= 2;
		for (size_t i=0; i<nr; i++) {
			size_t j = i*nc;
			v.erase(v.begin()+j);
			v.erase(v.begin()+j+nc);
		}
	}
}


SpatRaster SpatRaster::edges(bool classes, std::string type, unsigned directions, double falseval, SpatOptions &opt) {

	SpatRaster out = geometry();
	if (nlyr() > 1) {
		std::vector<unsigned> lyr = {0};
		SpatOptions ops(opt);
		out = subset(lyr, ops);
		out = out.edges(classes, type, directions, falseval, opt);
		out.addWarning("boundary detection is only done for the first layer");
		return out;
	}
	if (!hasValues()) {
		out.setError("SpatRaster has no values");
		return out;
	}


	if ((directions != 4) && (directions != 8)) {
		out.setError("directions should be 4 or 8");
		return(out);
	}
	if ((type != "inner") && (type != "outer")) {
		out.setError("directions should be 'inner' or 'outer'");
		return(out);
	}
	bool inner = type == "inner";

	size_t nc = ncol();
	size_t nr = nrow();


	if (!readStart()) {
		out.setError(getError());
		return(out);
	}

	opt.minrows = 2;
 	if (!out.writeStart(opt)) {
		readStop();
		return out;
	}

	for (size_t i = 0; i < out.bs.n; i++) {
		std::vector<double> v;
		//bool before = false;
		//bool after = false;
		if (i == 0) {
			if (out.bs.n == 1) {
				readValues(v, out.bs.row[i], out.bs.nrows[i], 0, nc);
				addrowcol(v, nr, nc, true, true, true);
			} else {
				readValues(v, out.bs.row[i], out.bs.nrows[i]+1, 0, nc);
				addrowcol(v, nr, nc, true, false, true);
				//after = true;
			}
		} else {
			//before = true;
			if (i == out.bs.n) {
				readValues(v, out.bs.row[i]-1, out.bs.nrows[i]+1, 0, nc);
				addrowcol(v, nr, nc, false, true, true);
			} else {
				readValues(v, out.bs.row[i]-1, out.bs.nrows[i]+2, 0, nc);
				addrowcol(v, nr, nc, false, false, true);
				//after = true;
			}
		}
		//before, after, 
		std::vector<double> vv = do_edge(v, out.bs.nrows[i]+2, nc+2, classes, inner, directions, falseval);
		striprowcol(vv, out.bs.nrows[i]+2, nc+2, true, true);
		if (!out.writeBlock(vv, i)) return out;
	}
	out.writeStop();
	readStop();

	return(out);
}



SpatRaster SpatRaster::buffer(double d, SpatOptions &opt) {

	SpatRaster out = geometry(1);
	if (!hasValues()) {
		out.setError("SpatRaster has no values");
		return out;
	}

	if (d <= 0) {
		out.setError("buffer size <= 0; nothing to compute");
		return out;
	}

	SpatOptions ops(opt);
	if (nlyr() > 1) {
		std::vector<unsigned> lyr = {0};
		out = subset(lyr, ops);
		out = out.buffer(d, opt);
		out.addWarning("buffer computations are only done for the first input layer");
		return out;
	}

	std::string etype = "inner";
	SpatRaster e = edges(false, etype, 8, 0, ops);
	SpatVector p = e.as_points(false, true, false, opt);
	out = out.disdir_vector_rasterize(p, false, true, false, false, ops);
	out = out.arith(d, "<=", false, opt);
	return out;
}


void SpatVector::fix_lonlat_overflow() {

	if (! ((extent.xmin < -180) || (extent.xmax > 180))) { return; }
	SpatExtent world(-180, 180, -90, 90);

	std::string vt = type();
	if (vt == "points") {
		for (size_t i=0; i<geoms.size(); i++) {
			SpatGeom g = geoms[i];
			for (size_t j=0; j<g.parts.size(); j++) {
				for (size_t k=0; k<g.parts[j].x.size(); k++) {
					if (geoms[i].parts[j].x[k] < -180) { geoms[i].parts[j].x[k] += 360; }
					if (geoms[i].parts[j].x[k] > 180) { geoms[i].parts[j].x[k] -= 360; }
				}
			}
		}
	} else {
		SpatExtent east(-360, -180, -180, 180);
		SpatExtent west(180, 360, -180, 180);

		for (size_t i=0; i<geoms.size(); i++) {
			if (geoms[i].extent.xmin < -180) {
				SpatVector v(geoms[i]);
				if (geoms[i].extent.xmax <= -180) {
					v = v.shift(360, 0);
				} else {
					SpatVector add = v.crop(east);
					add = add.shift(360, 0);
					v = v.crop(world);
					v.geoms[i].addPart(add.geoms[0].parts[0]);
				}
				replaceGeom(v.geoms[0], i);
			}
			if (geoms[i].extent.xmax > 180) {
				SpatVector v(geoms[i]);
				if (geoms[i].extent.xmin >= 180) {
					v = v.shift(-360, 0);
				} else {
					SpatVector add = v.crop(west);
					add = add.shift(-360, 0);
					v = v.crop(world);
					v.geoms[i].addPart(add.geoms[0].parts[0]);
				}
				replaceGeom(v.geoms[0], i);
			}
		}
	}

	if ((extent.ymax > 90) || (extent.ymin < -90)) {
		SpatVector out = crop(world);
		geoms = out.geoms;
		extent = out.extent;
		df = out.df;
		srs = out.srs;
	}
	return;
}


void sort_unique_2d(std::vector<double> &x, std::vector<double> &y) {
	std::vector<std::vector<double>> v(x.size());
	for (size_t i=0; i<v.size(); i++) {  
		v[i] = {x[i], y[i]};
	}
	std::sort(v.begin(), v.end(), [] 
		(const std::vector<double> &a, const std::vector<double> &b)
			{ return a[0] < b[0];});  

	v.erase(std::unique(v.begin(), v.end()), v.end());
	x.resize(v.size());
	y.resize(v.size());
	for (size_t i=0; i<x.size(); i++) {  
		x[i] = v[i][0];
		y[i] = v[i][1];
	}
}


void split_dateline(SpatVector &v) {
	SpatExtent e1 = {-1,  180, -91, 91};
	SpatExtent e2 = {180, 361, -91, 91};
	SpatVector ve(e1, "");
	SpatVector ve2(e2, "");
	ve = ve.append(ve2, true);
	v = v.intersect(ve);
	ve = v.subset_rows(1);
	ve = ve.shift(-360, 0);
	v.geoms[1] = ve.geoms[0];
	v = v.aggregate(false);
}




bool fix_date_line(SpatGeom &g, std::vector<double> &x, const std::vector<double> &y) {

	SpatPart p(x, y);
	double minx = vmin(x, false);
	double maxx = vmax(x, false);
	// need a better check but this should work for all normal cases
	if ((minx < -170) && (maxx > 170)) {
		for (size_t i=0; i<x.size(); i++) {
			if (x[i] < 0) {
				x[i] += 360;
			}
		}
		double minx2 = vmin(x, false);
		double maxx2 = vmax(x, false);
		if ((maxx - minx) < (maxx2 - minx2)) {
			g.reSetPart(p);
			return false;
		}
		p.x = x;
		g.reSetPart(p);
		SpatVector v(g);
		split_dateline(v);
		g = v.geoms[0];
		return true;
	} 
	g.reSetPart(p);
	return false;
}


SpatVector SpatVector::point_buffer(std::vector<double> d, unsigned quadsegs, bool no_multipolygons) { 

	SpatVector out;
	std::string vt = type();
	if (vt != "points") {
		out.setError("geometry must be points");
		return out;
	}

	size_t npts = size();

	size_t n = quadsegs * 4;
	double step = 360.0 / n;
	SpatGeom g(polygons);
	g.addPart(SpatPart(0, 0));

	std::vector<std::vector<double>> xy = coordinates();

	if (is_lonlat()) {
		std::vector<double> brng(n);
		for (size_t i=0; i<n; i++) {
			brng[i] = i * step;
		}
		double a = 6378137.0;
		double f = 1/298.257223563;
		struct geod_geodesic gd;
		geod_init(&gd, a, f);
		double lat, lon, azi, s12, azi2;

		// not checking for empty points

		for (size_t i=0; i<npts; i++) {
			if (std::isnan(xy[0][i] || std::isnan(xy[1][i]) || (xy[1][i]) > 90) || (xy[1][i] < -90)) {
				out.addGeom(SpatGeom(polygons));
			} else {
				std::vector<double> ptx;
				std::vector<double> pty;
				ptx.reserve(n);
				pty.reserve(n);
				geod_inverse(&gd, xy[1][i], xy[0][i],  90, xy[0][i], &s12, &azi, &azi2);
				bool npole = s12 < d[i];
				geod_inverse(&gd, xy[1][i], xy[0][i], -90, xy[0][i], &s12, &azi, &azi2);
				bool spole = s12 < d[i];
				if (npole && spole) {
					ptx = std::vector<double> {-180,  0, 180, 180, 180,   0, -180, -180};
					pty = std::vector<double> {  90, 90,  90,   0, -90, -90,  -90,    0};
					npole = false;
					spole = false;
				} else {
					for (size_t j=0; j < n; j++) {
						geod_direct(&gd, xy[1][i], xy[0][i], brng[j], d[i], &lat, &lon, &azi);
						ptx.push_back(lon);
						pty.push_back(lat);
					}
				}
				if (npole) {
					sort_unique_2d(ptx, pty);
					if (ptx[ptx.size()-1] < 180) {
						ptx.push_back(180);
						pty.push_back(pty[pty.size()-1]);
					}
					ptx.push_back(180);
					pty.push_back(90);
					ptx.push_back(-180);
					pty.push_back(90);
					if (ptx[0] > -180) {
						ptx.push_back(-180);
						pty.push_back(pty[0]);
					}
					ptx.push_back(ptx[0]);
					pty.push_back(pty[0]);
					g.reSetPart(SpatPart(ptx, pty));
					out.addGeom(g);
				} else if (spole) {
					sort_unique_2d(ptx, pty);
					if (ptx[ptx.size()-1] < 180) {
						ptx.push_back(180);
						pty.push_back(pty[pty.size()-1]);
					}
					ptx.push_back(180);
					pty.push_back(-90);
					ptx.push_back(-180);
					pty.push_back(-90);
					if (ptx[0] > -180) {
						ptx.push_back(-180);
						pty.push_back(pty[0]);
					}
					ptx.push_back(ptx[0]);
					pty.push_back(pty[0]);
					g.reSetPart(SpatPart(ptx, pty));
					out.addGeom(g);
				} else {
					ptx.push_back(ptx[0]);
					pty.push_back(pty[0]);
					bool split = fix_date_line(g, ptx, pty);
					if (split & no_multipolygons) {
						for (size_t j=0; j<g.parts.size(); j++) {
							SpatGeom gg(g.parts[j]);
							out.addGeom(gg);
						}
					} else {
						out.addGeom(g);
					}
				}
			}
		}
	} else {
		std::vector<double> cosb(n);
		std::vector<double> sinb(n);
		std::vector<double> px(n+1);
		std::vector<double> py(n+1);
		for (size_t i=0; i<n; i++) {
			double brng = i * step;
			brng = toRad(brng);
			cosb[i] = d[i] * cos(brng);
			sinb[i] = d[i] * sin(brng);
		}
		for (size_t i=0; i<npts; i++) {
			if (std::isnan(xy[0][i]) || std::isnan(xy[1][i])) {
				out.addGeom(SpatGeom(polygons));
			} else {
				for (size_t j=0; j<n; j++) {
					px[j] = xy[0][i] + cosb[j];
					py[j] = xy[1][i] + sinb[j];
				}
				px[n] = px[0];
				py[n] = py[0];
				g.setPart(SpatPart(px, py), 0);
				out.addGeom(g);
			}
		}
	}
	out.srs = srs;
	out.df = df;
	return(out);
}





double area_polygon_lonlat(geod_geodesic &g, const std::vector<double> &lon, const std::vector<double> &lat) {
	struct geod_polygon p;
	geod_polygon_init(&p, 0);
	size_t n = lat.size();
	for (size_t i=0; i < n; i++) {
		//double lat = lat[i] > 90 ? 90 : lat[i] < -90 ? -90 : lat[i];
		// for #397
		double flat = lat[i] < -90 ? -90 : lat[i];
		geod_polygon_addpoint(&g, &p, flat, lon[i]);
	}
	double area, P;
	geod_polygon_compute(&g, &p, 0, 1, &area, &P);
	return(area < 0 ? -area : area);
}



double area_polygon_plane(std::vector<double> x, std::vector<double> y) {
// based on http://paulbourke.net/geometry/polygonmesh/source1.c
	size_t n = x.size();
	double area = x[n-1] * y[0];
	area -= y[n-1] * x[0];
	for (size_t i=0; i < (n-1); i++) {
		area += x[i] * y[i+1];
		area -= x[i+1] * y[i];
	}
	area /= 2;
	return(area < 0 ? -area : area);
}


double area_lonlat(geod_geodesic &g, const SpatGeom &geom) {
	double area = 0;
	if (geom.gtype != polygons) return area;
	for (size_t i=0; i<geom.parts.size(); i++) {
		area += area_polygon_lonlat(g, geom.parts[i].x, geom.parts[i].y);
		for (size_t j=0; j < geom.parts[i].holes.size(); j++) {
			area -= area_polygon_lonlat(g, geom.parts[i].holes[j].x, geom.parts[i].holes[j].y);
		}
	}
	return area;
}


double area_plane(const SpatGeom &geom) {
	double area = 0;
	if (geom.gtype != polygons) return area;
	for (size_t i=0; i < geom.parts.size(); i++) {
		area += area_polygon_plane(geom.parts[i].x, geom.parts[i].y);
		for (size_t j=0; j < geom.parts[i].holes.size(); j++) {
			area -= area_polygon_plane(geom.parts[i].holes[j].x, geom.parts[i].holes[j].y);
		}
	}
	return area;
}


std::vector<double> SpatVector::area(std::string unit, bool transform, std::vector<double> mask) {

	size_t s = size();
	size_t m = mask.size();
	bool domask = false;
	if (m > 0) {
		if (s != mask.size()) {
			addWarning("mask size is not correct");
		} else {
			domask = true;
		}
	}

	std::vector<double> ar;
	ar.reserve(s);

	std::vector<std::string> ss {"m", "km", "ha"};
	if (std::find(ss.begin(), ss.end(), unit) == ss.end()) {
		setError("invalid unit");
		return {NAN};
	}
	double adj = unit == "m" ? 1 : unit == "km" ? 1000000 : 10000;

	if (srs.wkt == "") {
		addWarning("unknown CRS. Results can be wrong");
		if (domask) {
			for (size_t i=0; i<s; i++) {
				if (std::isnan(mask[i])) {
					ar.push_back(NAN);
				} else {
					ar.push_back(area_plane(geoms[i]));
				}
			}
		} else {
			for (size_t i=0; i<s; i++) {
				ar.push_back(area_plane(geoms[i]));
			}
		}
	} else {
		if (!srs.is_lonlat()) {
			if (transform) {
				SpatVector v = project("EPSG:4326");
				if (v.hasError()) {
					setError(v.getError());
					return {NAN};
				}
				return v.area(unit, false, mask);

			} else {
				double m = srs.to_meter();
				adj *= std::isnan(m) ? 1 : m * m;
				if (domask) {
					for (size_t i=0; i<s; i++) {
						if (std::isnan(mask[i])) {
							ar.push_back(NAN);
						} else {
							ar.push_back(area_plane(geoms[i]));
						}
					}
				} else {
					for (size_t i=0; i<s; i++) {
						ar.push_back(area_plane(geoms[i]));
					}
				}
			}

		} else {

			struct geod_geodesic g;
			double a = 6378137;
			double f = 1 / 298.257223563;
			geod_init(&g, a, f);
			if (domask) {
				for (size_t i=0; i<s; i++) {
					if (std::isnan(mask[i])) {
						ar.push_back(NAN);
					} else {
						ar.push_back(area_lonlat(g, geoms[i]));
					}
				}
			} else {
				for (size_t i=0; i<s; i++) {
					ar.push_back(area_lonlat(g, geoms[i]));
				}
			}
		}
	}

	if (adj != 1) {
		for (double& i : ar) i /= adj;
	}
	return ar;
}



double length_line_lonlat(geod_geodesic &g, const std::vector<double> &lon, const std::vector<double> &lat) {
	size_t n = lat.size();
	double length = 0;
	for (size_t i=1; i < n; i++) {
		length += distance_lonlat(lon[i-1], lat[i-1], lon[i], lat[i]);
	}
	return (length);
}


double length_line_plane(std::vector<double> x, std::vector<double> y) {
	size_t n = x.size();
	double length = 0;
	for (size_t i=1; i<n; i++) {
		length += sqrt(pow(x[i-1] - x[i], 2) + pow(y[i-1] - y[i], 2));
	}
	return (length);
}


double length_lonlat(geod_geodesic &g, const SpatGeom &geom) {
	double length = 0;
	if (geom.gtype == points) return length;
	for (size_t i=0; i<geom.parts.size(); i++) {
		length += length_line_lonlat(g, geom.parts[i].x, geom.parts[i].y);
		for (size_t j=0; j<geom.parts[i].holes.size(); j++) {
			length += length_line_lonlat(g, geom.parts[i].holes[j].x, geom.parts[i].holes[j].y);
		}
	}
	return length;
}


double length_plane(const SpatGeom &geom) {
	double length = 0;
	if (geom.gtype == points) return length;
	for (size_t i=0; i < geom.parts.size(); i++) {
		length += length_line_plane(geom.parts[i].x, geom.parts[i].y);
		for (size_t j=0; j < geom.parts[i].holes.size(); j++) {
			length += length_line_plane(geom.parts[i].holes[j].x, geom.parts[i].holes[j].y);
		}
	}
	return length;
}


std::vector<double> SpatVector::length() {

	size_t s = size();
	std::vector<double> r;
	r.reserve(s);

	double m = srs.to_meter();
	m = std::isnan(m) ? 1 : m;

	if (srs.wkt == "") {
		addWarning("unknown CRS. Results can be wrong");
	}
	
	if (m == 0) {
		struct geod_geodesic g;
		double a = 6378137;
		double f = 1 / 298.257223563;
		geod_init(&g, a, f);
		for (size_t i=0; i<s; i++) {
			r.push_back(length_lonlat(g, geoms[i]));
		}
	} else {
		for (size_t i=0; i<s; i++) {
			r.push_back(length_plane(geoms[i]) * m);
		}
	}
	return r;
}

SpatRaster SpatRaster::rst_area(bool mask, std::string unit, bool transform, int rcmax, SpatOptions &opt) {

	SpatRaster out = geometry(1);
	if (out.source[0].srs.wkt == "") {
		out.setError("empty CRS");
		return out;
	}

	std::vector<std::string> f {"m", "km", "ha"};
	if (std::find(f.begin(), f.end(), unit) == f.end()) {
		out.setError("invalid unit");
		return out;
	}

	if (opt.names.size() == 0) {
		opt.names = {"area"};
	}
	bool lonlat = is_lonlat();

	SpatOptions mopt(opt);
	if (mask) {
		if (!hasValues()) {
			mask = false;
		} else {
			mopt.filenames = opt.filenames;
			opt = SpatOptions(opt);
		}
	}


	SpatOptions xopt(opt);
	if (lonlat) { 
		bool disagg = false;
		SpatExtent extent = getExtent();
		if ((out.ncol() == 1) && ((extent.xmax - extent.xmin) > 180)) {
			disagg = true;
			std::vector<unsigned> fact = {1,2};
			out = out.disaggregate(fact, xopt);
		}
		SpatExtent e = {extent.xmin, extent.xmin+out.xres(), extent.ymin, extent.ymax};
		SpatRaster onecol = out.crop(e, "near", xopt);
		SpatVector p = onecol.as_polygons(false, false, false, false, false, xopt);
		if (p.hasError()) {
			out.setError(p.getError());
			return out;
		}
		std::vector<double> a = p.area(unit, true, {});
		size_t nc = out.ncol();
		if (disagg) {
			if (!out.writeStart(xopt)) { return out; }
		} else {
			if (!out.writeStart(opt)) { return out; }
		}
		for (size_t i = 0; i < out.bs.n; i++) {
			std::vector<double> v;
			v.reserve(out.bs.nrows[i] * nc);
			size_t r = out.bs.row[i];
			for (size_t j=0; j<out.bs.nrows[i]; j++) {
				v.insert(v.end(), nc, a[r]);
				r++;
			}
			if (!out.writeBlock(v, i)) return out;
		}
		out.writeStop();
		if (disagg) {
			SpatRaster tmp = out.to_memory_copy(xopt);
			std::vector<unsigned> fact = {1,2};
			opt.overwrite=true;
			out = tmp.aggregate(fact, "sum", true, opt); 
		}

	} else {
		if (transform) {
			bool resample = false;
//			SpatRaster empty = out.geometry(1);
			size_t rcx = std::max(rcmax, 10);
			unsigned frow = 1, fcol = 1;
			SpatRaster target = out.geometry(1);
			if ((nrow() > rcx) || (ncol() > rcx)) {
				resample = true;
				frow = (nrow() / rcx) + 1;	
				fcol = (ncol() / rcx) + 1;	
				out = out.aggregate({frow, fcol}, "mean", false, xopt);
				xopt.ncopies *= 5;
				if (!out.writeStart(xopt)) { return out; }
			} else {
				opt.ncopies *= 5;
				if (!out.writeStart(opt)) { return out; }
			}
			SpatRaster empty = out.geometry(1);
			SpatExtent extent = out.getExtent();
			double dy = out.yres() / 2;
			for (size_t i = 0; i < out.bs.n; i++) {
				double ymax = out.yFromRow(out.bs.row[i]) + dy;
				double ymin = out.yFromRow(out.bs.row[i] + out.bs.nrows[i]-1) - dy;
				SpatExtent e = {extent.xmin, extent.xmax, ymin, ymax};
				SpatRaster chunk = empty.crop(e, "near", xopt);
				SpatVector p = chunk.as_polygons(false, false, false, false, false, xopt);		
				std::vector<double> v = p.area(unit, true, {});
				if (!out.writeBlock(v, i)) return out;
				out.writeStop();
			}
			if (resample) {
				double divr = frow*fcol;
				out = out.arith(divr, "/", false, xopt);
				out = out.warper(target, "", "bilinear", false, false, opt);
			}
		} else {
			if (!out.writeStart(opt)) { return out; }
			double u = unit == "m" ? 1 : unit == "km" ? 1000000 : 10000;
			double m = out.source[0].srs.to_meter();
			double a = std::isnan(m) ? 1 : m;
			a *= xres() * yres() / u;
			for (size_t i = 0; i < out.bs.n; i++) {
				std::vector<double> v(out.bs.nrows[i]*ncol(), a);
				if (!out.writeBlock(v, i)) return out;
			}
			out.writeStop();
		}
	} 

	if (mask) {
		out = out.mask(*this, false, NAN, NAN, mopt);
	}
	return(out);
}


std::vector<double> SpatRaster::sum_area(std::string unit, bool transform, SpatOptions &opt) {

	if (source[0].srs.wkt == "") {
		setError("empty CRS");
		return {NAN};
	}

	std::vector<std::string> f {"m", "km", "ha"};
	if (std::find(f.begin(), f.end(), unit) == f.end()) {
		setError("invalid unit");
		return {NAN};
	}

	std::vector<double> out(nlyr(), 0);

	if (transform) { //avoid very large polygon objects
		opt.set_memfrac(std::max(0.1, opt.get_memfrac()/2));
	}
	BlockSize bs = getBlockSize(opt);
	if (!readStart()) {
		std::vector<double> err(nlyr(), -1);
		return(err);
	}

	if (is_lonlat()) {
		SpatRaster x = geometry(1);
		SpatExtent extent = x.getExtent();
		if ((x.ncol() == 1) && ((extent.xmax - extent.xmin) > 180)) {
			std::vector<unsigned> fact= {1,2};
			x = x.disaggregate(fact, opt);
		}
		size_t nc = x.ncol();
		SpatExtent e = {extent.xmin, extent.xmin+x.xres(), extent.ymin, extent.ymax};
		SpatRaster onecol = x.crop(e, "near", opt);
		SpatVector p = onecol.as_polygons(false, false, false, false, false, opt);
		std::vector<double> ar = p.area(unit, true, {});
		if (!hasValues()) {
			out.resize(1);
			for (size_t i=0; i<ar.size(); i++) {
				out[0] += ar[i] * nc;
			}
		} else {
			for (size_t i=0; i<bs.n; i++) {
				std::vector<double> v;
				readValues(v, bs.row[i], bs.nrows[i], 0, ncol());
				size_t blockoff = bs.nrows[i] * nc;
				for (size_t lyr=0; lyr<nlyr(); lyr++) {
					size_t lyroff = lyr * blockoff;
					for (size_t j=0; j<bs.nrows[i]; j++) {
						size_t row = bs.row[i] + j;
						size_t offset = lyroff + row * nc;
						size_t n = offset + nc;
						for (size_t k=offset; k<n; k++) {
							if (!std::isnan(v[k])) out[lyr] += ar[row];
						}
					}
				}
			}
		}
	} else if (transform) {
		SpatRaster x = geometry(1);
		SpatExtent extent = x.getExtent();
		double dy = x.yres() / 2;
		SpatOptions popt(opt);
		if (!hasValues()) {
			out.resize(1);
			for (size_t i=0; i<bs.n; i++) {
				double ymax = x.yFromRow(bs.row[i]) + dy;
				double ymin = x.yFromRow(bs.row[i] + bs.nrows[i]-1) - dy;
				SpatExtent e = {extent.xmin, extent.xmax, ymin, ymax};
				SpatRaster onechunk = x.crop(e, "near", popt);
				SpatVector p = onechunk.as_polygons(false, false, false, false, false, popt);
				p = p.project("EPSG:4326");
				std::vector<double> v = p.area(unit, true, 	{});
				out[0] += accumulate(v.begin(), v.end(), 0.0);
			}
		} else {
			for (size_t i=0; i<bs.n; i++) {
				double ymax = x.yFromRow(bs.row[i]) + dy;
				double ymin = x.yFromRow(bs.row[i] + bs.nrows[i]-1) - dy;
				SpatExtent e = {extent.xmin, extent.xmax, ymin, ymax};
				SpatRaster onechunk = x.crop(e, "near", popt);
				SpatVector p = onechunk.as_polygons(false, false, false, false, false, popt);
				p = p.project("EPSG:4326");
				std::vector<double> par = p.area(unit, true, {});

				std::vector<double> v;
				readValues(v, bs.row[i], bs.nrows[i], 0, ncol());
				unsigned off = bs.nrows[i] * ncol() ;
				for (size_t lyr=0; lyr<nlyr(); lyr++) {
					unsigned offset = lyr * off;
					unsigned n = offset + off;
					for (size_t j=offset; j<n; j++) {
						if (!std::isnan(v[j])) {
							out[lyr] += par[j - offset];
						}
					}
				}
			}

		}
	} else {
		double adj = unit == "m" ? 1 : unit == "km" ? 1000000 : 10000;
		double m = source[0].srs.to_meter();
		m = std::isnan(m) ? 1 : m;
		double ar = xres() * yres() * m * m / adj;
		if (!hasValues()) {
			out.resize(1);
			out[0] = ncell() * ar;
		} else {
			for (size_t i=0; i<bs.n; i++) {
				std::vector<double> v;
				readValues(v, bs.row[i], bs.nrows[i], 0, ncol());
				unsigned off = bs.nrows[i] * ncol() ;
				for (size_t lyr=0; lyr<nlyr(); lyr++) {
					unsigned offset = lyr * off;
					unsigned n = offset + off;
					for (size_t j=offset; j<n; j++) {
						if (!std::isnan(v[j])) out[lyr]++;
					}
				}
			}
			for (size_t lyr=0; lyr<nlyr(); lyr++) {
				out[lyr] = out[lyr] * ar;
			}
		}
	}
	readStop();
	return(out);
}



//layer<value-area
std::vector<std::vector<double>> SpatRaster::area_by_value(SpatOptions &opt) {

	double m = source[0].srs.to_meter();
	m = std::isnan(m) ? 1 : m;

	if (m != 0) {
		double ar = xres() * yres() * m * m;
		std::vector<std::vector<double>> f = freq(true, false, 0, opt);
		for (size_t i=0; i<f.size(); i++) {
			size_t fs = f[i].size();
			for (size_t j=fs/2; j<fs; j++) {
				f[i][j] *= ar;
			}
		}
		return f;
	} else {
		// to do
		// combine freq and area to get area by latitudes

		std::vector<std::vector<double>> out(nlyr());
		return out;
	}
}



void do_flowdir(std::vector<double> &val, std::vector<double> const &d, size_t nrow, size_t ncol, double dx, double dy, unsigned seed, bool before, bool after) {

	if (!before) {
		val.resize(val.size() + ncol, NAN);
	}

	std::vector<double> r = {0, 0, 0, 0, 0, 0, 0, 0};
	std::vector<double> p = {1, 2, 4, 8, 16, 32, 64, 128}; // pow(2, j)
	double dxy = sqrt(dx * dx + dy * dy);

	std::default_random_engine generator(seed);
	std::uniform_int_distribution<> U(0, 1);

	for (size_t row=1; row< (nrow-1); row++) {
		val.push_back(NAN);
		for (size_t col=1; col< (ncol-1); col++) {
			size_t i = row * ncol + col;
			if (!std::isnan(d[i])) {
				r[0] = (d[i] - d[i+1]) / dx;
				r[1] = (d[i] - d[i+1+ncol]) / dxy;
				r[2] = (d[i] - d[i+ncol]) / dy;
				r[3] = (d[i] - d[i-1+ncol]) / dxy;
				r[4] = (d[i] - d[i-1]) / dx;
				r[5] = (d[i] - d[i-1-ncol]) / dxy;
				r[6] = (d[i] - d[i-ncol]) / dy;
				r[7] = (d[i] - d[i+1-ncol]) / dxy;
				// using the lowest neighbor, even if it is higher than the focal cell.
				double dmin = r[0];
				int k = 0;
				for (size_t j=1; j<8; j++) {
					if (r[j] > dmin) {
						dmin = r[j];
						k = j;
					} else if (r[j] == dmin) {
						if (U(generator)) {
							dmin = r[j];
							k = j;
						}
					}
				}
				val.push_back( p[k] );
			} else {
				val.push_back( NAN );
			}
		}
		val.push_back(NAN);
	}

	if (!after) {
		val.resize(val.size() + ncol, NAN);
	}

}


void do_TRI(std::vector<double> &val, std::vector<double> const &d, size_t nrow, size_t ncol, bool before, bool after) {
	if (!before) {
		val.resize(val.size() + ncol, NAN);
	}
	for (size_t row=1; row< (nrow-1); row++) {
		val.push_back(NAN);
		for (size_t col=1; col< (ncol-1); col++) {
			size_t i = row * ncol + col;
			val.push_back( 
				(fabs(d[i-1-ncol]-d[i]) + fabs(d[i-1]-d[i]) + fabs(d[i-1+ncol]-d[i]) +  fabs(d[i-ncol]-d[i]) + fabs(d[i+ncol]-d[i]) +  fabs(d[i+1-ncol]-d[i]) + fabs(d[i+1]-d[i]) +  fabs(d[i+1+ncol]-d[i])) / 8 
			);
		}
		val.push_back(NAN);
	}
	if (!after) {
		val.resize(val.size() + ncol, NAN);
	}
}

void do_TPI(std::vector<double> &val, const std::vector<double> &d, const size_t nrow, const size_t ncol, bool before, bool after) {
	if (!before) {
		val.resize(val.size() + ncol, NAN);
	}
	for (size_t row=1; row< (nrow-1); row++) {
		val.push_back(NAN);
		for (size_t col=1; col< (ncol-1); col++) {
			size_t i = row * ncol + col;
			val.push_back( d[i] - (d[i-1-ncol] + d[i-1] + d[i-1+ncol] + d[i-ncol]
			+ d[i+ncol] + d[i+1-ncol] + d[i+1] + d[i+1+ncol]) / 8 );
		}
		val.push_back(NAN);
	}
/*
	if (expand) {
		for (size_t i=1; i < (ncol-1); i++) {
			val[i+add] = d[i] - (d[i-1] + d[i-1+ncol] + d[i+ncol] + d[i+1] + d[i+1+ncol]) / 5;
			size_t j = i+(nrow-1) * ncol;
			val[j+add] = d[j] - (d[j-1-ncol] + d[j-1] + d[j-ncol] + d[j+1-ncol] + d[j+1]) / 5;
		}
		for (size_t row=1; row< (nrow-1); row++) {
			size_t i = row * ncol;
			val[i+add] = d[i] - (d[i-ncol] + d[i+ncol] + d[i+1-ncol] + d[i+1] + d[i+1+ncol]) / 5;
			i += ncol - 1;
			val[i+add] = d[i] - (d[i-ncol] + d[i] + d[i+ncol] + d[i-ncol]) / 5;
		}
		size_t i = 0;
		val[i+add] = d[i] - (d[i+ncol] + d[i+1] + d[i+1+ncol]) / 3;
		i = ncol-1;
		val[i+add] = d[i] - (d[i+ncol] + d[i-1] + d[i-1+ncol]) / 3;
		i = (nrow-1)*ncol;
		val[i+add] = d[i] - (d[i-ncol] + d[i+1] + d[i+1-ncol]) / 3;
		i = (nrow*ncol)-1;
		val[i+add] = d[i] - (d[i-ncol] + d[i-1] + d[i-1-ncol]) / 3;
	}
*/
	if (!after) {
		val.resize(val.size() + ncol, NAN);
	}


}



void do_roughness(std::vector<double> &val, const std::vector<double> &d, size_t nrow, size_t ncol, bool before, bool after) {

	if (!before) {
		val.resize(val.size() + ncol, NAN);
	}

	int incol = ncol;
	int a[9] = { -1-incol, -1, -1+incol, -incol, 0, incol, 1-incol, 1, 1+incol };
	double min, max, v;
	for (size_t row=1; row< (nrow-1); row++) {
		val.push_back(NAN);
		for (size_t col=1; col< (ncol-1); col++) {
			size_t i = row * ncol + col;
			min = d[i + a[0]];
			max = d[i + a[0]];
			for (size_t j = 1; j < 9; j++) {
				v = d[i + a[j]]; 
				if (v > max) {
					max = v;
				} else if (v < min) {
					min = v;
				}
			}
			val.push_back(max - min);
		}
		val.push_back(NAN);
	}
	if (!after) {
		val.resize(val.size() + ncol, NAN);
	}
}


#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif




void to_degrees(std::vector<double>& x, size_t start) { 
	double adj = 180 / M_PI;
	for (size_t i=start; i<x.size(); i++) {
		x[i] *= adj;
	}
}


void do_slope(std::vector<double> &val, const std::vector<double> &d, unsigned ngb, unsigned nrow, unsigned ncol, double dx, double dy, bool geo, std::vector<double> &gy, bool degrees, bool before, bool after) {

	size_t start = val.size();
	if (!before) {
		val.resize(start + ncol, NAN);
	}
	
	std::vector<double> ddx;
	if (geo) {
		ddx.resize(nrow);
		for (size_t i=0; i<nrow; i++) {
			ddx[i] = distHaversine(-dx, gy[i], dx, gy[i]) / 2 ;
		}
	} 

	if (ngb == 4) {
		if (geo) {
			double xwi[2] = {-1,1};
			double xw[2] = {0,0};
			double yw[2] = {-1,1};

			for (size_t i=0; i<2; i++) {
				yw[i] = yw[i] / (2 * dy);
			}

			for (size_t row=1; row< (nrow-1); row++) {
				for (size_t k=0; k<2; k++) {
					xw[k] = xwi[k] / (-2 * ddx[row]);
				}
				val.push_back(NAN); // first col
				for (size_t col=1; col< (ncol-1); col++) {
					size_t i = row * ncol + col;

					double zx = d[i-1] * xw[0] + d[i+1] * xw[1];
					double zy = d[i-ncol] * yw[0] + d[i+ncol] * yw[1];
					val.push_back( atan(sqrt( pow(zy, 2) + pow(zx, 2) )));
				}
				val.push_back(NAN); // last col
			}
		} else { // ngb == 8

			double xw[2] = {-1,1};
			double yw[2] = {-1,1};
			for (size_t i=0; i<2; i++) {
				xw[i] /= -2 * dx;
				yw[i] /=  2 * dy;
			}
			for (size_t row=1; row< (nrow-1); row++) {
				val.push_back(NAN); 
				for (size_t col=1; col< (ncol-1); col++) {
					size_t i = row * ncol + col;
					double zx = d[i-1] * xw[0] + d[i+1] * xw[1];
					double zy = d[i-ncol] * yw[0] + d[i+ncol] * yw[1];
					val.push_back(  atan( sqrt( pow(zy, 2) + pow(zx, 2)  )));
				}
				val.push_back(NAN); 
			}
		}
	} else {

		if (geo) {

			double xwi[6] = {-1,-2,-1,1,2,1};
			double xw[6] = {0,0,0,0,0,0};
			double yw[6] = {-1,1,-2,2,-1,1};

			for (size_t i=0; i<6; i++) {
				yw[i] = yw[i] / (8 * dy);
			}

			for (size_t row=1; row< (nrow-1); row++) {
				for (size_t k=0; k<6; k++) {
					xw[k] = xwi[k] / (8 * ddx[row]);
				}
				val.push_back(NAN); 
				for (size_t col=1; col< (ncol-1); col++) {
					size_t i = row * ncol + col;
					double zx = d[i-1-ncol] * xw[0] + d[i-1] * xw[1] + d[i-1+ncol] * xw[2]
					   + d[i+1-ncol] * xw[3] + d[i+1] * xw[4] + d[i+1+ncol] * xw[5];
					   
					double zy = d[i-1-ncol] * yw[0] + d[i-1+ncol] * yw[1] + d[i-ncol] * yw[2] 
							+ d[i+ncol] * yw[3] + d[i+1-ncol] * yw[4] + d[i+1+ncol] * yw[5];
					val.push_back( atan(sqrt( pow(zy, 2) + pow(zx, 2))));
				}
				val.push_back(NAN); 
			}
		} else {

			double xw[6] = {-1,-2,-1,1,2,1};
			double yw[6] = {-1,1,-2,2,-1,1};
			for (size_t i=0; i<6; i++) {
				xw[i] /= -8 * dx;
				yw[i] /= 8 * dy;
			}
			for (size_t row=1; row< (nrow-1); row++) {
				val.push_back(NAN); 
				for (size_t col=1; col< (ncol-1); col++) {
					size_t i = row * ncol + col;
					double zx = d[i-1-ncol] * xw[0] + d[i-1] * xw[1] + d[i-1+ncol] * xw[2]
							+ d[i+1-ncol] * xw[3] + d[i+1] * xw[4] + d[i+1+ncol] * xw[5];
					double zy = d[i-1-ncol] * yw[0] + d[i-1+ncol] * yw[1] + d[i-ncol] * yw[2] 
							+ d[i+ncol] * yw[3] + d[i+1-ncol] * yw[4] + d[i+1+ncol] * yw[5];
					val.push_back( atan(sqrt( pow(zy, 2) + pow(zx, 2) )));
				}
				val.push_back(NAN); 
			}
		} 
	}
	if (!after) {
		val.resize(val.size() + ncol, NAN);
	}
	
	if (degrees) {
		to_degrees(val, start);
	} 
}


double dmod(double x, double n) {
	return(x - n * std::floor(x/n));
}



void do_aspect(std::vector<double> &val, const std::vector<double> &d, unsigned ngb, unsigned nrow, unsigned ncol, double dx, double dy, bool geo, std::vector<double> &gy, bool degrees, bool before, bool after) {

	size_t start = val.size();
	if (!before) {
		val.resize(start + ncol, NAN);
	}
	std::vector<double> ddx;
	if (geo) {
		ddx.resize(nrow);
		for (size_t i=0; i<nrow; i++) {
			ddx[i] = distHaversine(-dx, gy[i], dx, gy[i]) / 2 ;
		}
	} 
	double zy, zx; 

	//double const pi2 = M_PI / 2;
	double const twoPI = 2 * M_PI;
	double const halfPI = M_PI / 2;

	if (ngb == 4) {
		if (geo) {
			double xwi[2] = {-1,1};
			double xw[2] = {0,0};
			double yw[2] = {-1,1};

			for (size_t i=0; i<2; i++) {
				yw[i] = yw[i] / (2 * dy);
			}

			for (size_t row=1; row < (nrow-1); row++) {
				val.push_back(NAN); 
				for (size_t k=0; k<2; k++) {
					xw[k] = xwi[k] / (-2 * ddx[row]);
				}
				for (size_t col=1; col< (ncol-1); col++) {
					size_t i = row * ncol + col;
					zx = d[i-1] * xw[0] + d[i+1] * xw[1];
					zy = d[i-ncol] * yw[0] + d[i+ncol] * yw[1];
					zx = atan2(zy, zx);
					val.push_back( std::fmod( halfPI - zx, twoPI) );
				}
				val.push_back(NAN); 
			}
		} else {

			double xw[2] = {-1,1};
			double yw[2] = {-1,1};
			for (size_t i=0; i<2; i++) {
				xw[i] = xw[i] / (-2 * dx);
				yw[i] = yw[i] / (2 * dy);
			}
			for (size_t row=1; row< (nrow-1); row++) {
				val.push_back(NAN); 
				for (size_t col=1; col< (ncol-1); col++) {
					size_t i = row * ncol + col;
					zx = d[i-1] * xw[0] + d[i+1] * xw[1];
					zy = d[i-ncol] * yw[0] + d[i+ncol] * yw[1];
					zx = atan2(zy, zx);
					val.push_back( dmod( halfPI - zx, twoPI));
				}
				val.push_back(NAN); 
			}
		}
	} else { //	(ngb == 8) {

		if (geo) {

			double xwi[6] = {-1,-2,-1,1,2,1};
			double xw[6] = {0,0,0,0,0,0};
			double yw[6] = {-1,1,-2,2,-1,1};

			for (size_t i=0; i<6; i++) {
				yw[i] /= (8 * dy);
			}

			for (size_t row=1; row < (nrow-1); row++) {
				val.push_back(NAN); 
				for (size_t k=0; k<6; k++) {
					xw[k] = xwi[k] / (-8 * ddx[row]);
				}
				for (size_t col=1; col< (ncol-1); col++) {
					size_t i = row * ncol + col;

					zx = d[i-1-ncol] * xw[0] + d[i-1] * xw[1] + d[i-1+ncol] * xw[2]
							+ d[i+1-ncol] * xw[3] + d[i+1] * xw[4] + d[i+1+ncol] * xw[5];
					zy = d[i-1-ncol] * yw[0] + d[i-1+ncol] * yw[1] + d[i-ncol] * yw[2] 
							+ d[i+ncol] * yw[3] + d[i+1-ncol] * yw[4] + d[i+1+ncol] * yw[5];

					zx = atan2(zy, zx);
					val.push_back( dmod( halfPI - zx, twoPI) );
				}
				val.push_back(NAN); 
			}

		} else {

			double xw[6] = {-1,-2,-1,1,2,1};
			double yw[6] = {-1,1,-2,2,-1,1};
			for (size_t i=0; i<6; i++) {
				xw[i] /= -8 * dx;
				yw[i] /= 8 * dy;
			}
			for (size_t row=1; row< (nrow-1); row++) {
				val.push_back(NAN); 
				for (size_t col=1; col< (ncol-1); col++) {
					size_t i = row * ncol + col;
					zx = d[i-1-ncol] * xw[0] + d[i-1] * xw[1] + d[i-1+ncol] * xw[2]
						+ d[i+1-ncol] * xw[3] + d[i+1] * xw[4] + d[i+1+ncol] * xw[5];
					zy = d[i-1-ncol] * yw[0] + d[i-1+ncol] * yw[1] + d[i-ncol] * yw[2] 
							+ d[i+ncol] * yw[3] + d[i+1-ncol] * yw[4] + d[i+1+ncol] * yw[5];
					zx = atan2(zy, zx);
					val.push_back( dmod( halfPI - zx, twoPI) );
				}
				val.push_back(NAN); 
			}
		}
	}
	if (!after) {
		val.resize(val.size() + ncol, NAN);
	}

	if (degrees) {
		to_degrees(val, start);
	}
}


SpatRaster SpatRaster::terrain(std::vector<std::string> v, unsigned neighbors, bool degrees, unsigned seed, SpatOptions &opt) {

//TPI, TRI, aspect, flowdir, slope, roughness
	//std::sort(v.begin(), v.end());
	//v.erase(std::unique(v.begin(), v.end()), v.end());

	SpatRaster out = geometry(v.size());
	out.setNames(v);

	if (nlyr() > 1) {
		out.setError("terrain needs a single layer object");
		return out;
	}

	bool aspslope = false;
	std::vector<std::string> f {"TPI", "TRI", "aspect", "flowdir", "slope", "roughness"};
	for (size_t i=0; i<v.size(); i++) {
		if (std::find(f.begin(), f.end(), v[i]) == f.end()) {
			out.setError("unknown terrain variable: " + v[i]);
			return(out);
		}
		if (v[i] == "slope" || v[i] == "aspect") {
			aspslope=true;
		} 
	}

	if ((neighbors != 4) && (neighbors != 8)) {
		out.setError("neighbors should be 4 or 8");
		return out;
	}
	bool lonlat = is_lonlat();
	double yr = yres();

	if (!readStart()) {
		out.setError(getError());
		return(out);
	}
	
	opt.minrows = 3;
  	if (!out.writeStart(opt)) {
		readStop();
		return out;
	}
	size_t nc = ncol();

	if (nrow() < 3 || nc < 3) {
		for (size_t i = 0; i < out.bs.n; i++) {
			std::vector<double> val(out.bs.nrows[i] * nc, NAN);
			if (!out.writeBlock(val, i)) return out;
		}
		return out;
	}


	std::vector<double> y;
	for (size_t i = 0; i < out.bs.n; i++) {
		std::vector<double> d;
		bool before= false;
		bool after = false;
		size_t rrow = out.bs.row[i];
		size_t rnrw = out.bs.nrows[i];
		if (i > 0) {
			rrow--;
			rnrw++;
			before=true;
		}	
		if ((out.bs.row[i] + out.bs.nrows[i]) < nrow()) {
			rnrw++;
			after = true;
		}
		readValues(d, rrow, rnrw, 0, nc);
		if (lonlat && aspslope) {
			std::vector<int_64> rows(rnrw);
			std::iota(rows.begin(), rows.end(), rrow);
			y = yFromRow(rows);
			yr = distHaversine(0, 0, 0, yres());
		}
		std::vector<double> val;
		val.reserve(out.bs.nrows[i] * ncol() * v.size());
		for (size_t j =0; j<v.size(); j++) {
			if (v[j] == "slope") {
				do_slope(val, d, neighbors, rnrw, nc, xres(), yr, lonlat, y, degrees, before, after);
			} else if (v[j] == "aspect") {
				do_aspect(val, d, neighbors, rnrw, nc, xres(), yr, lonlat, y, degrees, before, after);
			} else if (v[j] == "flowdir") {
				double dx = xres();
				double dy = yres();
				if (lonlat) {
					double yhalf = yFromRow((size_t) nrow()/2);
					dx = distHaversine(0, yhalf, dx, yhalf);
					dy = distHaversine(0, 0, 0, dy);
				}
				do_flowdir(val, d, rnrw, nc, dx, dy, seed, before, after);
			} else if (v[j] == "roughness") {
				do_roughness(val, d, rnrw, nc, before, after);
			} else if (v[j] == "TPI") {
				do_TPI(val, d, rnrw, nc, before, after);
			} else if (v[j] == "TRI") {
				do_TRI(val, d, rnrw, nc, before, after);
			} else {
				out.setError("?"); return out;
			}
		}
		if (!out.writeBlock(val, i)) return out;
	}
	out.writeStop();
	readStop();
	return out; 
}

