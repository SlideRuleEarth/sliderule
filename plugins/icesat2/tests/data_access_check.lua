local runner = require("test_executive")
local console = require("console")
local filename = arg[1] or "file:///data/ATLAS/ATL03_20181019065445_03150111_003_01.h5"

-- Setup --

print(filename)
console.logger:config(core.INFO)
local recq = msg.subscribe("recq")

-- Test --

local atl03_reader = icesat2.atl03(filename, "recq", {}, icesat2.RPT_1)

local extent_rec = recq:recvrecord(3000)
runner.check(extent_rec, "Failed to read an extent record")

if extent_rec then
    local extent_tbl = extent_rec:tabulate()
    print(extent_tbl.segment_id[1], extent_tbl.segment_id[2])
    print(extent_tbl.delta_time[1], extent_tbl.delta_time[2])
end

-- Clean Up --

recq:destroy()

-- Report Results --

runner.report()

