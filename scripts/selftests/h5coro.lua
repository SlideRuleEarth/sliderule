local runner = require("test_executive")
local console = require("console")
local td = runner.rootdir(arg[0])

-- Setup --

console.monitor:config(core.LOG, core.DEBUG)
sys.setlvl(core.LOG, core.DEBUG)

asset = core.asset("local", "nil", "file", td, "empty.index")

-- Unit Test --

print('\n------------------\nTest: Read Dataset\n------------------')

dataq = "dataq"
rsps2 = msg.subscribe(dataq)

f2 = h5.dataset(core.READER, asset, "h5ex_d_gzip.h5", "/DS1", 0, true, core.INTEGER, 2, 0, core.ALL_ROWS)
r2 = core.reader(f2, dataq)

vals = rsps2:recvstring(3000)
e1, e2, e3, e4 = string.unpack('iiii', vals)

runner.check(-2 == e1, "failed dataset read")
runner.check( 0 == e2, "failed dataset read")
runner.check( 2 == e3, "failed dataset read")
runner.check( 4 == e4, "failed dataset read")

rsps2:destroy()
r2:destroy()

-- Report Results --

runner.report()

