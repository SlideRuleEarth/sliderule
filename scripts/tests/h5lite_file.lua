local runner = require("test_executive")
local console = require("console")

-- Setup --

console.logger:config(core.INFO)

-- Unit Test --

print('\n------------------\nTest01: Traverse\n------------------')

--f1 = h5.file("file:///data/ATLAS/ATL03_20200304065203_10470605_003_01.h5")
--runner.check(f1:dir(2, "gt2l/heights/delta_time"), "failed to traverse h5lite file")

print('\n------------------\nTest02: Read\n------------------')

--[[
recq = "recq"
rsps2 = msg.subscribe(recq)

f2 = h5.dataset(core.READER, "file:///data/ATLAS/ATL03_20200304065203_10470605_003_01.h5", "gt2l/heights/delta_time", 3, false)
r2 = core.reader(f2, recq)

recdata = rsps2:recvrecord(3000)

rectable = recdata:tabulate()
print("ID:     "..rectable.id)
print("OFFSET: "..rectable.offset)
print("SIZE:   "..rectable.size)

runner.check(rectable.id == 3)

rsps2:destroy()
r2:destroy()
--]]

dataq = "dataq"
rsps2 = msg.subscribe(dataq)

f2 = h5.dataset(core.READER, "file:///data/ATLAS/ATL03_20200304065203_10470605_003_01.h5", "ancillary_data/atlas_sdp_gps_epoch")
r2 = core.reader(f2, dataq)

vals = rsps2:recvstring(3000)
epoch = string.unpack('d', vals)

runner.check(epoch == 1198800018.0, "failed to read correct epoch")

rsps2:destroy()
r2:destroy()

-- Clean Up --

-- Report Results --

sys.wait(1)

runner.report()

