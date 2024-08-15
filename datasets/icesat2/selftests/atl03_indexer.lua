local runner = require("test_executive")
-- local console = require("console")
local asset = require("asset")
local csv = require("csv")
local json = require("json")

-- Setup --

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local assets = asset.loaddir()
local asset_name = "icesat2"
local nsidc_s3 = core.getbyname(asset_name)
local name, identity, driver = nsidc_s3:info()

local creds = aws.csget(identity)
if not creds then
    local earthdata_url = "https://data.nsidc.earthdatacloud.nasa.gov/s3credentials"
    local response, _ = netsvc.get(earthdata_url)
    local _, credential = pcall(json.decode, response)
    aws.csput(identity, credential)
end

-- Unit Test --

print('\n------------------\nTest01: Atl03 Indexer \n------------------')

local filelist = { "ATL03_20181019065445_03150111_005_01.h5",
                   "ATL03_20200304065203_10470605_005_01.h5" }

-- get index filename
local name, identity, driver, path, index_filename, region, endpoint, status = nsidc_s3:info()
runner.check(status)

-- setup index file writer
local asset_index_file = core.file(core.WRITER, core.TEXT, index_filename)
local writer = core.writer(asset_index_file, "indexq"):name("writer")

-- setup csv dispatch
local indexrecq = msg.subscribe("indexrecq")
local dispatcher = core.dispatcher("indexrecq"):name("dispatcher")
local csvdispatch = core.csv({"name", "t0", "t1", "lat0", "lon0", "lat1", "lon1", "cycle", "rgt"}, "indexq"):name("csvdispatch")
dispatcher:attach(csvdispatch, "atl03rec.index")
dispatcher:run()

-- create and run indexer
local indexer = icesat2.atl03indexer(nsidc_s3, filelist, "indexrecq", 1):name("indexer")

-- read in index list
local indexlist = {}
print("--------------------------")
for r=1,2 do
    local indexrec = indexrecq:recvrecord(3000)
    runner.check(indexrec, "Failed to read an extent record")
    if indexrec then
        local index = indexrec:tabulate()
        indexlist[r] = index
        print(index.name)
        print("t0: "..index.t0)
        print("t1: "..index.t1)
        print("lat0: "..index.lat0)
        print("lon0: "..index.lon0)
        print("lat1: "..index.lat1)
        print("lon1: "..index.lon1)
        print("cycle: "..index.cycle)
        print("rgt: "..index.rgt)
        print("--------------------------")
    end
end

-- close index file --
sys.wait(1)
asset_index_file:close()

-- check records against index file
local i = 1
local raw_index = csv.open(index_filename)
for _,line in ipairs(raw_index) do
    runner.check(indexlist[i]["name"] == line["name"])
    runner.check(runner.cmpfloat(indexlist[i]["t0"], line["t0"], 0.0001))
    runner.check(runner.cmpfloat(indexlist[i]["t1"], line["t1"], 0.0001))
    runner.check(runner.cmpfloat(indexlist[i]["lat0"], line["lat0"], 0.0001))
    runner.check(runner.cmpfloat(indexlist[i]["lon0"], line["lon0"], 0.0001))
    runner.check(runner.cmpfloat(indexlist[i]["lat1"], line["lat1"], 0.0001))
    runner.check(runner.cmpfloat(indexlist[i]["lon1"], line["lon1"], 0.0001))
    runner.check(runner.cmpfloat(indexlist[i]["cycle"], line["cycle"], 0.0001))
    runner.check(runner.cmpfloat(indexlist[i]["rgt"], line["rgt"], 0.0001))
    i = i + 1
end

-- Clean Up --

indexrecq:destroy()
os.remove(index_filename)
writer:destroy()
csvdispatch:destroy()
dispatcher:destroy()
indexer:destroy()

-- Report Results --

runner.report()

