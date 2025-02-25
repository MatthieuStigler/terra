
setMethod("tapp", signature(x="SpatRaster"), 
function(x, index, fun, ..., cores=1, filename="", overwrite=FALSE, wopt=list()) {

	stopifnot(!any(is.na(index)))
	if (length(index) > nlyr(x)) {
		error("tapp", "length(index) > nlyr(x)")
	}
	if (!is.factor(index)) {
		index <- factor(index, levels=unique(index))
	}
	nms <- as.character(index)
	ind <- as.integer(index)
	d <- unique(data.frame(nms, ind, stringsAsFactors=FALSE))
	uin <- d[,2]
	nms <- make.names(d[,1])
	nms <- nms[uin]
	txtfun <- .makeTextFun(fun)
	if (inherits(txtfun, "character")) { 
		if (txtfun %in% .cpp_funs) {
			opt <- spatOptions(filename, overwrite, wopt=wopt)
			narm <- isTRUE(list(...)$na.rm)
			x@ptr <- x@ptr$apply(index, txtfun, narm, nms, opt)
			return(messages(x, "tapp"))
		}
	}
	fun <- match.fun(fun)

	nl <- nlyr(x)
	ind <- rep_len(index, nl)
	out <- rast(x)
	nlyr(out) <- length(uin)
	names(out) <- nms

	doclust <- FALSE
	if (inherits(cores, "cluster")) {
		doclust <- TRUE
	} else if (cores > 1) {
		doclust <- TRUE
		cores <- parallel::makeCluster(cores)
		on.exit(parallel::stopCluster(cores))
	}

	readStart(x)
	on.exit(readStop(x), add=TRUE)
	b <- writeStart(out, filename, overwrite, wopt=wopt)

	if (doclust) {
		pfun <- function(x, ...) apply(x, 1, FUN=fun, ...)
		parallel::clusterExport(cores, "pfun", environment())
		for (i in 1:b$n) {
			v <- readValues(x, b$row[i], b$nrows[i], 1, ncol(out), TRUE)
			v <- lapply(uin, function(i) v[, ind==i, drop=FALSE])
			v <- parallel::parLapply(cores, v, pfun, ...)
			v <- do.call(cbind, v)
			writeValues(out, v, b$row[i], b$nrows[i])
		}
	} else {
		for (i in 1:b$n) {
			v <- readValues(x, b$row[i], b$nrows[i], 1, ncol(out), TRUE)
			# like this, na.rm is not passed to FUN
			# v <- lapply(uin, function(j, ...) apply(v[, ind==uin[j], drop=FALSE], 1, FUN=fun, ...))
			# like this it works
			v <- lapply(uin, function(j) apply(v[, ind==uin[j], drop=FALSE], 1, FUN=fun, ...))
			v <- do.call(cbind, v)
			writeValues(out, v, b$row[i], b$nrows[i])
		}
	}
	out <- writeStop(out)
	return(out)
}
)


