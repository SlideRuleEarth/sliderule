local runner = require("test_executive")
local asset = require("asset")

local pp = require("prettyprint")

-- Setup --

local assets = asset.loaddir("../data/asset_directory.csv")

local expected = {
    dataset1={driver="file", url="/data/1"},
    dataset2={driver="file", url="/data/2"},
    dataset3={driver="s3",   url="/data/3"}
}

local function check_query(act, exp)
    for _, e in pairs(exp) do
        local found = false
        for _, v in pairs(act) do
            if tostring(e) == v then
                found = true
            end
        end
        runner.assert(found, string.format('Failed to return resource %s', tostring(e)))
    end
end

-- Self Test --

runner.unittest("Print Info", function()
    for _,v in pairs(assets) do
        local name, identity, driver, url, index_filename, endpoint, status = v:info()
        runner.compare(driver, expected[name]["driver"])
        runner.compare(url, expected[name]["url"])
        runner.assert(status)
    end
end)

runner.unittest("Retrieve Existing Asset", function()
    local a = core.getbyname("dataset1")
    local name, identity, driver, url, index_filename, endpoint, status = a:info()
    runner.compare(name, "dataset1")
    runner.compare(driver, expected["dataset1"]["driver"])
    runner.compare(url, expected["dataset1"]["url"])
end)

runner.unittest("Display Time Tree for Dataset1", function()
    local a = core.getbyname("dataset1")
    core.intervalindex(a, "t0", "t1"):display()
end)

runner.unittest("Query Dataset", function()
    local a = core.getbyname("dataset1")
    local ii = core.intervalindex(a, "t0", "t1")
    local r = ii:query({t0=5.0, t1=17.0})
    check_query(r, { 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17})
end)

runner.unittest("Query Dataset with Field Index", function()
    local a = core.getbyname("dataset1")
    local f = core.pointindex(a, "foot")
    local new_resource = {name="46",t0=46,t1=46,lat0=-83.2,lat1=-80.1,lon0=45.0,lon1=46.0,hand=255.4,foot=15}
    a:load(new_resource["name"], new_resource)
    f:add(new_resource)
    f:display()
    local r = f:query({foot=15})
    check_query(r, { 1, 4, 7, 10, 13, 14, 17, 18, 21, 22, 25, 26, 29, 30, 33, 34, 37, 38, 41, 42, 45, 46})
end)

runner.unittest("Query Overlapping Dataset", function()
    local a = core.getbyname("dataset2")
    local name, identity, driver, url, index_filename, endpoint, status = a:info()
    runner.compare(name, "dataset2")
    runner.compare(driver, expected["dataset2"]["driver"])
    runner.compare(url, expected["dataset2"]["url"])
    local ii = core.intervalindex(a, "t0", "t1"):display()
    local r = ii:query({t0=6.0, t1=10.0})
    check_query(r, {"B", "C", "D", "E", "F", "G", "H", "I", "J"})
end)

runner.unittest("Test Spatial Index", function()
    local a = core.getbyname("dataset3")
    local si = core.spatialindex(a, core.SOUTH_POLAR)

    local lat = -80.0
    local lon = 45.0
    while lat < 80.0 do
        local x, y = si:project(lon, lat)
        local nlon, nlat = si:sphere(x, y)
        runner.assert(runner.cmpfloat(math.abs(x), math.abs(y), 0.0000001))
        runner.assert(runner.cmpfloat(lat, nlat, 0.0000001))
        runner.assert(runner.cmpfloat(lon, nlon, 0.0000001))
        lat = lat + 10.0
    end

    lat = -60.0
    lon = -170.0
    while lon < 170.0 do
        local x, y = si:project(lon, lat)
        local nlon, nlat = si:sphere(x, y)
        runner.assert(runner.cmpfloat(lon, nlon, 0.0000001))
        runner.assert(runner.cmpfloat(lat, nlat, 0.0000001))
        lon = lon + 10.0
    end
end)

runner.unittest("Query Dataset with Sptial Index", function()
    local a = core.getbyname("dataset1")
    local si = core.spatialindex(a, core.SOUTH_POLAR):display()
    local r = si:query({lat0=-83.2, lon0=45.0, lat1=-73.2, lon1=55.0})
    check_query(r, { 1, 4, 7, 10, 13, 14, 17, 18, 21, 22, 25, 26, 29, 30, 33, 34, 37, 38, 41, 42, 45})
end)

-- Report Results --

runner.report()
