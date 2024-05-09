local runner = require("test_executive")
local console = require("console")
local asset = require("asset")
local assets = asset.loaddir()
local json = require("json")

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


local jsonstr = ""
local verbose = true

function print_jsonstr()
    if verbose then
        print(jsonstr)
    end
end


if __arrow__ then
    print(string.format("\n--------------------------------\nDefault arrow.parms.tojson\n--------------------------------"))
    jsonstr = arrow.parms({}):tojson()
    runner.check(string.len(jsonstr) > 0)
    print_jsonstr()

    print(string.format("\n--------------------------------\nUserSet arrow.parms.tojson\n--------------------------------"))
    jsonstr = arrow.parms({path="/tmp/samples.geoparquet", format="parquet", as_geo=true}):tojson()
    runner.check(string.len(jsonstr) > 0)
    print_jsonstr()
end

if __cre__ then
    print(string.format("\n--------------------------------\nDefault cre.parms.tojson\n--------------------------------"))
    jsonstr = cre.parms({}):tojson()
    runner.check(string.len(jsonstr) > 0)
    print_jsonstr()

    print(string.format("\n--------------------------------\nUserSet cre.parms.tojson\n--------------------------------"))
    jsonstr = cre.parms({image="/tmp/fakeimage.jpg", command="fakecommand"}):tojson()
    js = json.decode(jsonstr)
    runner.check(js.image == "/tmp/fakeimage.jpg", js.image)
    runner.check(js.command == "fakecommand", js.command)
    print_jsonstr()
end

if __geo__ then
    print(string.format("\n--------------------------------\nDefault geo.parms.tojson\n--------------------------------"))
    jsonstr = geo.parms({}):tojson()
    runner.check(string.len(jsonstr) > 0)
    print_jsonstr()

    print(string.format("\n--------------------------------\nUserSet geo.parms.tojson\n--------------------------------"))
    jsonstr = geo.parms({ asset = "arcticdem-mosaic", algorithm = "Bilinear", radius = 0, bands = {"NDVI", "B03"}}):tojson()
    js = json.decode(jsonstr)
    runner.check(js.sampling_algo == "Bilinear", js.sampling_algo)
    runner.check(js.bands_list[1] == "NDVI", js.bands_list[1])
    runner.check(js.bands_list[2] == "B03", js.bands_list[2])
    print_jsonstr()
end

if __netsvc__ then
    print(string.format("\n--------------------------------\nDefault netsvc.parms.tojson\n--------------------------------"))
    jsonstr = netsvc.parms({}):tojson()
    runner.check(string.len(jsonstr) > 0)
    print_jsonstr()

    print(string.format("\n--------------------------------\nUserSet 'polygon' netsvc.parms.tojson\n--------------------------------"))
    jsonstr = netsvc.parms({poly=poly}):tojson()
    js = json.decode(jsonstr)
    runner.check(js.polygon[1].lat == 0.73734824791566, js.polygon[1].lat)
    runner.check(js.polygon[1].lon == 172.51715474926,  js.polygon[1].lon)
    runner.check(js.polygon[2].lat == 0.73734824791566, js.polygon[2].lat)
    runner.check(js.polygon[2].lon == 173.47569646684,  js.polygon[2].lon)
    runner.check(js.polygon[3].lat == 2.1059226076334,  js.polygon[3].lat)
    runner.check(js.polygon[3].lon == 173.47569646684,  js.polygon[3].lon)
    runner.check(js.polygon[4].lat == 2.1059226076334,  js.polygon[4].lat)
    runner.check(js.polygon[4].lon == 172.51715474926,  js.polygon[4].lon)
    runner.check(js.polygon[5].lat == 0.73734824791566, js.polygon[5].lat)
    runner.check(js.polygon[5].lon == 172.51715474926,  js.polygon[5].lon)
    print_jsonstr()

    print(string.format("\n--------------------------------\nUserSet 'raster' netsvc.parms.tojson\n--------------------------------"))
    jsonstr = netsvc.parms({raster={data=vectorfile, cellsize=0.01}}):tojson()
    raster = json.decode(jsonstr).raster
    geojsonFile = json.decode(raster)  -- raster is another json string (geojson file content)
    runner.check(geojsonFile.name == "grand_mesa_poly", geojsonFile.name)
    print_jsonstr()
end

if __swot__ then
    print(string.format("\n--------------------------------\nDefault swot.parms.tojson\n--------------------------------"))
    jsonstr = swot.parms({}):tojson()
    runner.check(string.len(jsonstr) > 0)
    print_jsonstr()

    print(string.format("\n--------------------------------\nUserSet swot.parms.tojson\n--------------------------------"))
    jsonstr = swot.parms({variables={"var1", "var2"}}):tojson()
    js = json.decode(jsonstr)
    runner.check(js.variables[1] == "var1", js.variables[1])
    runner.check(js.variables[2] == "var2", js.variables[2])
    print_jsonstr()
end

if __gedi__ then
    print(string.format("\n--------------------------------\nDefault gedi.parms.tojson\n--------------------------------"))
    jsonstr = gedi.parms({}):tojson()
    runner.check(string.len(jsonstr) > 0)
    print_jsonstr()

    print(string.format("\n--------------------------------\nUserSet gedi.parms.tojson\n--------------------------------"))
    jsonstr = gedi.parms({degrade_flag=1, surface_flag=1, l2_quality_flag=1, l4_quality_flag=1}):tojson()
    js = json.decode(jsonstr)
    runner.check(js.degrade_filter    == 'SET', js.degrade_filter)
    runner.check(js.surface_filter    == 'SET', js.surface_filter)
    runner.check(js.l2_quality_filter == 'SET', js.l2_quality_filter)
    runner.check(js.l4_quality_filter == 'SET', js.l4_quality_filter)
    print_jsonstr()
end

if __icesat2__ then
    print(string.format("\n--------------------------------\nDefault icesat2.parms.tojson\n--------------------------------"))
    jsonstr = icesat2.parms({}):tojson()
    runner.check(string.len(jsonstr) > 0)
    print_jsonstr()

    print(string.format("\n--------------------------------\nUserSet icesat2.parms.tojson\n--------------------------------"))
    jsonstr = icesat2.parms({cnf=4, track=icesat2.RPT_1, atl03_geo_fields={"solar_elevation"}}):tojson()
    js = json.decode(jsonstr)
    runner.check(js.atl03_geo_fields[1]["field"] == "solar_elevation")
    print_jsonstr()
end

-- Report Results --

runner.report()

