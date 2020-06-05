local runner = require("test_executive")
console = require("console")

-- Setup --

console.logger:config(core.INFO)

-- Unit Test --

print('\n------------------\nTest01: Traverse\n------------------')

f1 = icesat2.h5file(nil, core.READER, "/data/ATLAS/ATL03_20200304065203_10470605_003_01.h5")
runner.check(f1:dir(2, "gt2l"), "failed to traverse hdf5 file")


print('\n------------------\nTest02: Read Dataset\n------------------')

--output2 = core.file(core.WRITER, core.ASCII, tmpfile)
--writer2 = core.writer(output2, "q2")

dataq = "dataq"
rsps = msg.subscribe(dataq)

h2 = icesat2.h5dataset("ancillary_data/atlas_sdp_gps_epoch", 0)
f2 = icesat2.h5file(h2, core.READER, "/data/ATLAS/ATL03_20200304065203_10470605_003_01.h5")
r2 = core.reader(f2, dataq)

vals = rsps:recvstring(3000)
epoch = string.unpack('d', vals)

runner.check(epoch == 1198800018.0, "failed to read correct epoch")

-- Clean Up --

-- Report Results --

runner.report()

