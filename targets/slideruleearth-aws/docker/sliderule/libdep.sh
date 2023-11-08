echo PDAL: $(git --git-dir /PDAL/.git --work-tree /PDAL describe --abbrev --dirty --always --tags --long) > /host/libdep.lock
echo PROJ: $(git --git-dir /PROJ/.git --work-tree /PROJ describe --abbrev --dirty --always --tags --long) >> /host/libdep.lock
echo arrow: $(git --git-dir /arrow/.git --work-tree /arrow describe --abbrev --dirty --always --tags --long) >> /host/libdep.lock
echo gdal: $(git --git-dir /gdal/.git --work-tree /gdal describe --abbrev --dirty --always --tags --long) >> /host/libdep.lock
echo geos: $(git --git-dir /geos/.git --work-tree /geos describe --abbrev --dirty --always --tags --long) >> /host/libdep.lock
echo libgeotiff: $(git --git-dir /libgeotiff/.git --work-tree /libgeotiff describe --abbrev --dirty --always --tags --long) >> /host/libdep.lock
echo rapidjson: $(git --git-dir /rapidjson/.git --work-tree /rapidjson  describe --abbrev --dirty --always --tags --long) >> /host/libdep.lock