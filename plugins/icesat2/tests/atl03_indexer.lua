local runner = require("test_executive")
console = require("console")
asset = require("asset")

-- Setup --

local td = runner.rootdir(arg[0]) .. "../tests"

console.logger:config(core.INFO)

assets = asset.loaddir(td.."/asset_directory.csv")


-- Unit Test --

print('\n------------------\nTest01: Atl03 Indexer \n------------------')

local atl03 = core.asset("atl03-local")
local name, format, url, index_filename, status = atl03:info()
runner.check(status)

local filelist = { "ATL03_20181019065445_03150111_003_01.h5",
                   "ATL03_20200304065203_10470605_003_01.h5" }

local asset_index_file = core.file(core.WRITER, core.TEXT, index_filename)
local writer = core.writer(asset_index_file, "indexq")

local dispatcher = core.dispatcher("recq")
local csvdispatch = core.csv({"name", "t0", "t1", "lat0", "lon0", "lat1", "lon1", "cycle", "rgt"}, "indexq")
dispatcher:attach(csvdispatch, "atl03rec.index")
dispatcher:run()

local recq = msg.subscribe("recq")

local indexer = icesat2.atl03indexer(atl03, filelist, "recq", 1)

print("--------------------------")
for r=1,2 do
    local indexrec = recq:recvrecord(3000)
    runner.check(indexrec, "Failed to read an extent record")
    if indexrec then
        local index = indexrec:tabulate()
        print(filelist[r])
        print("t0: "..index.t0)
        print("t1: "..index.t1)
        print("lat0: "..index.lat0)
        print("lon0: "..index.lon0)
        print("lat1: "..index.lat1)
        print("lon1: "..index.lon1)
        print("cycle: "..index.cycle)
        print("rgt: "..index.rgt)
        print("--------------------------")
        --    runner.check(t2.seg_id[1] == extentrec:getvalue("seg_id[0]"))
    --    runner.check(t2.seg_id[2] == extentrec:getvalue("seg_id[1]"))
    --    runner.check(runner.cmpfloat(t2.gps[1], extentrec:getvalue("gps[0]"), 0.0001))
    --    runner.check(runner.cmpfloat(t2.gps[2], extentrec:getvalue("gps[1]"), 0.0001))
    end
end

-- Clean Up --

recq:destroy()

-- Report Results --

runner.report()

