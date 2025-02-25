\name{erase}

\docType{methods}

\alias{erase}
\alias{erase,SpatVector,SpatVector-method}
\alias{erase,SpatVector,missing-method}
\alias{erase,SpatVector,SpatExtent-method}

\title{ Erase parts of a SpatVector object}

\description{
Erase parts of a SpatVector with another SpatVector or with a SpatExtent. You can also erase (parts of) polygons with the other polygons of the same SpatVector.

}

\usage{
\S4method{erase}{SpatVector,SpatVector}(x, y)

\S4method{erase}{SpatVector,missing}(x)

\S4method{erase}{SpatVector,SpatExtent}(x, y)
}

\arguments{
  \item{x}{SpatVector}
  \item{y}{SpatVector or SpatExtent}
}

\value{
SpatVector or SpatExtent
}

\seealso{
\code{\link{intersect}}, \code{\link{crop}}. 
The equivalent for SpatRaster is \code{\link{mask}}
}


\examples{
f <- system.file("ex/lux.shp", package="terra")
v <- vect(f)
e <- ext(5.6, 6, 49.55, 49.7)
x <- erase(v, e)

p <- vect("POLYGON ((5.8 49.8, 6 49.9, 6.15 49.8, 6 49.6, 5.8 49.8))")
y <- erase(v, p)

# self-erase
h <- convHull(v[-12], "NAME_1")
he <- erase(h)
plot(h, lwd=2, border="red", lty=2)
lines(he, col="gray", lwd=3)
}

\keyword{methods}
\keyword{spatial}

