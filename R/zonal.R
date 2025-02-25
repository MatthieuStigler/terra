
setMethod("zonal", signature(x="SpatRaster", z="SpatRaster"), 
	function(x, z, fun="mean", ..., as.raster=FALSE, filename="", wopt=list())  {
		if (nlyr(z) > 1) {
			z <- z[[1]]
		}
		zname <- names(z)
		txtfun <- .makeTextFun(match.fun(fun))
		if (inherits(txtfun, "character") && (txtfun %in% c("max", "min", "mean", "sum"))) {
			na.rm <- isTRUE(list(...)$na.rm)
			opt <- spatOptions()
			ptr <- x@ptr$zonal(z@ptr, txtfun, na.rm, opt)
			messages(ptr, "zonal")
			out <- .getSpatDF(ptr)
		} else {
			nl <- nlyr(x)
			res <- list()
			vz <- values(z)
			nms <- names(x)
			for (i in 1:nl) {
				d <- stats::aggregate(values(x[[i]]), list(zone=vz), fun, ...)
				colnames(d)[2] <- nms[i]
				res[[i]] <- d
			}
			out <- res[[1]]
			if (nl > 1) {
				for (i in 2:nl) {
					out <- merge(out, res[[i]])
				}
			}
		}
		if (as.raster) {
			if (is.null(wopt$names)) {
				wopt$names <- names(x)
			}
			subst(z, out[,1], out[,-1], filename=filename, wopt=wopt)
		} else {
			if (is.factor(z)) {
				levs <- active_cats(z)[[1]]
				m <- match(out$zone, levs[,1])
				out$zone <- levs[m, 2]
			}
			colnames(out)[1] <- zname
			out
		}
	}
)


setMethod("global", signature(x="SpatRaster"), 
	function(x, fun="mean", weights=NULL, ...)  {

		nms <- names(x)
		nms <- make.unique(nms)
		txtfun <- .makeTextFun(fun)

		opt <- spatOptions()
		if (!is.null(weights)) {
			stopifnot(inherits(weights, "SpatRaster"))
			stopifnot(txtfun %in% c("mean", "sum"))
			na.rm <- isTRUE(list(...)$na.rm)
			ptr <- x@ptr$global_weighted_mean(weights@ptr, txtfun, na.rm, opt)
			messages(ptr, "global")
			res <- (.getSpatDF(ptr))
			rownames(res) <- nms
			return(res)
		}

		if (inherits(txtfun, "character")) { 
			if (txtfun %in% c("prod", "max", "min", "mean", "sum", "range", "rms", "sd", "sdpop", "notNA", "isNA")) {
				na.rm <- isTRUE(list(...)$na.rm)
				ptr <- x@ptr$global(txtfun, na.rm, opt)
				messages(ptr, "global")
				res <- .getSpatDF(ptr)

				rownames(res) <- nms
				return(res)
			}
		}

		nl <- nlyr(x)
		res <- list()
		for (i in 1:nl) {
			res[[i]] <- fun(values(x[[i]]), ...)
		}
		res <- do.call(rbind, res)
		res <- data.frame(res)

		# more efficient but more risky:
		#apply(data.frame(x), 2, fun, ...)

		if ((ncol(res) == 1) && (colnames(res) == "res")) {
			colnames(res) <- "global"
		}

		rownames(res) <- nms
		res
	}
)
