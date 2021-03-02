local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")

-- Setup --

local info = debug.getinfo(1,'S');
local td = info.source:sub(2, string.find(info.source, "/[^/]*$"))

console.logger:config(core.INFO)

assets = asset.loaddir(td.."/asset_directory.csv")


-- Unit Test --

print('\n------------------\nTest01: Atl03 Indexer \n------------------')

local filelist = { "ATL03_20181019065445_03150111_003_01.h5",
                   "ATL03_20200304065203_10470605_003_01.h5" }

-- load asset 
local atl03 = core.getbyname("atlas-local")
local name, format, url, index_filename, status = atl03:info()
runner.check(status)

-- setup index file writer
local asset_index_file = core.file(core.WRITER, core.TEXT, index_filename)
local writer = core.writer(asset_index_file, "indexq")

-- setup csv dispatch
local indexrecq = msg.subscribe("indexrecq")
local dispatcher = core.dispatcher("indexrecq")
local csvdispatch = core.csv({"name", "t0", "t1", "lat0", "lon0", "lat1", "lon1", "cycle", "rgt"}, "indexq")
dispatcher:attach(csvdispatch, "atl03rec.index")
dispatcher:run()

-- create and run indexer
local indexer = icesat2.atl03indexer(atl03, filelist, "indexrecq", 1)

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
local raw_index = csv.open(index_filename, {header=true})
for fields in raw_index:lines() do    
    runner.check(indexlist[i]["name"] == fields["name"])
    runner.check(runner.cmpfloat(indexlist[i]["t0"], fields["t0"], 0.0001))
    runner.check(runner.cmpfloat(indexlist[i]["t1"], fields["t1"], 0.0001))
    runner.check(runner.cmpfloat(indexlist[i]["lat0"], fields["lat0"], 0.0001))
    runner.check(runner.cmpfloat(indexlist[i]["lon0"], fields["lon0"], 0.0001))
    runner.check(runner.cmpfloat(indexlist[i]["lat1"], fields["lat1"], 0.0001))
    runner.check(runner.cmpfloat(indexlist[i]["lon1"], fields["lon1"], 0.0001))
    runner.check(runner.cmpfloat(indexlist[i]["cycle"], fields["cycle"], 0.0001))
    runner.check(runner.cmpfloat(indexlist[i]["rgt"], fields["rgt"], 0.0001))
    i = i + 1
end

-- Clean Up --

indexrecq:destroy()
os.remove(index_filename)

-- Report Results --

runner.report()

