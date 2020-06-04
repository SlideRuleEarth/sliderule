local runner = require("test_executive")
console = require("console")

-- Setup --

local tmpfile = "dataset.hex"
os.execute("rm "..tmpfile)

console.logger:config(core.INFO)

-- Unit Test --

print('\n------------------\nTest01: Traverse\n------------------')

f1 = icesat2.h5file(nil, core.READER, "/data/ATLAS/ATL03_20200304065203_10470605_003_01.h5")
f1:dir(2, "gt2l")


print('\n------------------\nTest02: Read Dataset\n------------------')

output2 = core.file(core.WRITER, core.ASCII, tmpfile)
writer2 = core.writer(output2, "q2")

h2 = icesat2.h5dataset("gt2l/heights/dist_ph_along", 0)
f2 = icesat2.h5file(h2, core.READER, "/data/ATLAS/ATL03_20200304065203_10470605_003_01.h5")
r2 = core.reader(f2, "q2")

-- Clean Up --

sys.wait(1)

--os.execute("rm "..tmpfile)

-- Report Results --

runner.report()

