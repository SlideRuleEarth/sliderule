local runner = require("test_executive")
console = require("console")

-- Setup --

console.logger:config(core.INFO)

-- Unit Test --

print('\n------------------\nTest01: Traverse\n------------------')

f1 = h5.file("/data/ATLAS/ATL03_20200304065203_10470605_003_01.h5")
runner.check(f1:dir(2, "gt2l"), "failed to traverse hdf5 file")


print('\n------------------\nTest02: Read Dataset\n------------------')

dataq = "dataq"
rsps2 = msg.subscribe(dataq)

f2 = h5.dataset(core.READER, "/data/ATLAS/ATL03_20200304065203_10470605_003_01.h5", "ancillary_data/atlas_sdp_gps_epoch")
r2 = core.reader(f2, dataq)

vals = rsps2:recvstring(3000)
epoch = string.unpack('d', vals)

runner.check(epoch == 1198800018.0, "failed to read correct epoch")

rsps2:destroy()
r2:destroy()


print('\n------------------\nTest03: Read Dataset as Record\n------------------')

recq = "recq"
rsps3 = msg.subscribe(recq)

f3 = h5.dataset(core.READER, "/data/ATLAS/ATL03_20200304065203_10470605_003_01.h5", "gt2l/heights/dist_ph_along", 5, false)
r3 = core.reader(f3, recq)

recdata = rsps3:recvrecord(3000)

rectable = recdata:tabulate()
print("ID:     "..rectable.ID)
print("OFFSET: "..rectable.OFFSET)
print("SIZE:   "..rectable.SIZE)

runner.check(rectable.ID == 5)

rsps3:destroy()
r3:destroy()

-- Clean Up --

-- Report Results --

runner.report()

