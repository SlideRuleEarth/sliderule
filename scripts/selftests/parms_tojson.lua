local runner = require("test_executive")
local console = require("console")
local asset = require("asset")
local assets = asset.loaddir()
local json = require("json")
local prettyprint = require("prettyprint")

-- Setup --
-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)


local poly = {
    {lat = 0.73734824791566,    lon = 172.51715474926},
    {lat = 0.73734824791566,    lon = 173.47569646684},
    {lat = 2.1059226076334,     lon = 173.47569646684},
    {lat = 2.1059226076334,     lon = 172.51715474926},
    {lat = 0.73734824791566,    lon = 172.51715474926}
}

local td = runner.rootdir(arg[0])
local geojsonfile = td.."/grandmesa.geojson"
local f = io.open(geojsonfile, "r")
local vectorfile = nil

if f ~= nil then
    vectorfile = f:read("*a")
    f:close()
else
    runner.check(false, "failed to open geojson file")
end

local js = ""
local jsonstr = ""

if __arrow__ then
    print(string.format("\n--------------------------------\nDefault arrow.parms.export\n--------------------------------"))
    js = core.parms({}):export()["output"]
    runner.check(string.len(json.encode(js)) > 0)

    print(string.format("\n--------------------------------\nUserSet arrow.parms.export\n--------------------------------"))
    js = core.parms({path="/tmp/samples.geoparquet", format="parquet", as_geo=true}):export()["output"]
    runner.check(string.len(json.encode(js)) > 0)
end

if __cre__ then
    print(string.format("\n--------------------------------\nDefault cre.parms.export\n--------------------------------"))
    js = cre.parms({}):export()
    runner.check(string.len(json.encode(js)) > 0)

    print(string.format("\n--------------------------------\nUserSet cre.parms.export\n--------------------------------"))
    js = cre.parms({image="/tmp/fakeimage.jpg", command="fakecommand"}):export()
    runner.check(js.image == "/tmp/fakeimage.jpg", js.image)
    runner.check(js.command == "fakecommand", js.command)
end

if __geo__ then
    print(string.format("\n--------------------------------\nDefault geo.parms.export\n--------------------------------"))
    jsonstr = geo.parms({}):tojson()
    runner.check(string.len(jsonstr) > 0)

    print(string.format("\n--------------------------------\nUserSet geo.parms.export\n--------------------------------"))
    jsonstr = geo.parms({ asset = "arcticdem-mosaic", algorithm = "Bilinear", radius = 0, bands = {"NDVI", "B03"}}):tojson()
    js = json.decode(jsonstr)
    runner.check(js.sampling_algo == "Bilinear", js.sampling_algo)
    runner.check(js.bands_list[1] == "NDVI", js.bands_list[1])
    runner.check(js.bands_list[2] == "B03", js.bands_list[2])
end

print(string.format("\n--------------------------------\nDefault core.parms.export\n--------------------------------"))
js = core.parms({}):export()
runner.check(string.len(json.encode(js)) > 0)

print(string.format("\n--------------------------------\nUserSet 'polygon' core.parms.export\n--------------------------------"))
js = core.parms({poly=poly}):export()
runner.check(js.poly[1].lat == 0.73734824791566, js.poly[1].lat)
runner.check(js.poly[1].lon == 172.51715474926,  js.poly[1].lon)
runner.check(js.poly[2].lat == 0.73734824791566, js.poly[2].lat)
runner.check(js.poly[2].lon == 173.47569646684,  js.poly[2].lon)
runner.check(js.poly[3].lat == 2.1059226076334,  js.poly[3].lat)
runner.check(js.poly[3].lon == 173.47569646684,  js.poly[3].lon)
runner.check(js.poly[4].lat == 2.1059226076334,  js.poly[4].lat)
runner.check(js.poly[4].lon == 172.51715474926,  js.poly[4].lon)
runner.check(js.poly[5].lat == 0.73734824791566, js.poly[5].lat)
runner.check(js.poly[5].lon == 172.51715474926,  js.poly[5].lon)

print(string.format("\n--------------------------------\nUserSet 'raster' core.parms.export\n--------------------------------"))
js = core.parms({region_mask={geojson=vectorfile, cellsize=0.01}}):export()
local geojsonFile = json.decode(js.region_mask.geojson)  -- raster is another json string (geojson file content)
runner.check(geojsonFile.name == "grand_mesa_poly", geojsonFile.name)

if __swot__ then
    print(string.format("\n--------------------------------\nDefault swot.parms.export\n--------------------------------"))
    js = swot.parms({}):export()
    runner.check(string.len(json.encode(js)) > 0)

    print(string.format("\n--------------------------------\nUserSet swot.parms.export\n--------------------------------"))
    js = swot.parms({variables={"var1", "var2"}}):export()
    runner.check(js.variables[1] == "var1", js.variables[1])
    runner.check(js.variables[2] == "var2", js.variables[2])
end

if __gedi__ then
    print(string.format("\n--------------------------------\nDefault gedi.parms.export\n--------------------------------"))
    js = gedi.parms({}):export()
    runner.check(string.len(json.encode(js)) > 0)

    print(string.format("\n--------------------------------\nUserSet gedi.parms.export\n--------------------------------"))
    js = gedi.parms({degrade_filter=true, surface_filter=true, l2_quality_filter=true, l4_quality_filter=true}):export()
    prettyprint.display(js)
    runner.check(js.degrade_filter    == true, string.format("gedi parm incorrect: %s", tostring(js.degrade_filter)))
    runner.check(js.surface_filter    == true, string.format("gedi parm incorrect: %s", tostring(js.surface_filter)))
    runner.check(js.l2_quality_filter == true, string.format("gedi parm incorrect: %s", tostring(js.l2_quality_filter)))
    runner.check(js.l4_quality_filter == true, string.format("gedi parm incorrect: %s", tostring(js.l4_quality_filter)))
end

if __icesat2__ then
    print(string.format("\n--------------------------------\nDefault icesat2.parms.export\n--------------------------------"))
    js = icesat2.parms({}):export()
--    runner.check(string.len(json.encode(js)) > 0)

    print(string.format("\n--------------------------------\nUserSet icesat2.parms.export\n--------------------------------"))
    js = icesat2.parms({cnf=4, track=icesat2.RPT_1, atl03_geo_fields={"solar_elevation"}}):export()
    runner.check(js.atl03_geo_fields[1] == "solar_elevation")
end

-- Report Results --

runner.report()

