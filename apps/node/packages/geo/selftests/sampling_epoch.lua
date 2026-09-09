local runner = require("test_executive")
local srcfile, dirpath = runner.srcscript()
local base64 = require("base64")

-- Setup --

-- ramp_east_4mm.tif and ramp_north_4mm.tif are synthetic 251x251 Float32 planes on a 4 mm
-- NAD83(2011) / UTM zone 11N grid (EPSG:6340) in the western US: value = easting - 671037 on
-- the east ramp and value = northing - 3996711 on the north ramp.  Nearest-neighbour
-- sampling therefore returns the projected coordinate of the location the raster was
-- sampled at, quantised to the 4 mm pixel (+/- 2 mm).  That location depends on the epoch of the point: the
-- point CRS (ITRF2014, EPSG:7912) is dynamic and the raster CRS is static, so PROJ applies
-- the time-dependent Helmert "ITRF2014 to NAD83(2011) (1)".  Without a point time the
-- operation is evaluated at its reference epoch, 2010.0; with a point time it must be
-- evaluated at that time.  Expected values from pyproj 3.7.2 / PROJ 9.7.1, EPSG:7912 ->
-- EPSG:6340 at (-115.1, 36.1): 2010.0 -> (671036.9550, 3996710.8276), 2024.5 ->
-- (671037.1574, 3996710.9839), i.e. the plate moved the sampling location by
-- (+0.202, +0.156) m between the two epochs.
local lon = -115.1
local lat =   36.1

local expEast2010  = -0.0450   -- easting - 671037 at epoch 2010.0
local expNorth2010 = -0.1724   -- northing - 3996711 at epoch 2010.0
local expDE        =  0.2024   -- displacement of the sampled location, 2024.5 minus 2010.0
local expDN        =  0.1563
local sigma        =  0.005    -- metres: 4 mm pixel quantisation (+/- 2 mm) plus the Helmert (no grids, PROJ versions agree to < 1 mm)

local function loadRaster(name)
    local f = io.open(dirpath.."../data/"..name, "rb")
    assert(f)
    local rasterfile = f:read("*a")
    f:close()
    local encodedRaster = base64.encode(rasterfile)
    local params = {
        data = encodedRaster,
        length = string.len(encodedRaster),
        date = 0,
        samples = {
            bands = {"1"},
            algorithm = "NearestNeighbour"
        }
    }
    local robj = geo.userraster(params)
    runner.assert(robj ~= nil, "failed to create userraster "..name, true)
    return robj
end

local function sampleAt(robj, timestr)
    local tbl, err = robj:sample(lon, lat, 0, timestr)
    runner.assert(err == 0, "failed to sample raster", true)
    runner.assert(tbl ~= nil and tbl[1] ~= nil, "no sample returned", true)
    return tbl[1]["value"]
end

-- Self Test --

runner.unittest("Sampling Epoch", function()

    local east  = loadRaster("ramp_east_4mm.tif")
    local north = loadRaster("ramp_north_4mm.tif")

    local e_none = sampleAt(east)
    local e_2010 = sampleAt(east,  "2010-01-01T00:00:00Z")
    local e_2024 = sampleAt(east,  "2024-07-02T00:00:00Z")   -- 2024.5 (leap year, day 184)
    local n_none = sampleAt(north)
    local n_2010 = sampleAt(north, "2010-01-01T00:00:00Z")
    local n_2024 = sampleAt(north, "2024-07-02T00:00:00Z")

    print(string.format("east  ramp: none %.4f  2010 %.4f  2024.5 %.4f  (dE %.4f, expected %.4f)", e_none, e_2010, e_2024, e_2024 - e_2010, expDE))
    print(string.format("north ramp: none %.4f  2010 %.4f  2024.5 %.4f  (dN %.4f, expected %.4f)", n_none, n_2010, n_2024, n_2024 - n_2010, expDN))

    -- a point without a time is sampled at the operation's reference epoch (unchanged behaviour)
    runner.assert(e_none == e_2010, string.format("no-time east sample %f != 2010 sample %f", e_none, e_2010))
    runner.assert(n_none == n_2010, string.format("no-time north sample %f != 2010 sample %f", n_none, n_2010))

    -- the reference-epoch location itself
    runner.assert(math.abs(e_2010 - expEast2010)  < sigma, string.format("east sample at 2010 = %f, expected %f", e_2010, expEast2010))
    runner.assert(math.abs(n_2010 - expNorth2010) < sigma, string.format("north sample at 2010 = %f, expected %f", n_2010, expNorth2010))

    -- a point with a time is sampled at the epoch of the point
    runner.assert(math.abs((e_2024 - e_2010) - expDE) < sigma, string.format("east displacement 2024.5 - 2010 = %f, expected %f", e_2024 - e_2010, expDE))
    runner.assert(math.abs((n_2024 - n_2010) - expDN) < sigma, string.format("north displacement 2024.5 - 2010 = %f, expected %f", n_2024 - n_2010, expDN))
end)

-- Report Results --

runner.report()
