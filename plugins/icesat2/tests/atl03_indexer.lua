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

local filelist = { "ATL03_20181014040628_02370109_002_01.h5",
                   "ATL03_20181019065445_03150111_003_01.h5",
                   "ATL03_20200304065203_10470605_003_01.h5" }

local asset_index_file = core.file(core.WRITER, core.TEXT, index_filename)
local writer = core.writer(asset_index_file, "indexq")

local dispatcher = core.dispatcher("recq")
local csvdispatch = core.csv({"name", "t0", "t1", "lat0", "lon0", "lat1", "lon1", "cycle", "rgt"}, "indexq")
dispatcher:attach(csvdispatch, "atl03rec.index")

local recq = msg.subscribe("recq")

local indexer = icesat2.atl03indexer(atl03, filelist, "recq")

local indexrec = recq:recvrecord(3000)
recq:destroy()

runner.check(indexrec, "Failed to read an extent record")

if indexrec then
    local index = indexrec:tabulate()
--    runner.check(t2.seg_id[1] == extentrec:getvalue("seg_id[0]"))
--    runner.check(t2.seg_id[2] == extentrec:getvalue("seg_id[1]"))
--    runner.check(runner.cmpfloat(t2.gps[1], extentrec:getvalue("gps[0]"), 0.0001))
--    runner.check(runner.cmpfloat(t2.gps[2], extentrec:getvalue("gps[1]"), 0.0001))
end

-- Clean Up --

-- Report Results --

runner.report()

