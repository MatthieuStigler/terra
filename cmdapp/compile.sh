#!/bin/bash
#g++  `gdal-config --cflags` `gdal-config --libs` -std=c++11 -I/usr/include/gdal  -I../src/  ../src/aggregate.cpp ../src/area.cpp ../src/arith.cpp ../src/buffer.cpp ../src/categories.cpp ../src/cellnumber.cpp ../src/crs.cpp ../src/distance.cpp ../src/distRaster.cpp ../src/extent.cpp ../src/extract.cpp ../src/file_utils.cpp ../src/focal.cpp ../src/gdal.cpp ../src/GeographicLib_geodesic.c ../src/geos_methods.cpp ../src/math_utils.cpp  ../src/memory.cpp  ../src/modal.cpp  ../src/names.cpp ../src/pointInPolygon.cpp  ../src/ram.cpp ../src/ram.h ../src/raster_coerce.cpp ../src/raster_methods.cpp ../src/raster_stats.cpp ../src/rasterFromFile.cpp ../src/read.cpp ../src/read_gdal.cpp ../src/read_ogr.cpp ../src/reclassify.cpp ../src/sample.cpp ../src/sources.cpp ../src/spatDataframe.cpp ../src/spatOptions.cpp ../src/spatRaster.cpp ../src/spatVector.cpp ../src/string_utils.cpp ../src/string_utils.h ../src/terrain.cpp  ../src/resample.cpp ../src/project.cpp ../src/write.cpp ../src/write_gdal.cpp ../src/write_mem.cpp ../src/write_ogr.cpp  main.cpp -o terra

g++ -o terra -std=c++11 -I/usr/include/gdal  -I../src/   ../src/crs.cpp \
    ../src/spatDataframe.cpp ../src/spatVector.cpp ../src/spatRaster.cpp ../src/string_utils.cpp \
    ../src/gdalio.cpp ../src/memory.cpp  ../src/math_utils.cpp  \
    ../src/read.cpp ../src/read_gdal.cpp  \
   ../src/ram.cpp  ../src/raster_methods.cpp ../src/raster_stats.cpp ../src/rasterize.cpp ../src/spatSources.cpp  ../src/spatOptions.cpp \
   ../src/spatDataframe.cpp ../src/vecmathfun.cpp ../src/write.cpp ../src/write_gdal.cpp  main.cpp show.cpp  \
    -lgdal -lproj -ltiff -lgeotiff  -I/usr/include/gdal  -Dstandalone

# ../src/aggregate.cpp ../src/area.cpp ../src/arith.cpp ../src/buffer.cpp ../src/categories.cpp ../src/cellnumber.cpp ../src/crs.cpp ../src/distance.cpp ../src/distRaster.cpp ../src/extent.cpp ../src/extract.cpp ../src/file_utils.cpp ../src/focal.cpp ../src/gdal.cpp ../src/GeographicLib_geodesic.c ../src/geos_methods.cpp ../src/math_utils.cpp  ../src/memory.cpp  ../src/modal.cpp  ../src/names.cpp ../src/pointInPolygon.cpp  ../src/ram.cpp ../src/ram.h ../src/raster_coerce.cpp ../src/raster_methods.cpp ../src/raster_stats.cpp ../src/rasterFromFile.cpp ../src/read.cpp ../src/read_gdal.cpp ../src/read_ogr.cpp ../src/reclassify.cpp ../src/sample.cpp ../src/sources.cpp ../src/spatDataframe.cpp ../src/spatOptions.cpp ../src/spatRaster.cpp ../src/spatVector.cpp ../src/string_utils.cpp ../src/string_utils.h ../src/terrain.cpp  ../src/resample.cpp ../src/project.cpp ../src/write.cpp ../src/write_gdal.cpp ../src/write_mem.cpp ../src/write_ogr.cpp  main.cpp -lgdal -I/usr/include/gdal

