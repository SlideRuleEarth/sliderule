local runner = require("test_executive")
local asset = require("asset")
local console = require("console")
local td = runner.rootdir(arg[0]) .. "../tests"

-- Setup --

console.monitor:config(core.INFO)
assets = asset.loaddir(td.."/asset_directory.csv")

expected = {
    dataset1={format="json", url="/data/1"},
    dataset2={format="binary", url="/data/2"},
    dataset3={format="binary", url="/data/3"}
}

local function check_query(act, exp)
    for _, e in pairs(exp) do
        found = false
        for _, v in pairs(act) do
            if tostring(e) == v then
                found = true
            end
        end
        runner.check(found, string.format('Failed to return resource %s', tostring(e)))
    end
end

-- Unit Test --

sys.log(core.RAW, '\n------------------\nTest01: Print Info\n------------------\n')
for _,v in pairs(assets) do
    name, format, url, index_filename, status = v:info()
    runner.compare(format, expected[name]["format"])
    runner.compare(url, expected[name]["url"])
    runner.check(status)
end

sys.log(core.RAW, '\n------------------\nTest02: Retrieve Existing Asset\n------------------\n')
local a2 = core.getbyname("dataset1")
name, format, url, index_filename, status = a2:info()
runner.compare(name, "dataset1")
runner.compare(format, expected["dataset1"]["format"])
runner.compare(url, expected["dataset1"]["url"])

sys.log(core.RAW, '\n------------------\nTest03: Display Time Tree for Dataset1\n------------------\n')
local i3 = core.intervalindex(a2, "t0", "t1")
i3:name("intervalindex")
i3:display()

sys.log(core.RAW, '\n------------------\nTest04: Query Dataset1\n------------------\n')
local r4 = i3:query({t0=5.0, t1=17.0})
local e4 = { 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17} 
check_query(r4, e4)

sys.log(core.RAW, '\n------------------\nTest05: Query Dataset1 with Field Index\n------------------\n')
local f5 = core.pointindex(a2, "foot")
local new_resource = {name="46",t0=46,t1=46,lat0=-83.2,lat1=-80.1,lon0=45.0,lon1=46.0,hand=255.4,foot=15}
a2:load(new_resource["name"], new_resource)
f5:add(new_resource)
f5:name("pointindex")
f5:display()
local r5 = f5:query({foot=15})
local e5 = { 1, 4, 7, 10, 13, 14, 17, 18, 21, 22, 25, 26, 29, 30, 33, 34, 37, 38, 41, 42, 45, 46} 
check_query(r5, e5)

sys.log(core.RAW, '\n------------------\nTest06: Query Overlapping Dataset\n------------------\n')
local a6 = core.getbyname("dataset2")
name, format, url, index_filename, status = a6:info()
runner.compare(name, "dataset2")
runner.compare(format, expected["dataset2"]["format"])
runner.compare(url, expected["dataset2"]["url"])
local i6 = core.intervalindex(a6, "t0", "t1")
i6:name("overlappingindex")
i6:display()
local r6 = i6:query({t0=6.0, t1=10.0})
local e6 = {"B", "C", "D", "E", "F", "G", "H", "I", "J"} 
check_query(r6, e6)

sys.log(core.RAW, '\n------------------\nTest07: Test Sptial Index\n------------------\n')
local a7 = core.getbyname("dataset3")
local i7 = core.spatialindex(a7, core.SOUTH_POLAR)

lat = -80.0
lon = 45.0
while lat < 80.0 do
    x, y = i7:project(lat, lon) 
    nlat, nlon = i7:sphere(x, y)
    runner.check(runner.cmpfloat(math.abs(x), math.abs(y), 0.0000001))
    runner.check(runner.cmpfloat(lat, nlat, 0.0000001))
    runner.check(runner.cmpfloat(lon, nlon, 0.0000001))
    lat = lat + 10.0
end

lat = -60.0
lon = -170.0
while lon < 170.0 do
    x, y = i7:project(lat, lon) 
    nlat, nlon = i7:sphere(x, y)
    runner.check(runner.cmpfloat(lat, nlat, 0.0000001))
    runner.check(runner.cmpfloat(lon, nlon, 0.0000001))
    lon = lon + 10.0
end

sys.log(core.RAW, '\n------------------\nTest08: Query Dataset1 with Spatial Index\n------------------\n')
local a8 = core.getbyname("dataset1")
local i8 = core.spatialindex(a8, core.SOUTH_POLAR)
i8:name("spatialindex")
i8:display()
local r5 = i8:query({lat0=-83.2, lon0=45.0, lat1=-73.2, lon1=55.0})
local e5 = { 1, 4, 7, 10, 13, 14, 17, 18, 21, 22, 25, 26, 29, 30, 33, 34, 37, 38, 41, 42, 45} 
check_query(r5, e5)

-- Clean Up --

-- Report Results --

sys.wait(1)
runner.report()
