\name{terrain}

\alias{terrain}
\alias{terrain,SpatRaster-method}

\title{terrain characteristics}

\description{
Compute terrain characteristics from elevation data. The elevation values should be in the same units as the map units (typically meter) for projected (planar) raster data. They should be in meter when the coordinate reference system is longitude/latitude. 
}

\usage{
\S4method{terrain}{SpatRaster}(x, v="slope", neighbors=8, unit="degrees", filename="", ...)  
}

\arguments{
  \item{x}{SpatRaster, single layer with elevation values. Values should have the same unit as the map units, or in meters when the crs is longitude/latitude}
  \item{v}{character. One or more of these options: slope, aspect, TPI, TRI, roughness, flowdir (see Details)}
  \item{unit}{character. "degrees" or "radians" for the output of "slope" and "aspect"}
  \item{neighbors}{integer. Indicating how many neighboring cells to use to compute slope or aspect with. Either 8 (queen case) or 4 (rook case)}
  \item{filename}{character. Output filename}
  \item{...}{list. Options for writing files as in \code{\link{writeRaster}}}
}

\details{
When \code{neighbors=4}, slope and aspect are computed according to Fleming and Hoffer (1979) and Ritter (1987). When \code{neighbors=8}, slope and aspect are computed according to Horn (1981). The Horn algorithm may be best for rough surfaces, and the Fleming and Hoffer algorithm may be better for smoother surfaces (Jones, 1997; Burrough and McDonnell, 1998).

If slope = 0, aspect is set to 0.5*pi radians (or 90 degrees if unit="degrees"). When computing slope or aspect, the coordinate reference system of \code{x} must be known for the algorithm to differentiate between planar and longitude/latitude data.

\code{terrain} is not vectorized over "neighbors" or "unit" -- only the first value is used.

flowdir returns the "flow direction" (of water), that is the direction of the greatest drop in elevation (or the smallest rise if all neighbors are higher). They are encoded as powers of 2 (0 to 7). The cell to the right of the focal cell is 1, the one below that is 2, and so on:
\tabular{rrr}{
32 \tab64 \tab 128\cr 
16 \tab x \tab 1 \cr 
 8 \tab 4 \tab 2 \cr }

If two cells have the same drop in elevation, a random cell is picked. That is not ideal as it may prevent the creation of connected flow networks. ArcGIS implements the approach of Greenlee (1987) and I might adopt that in the future.

The terrain indices are according to Wilson et al. (2007), as in \href{https://gdal.org/programs/gdaldem.html}{gdaldem}. TRI (Terrain Ruggedness Index) is the mean of the absolute differences between the value of a cell and the value of its 8 surrounding cells. TPI (Topographic Position Index) is the difference between the value of a cell and the mean value of its 8 surrounding cells. Roughness is the difference between the maximum and the minimum value of a cell and its 8 surrounding cells.

Such measures can also be computed with the \code{\link{focal}} function:

f <- matrix(1, nrow=3, ncol=3)

TRI <- focal(x, w=f, fun=function(x, ...) sum(abs(x[-5]-x[5]))/8)

TPI <- focal(x, w=f, fun=function(x, ...) x[5] - mean(x[-5]))

rough <- focal(x, w=f, fun=function(x, ...) {max(x) - min(x)}, na.rm=TRUE)
}

\references{
Burrough, P., and R.A. McDonnell, 1998. Principles of Geographical Information Systems. Oxford University Press.

Fleming, M.D. and Hoffer, R.M., 1979. Machine processing of Landsat MSS data and DMA topographic data for forest cover type mapping. LARS Technical Report 062879. Laboratory for Applications of Remote Sensing, Purdue University, West Lafayette, Indiana.

Horn, B.K.P., 1981. Hill shading and the reflectance map. Proceedings of the IEEE 69:14-47

Jones, K.H., 1998. A comparison of algorithms used to compute hill terrain as a property of the DEM. Computers & Geosciences 24: 315-323 

Ritter, P., 1987. A vector-based terrain and aspect generation algorithm. Photogrammetric Engineering and Remote Sensing 53: 1109-1111
}

\examples{
f <- system.file("ex/elev.tif", package="terra")
r <- rast(f)
x <- terrain(r, "slope")
}

\keyword{spatial}
