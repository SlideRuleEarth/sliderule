local runner = require("test_executive")
local console = require("console") -- non trivial printing
local td = runner.rootdir(arg[0]) -- set dir

-- Setup --

console.monitor:config(core.LOG, core.DEBUG) -- looking at log mssg infos, include all and debug: DEBUG, INFO, WARNING, ERROR, CRITICAL --
-- monitor set; does it have ability to log this? need to attach inspection
-- mul monitors per time
sys.setlvl(core.LOG, core.DEBUG) -- global level set to debug, print to screen

asset = core.asset("local", "nil", "file", td, "empty.index")
-- asset == driving file, config user/access, set up do-nothing for local

-- Unit Test --

print('\n------------------\nTest: Read Dataset\n------------------')

dataq = "dataq" -- stream based: data comes in, then consumed/produced
-- produce: produce stream
-- consume: subscribe to stream
-- one subscribing, get anything coming on from point on
rsps2 = msg.subscribe(dataq) -- responses posted to dataq

-- TODO:
-- replace zip h5 with nc
-- read diff variables + attrs
-- verify can read --> should fail
-- fix issues to read
-- verbose printing/writing to file
-- expect: 3 weeks~ understanding code and parsing

f2 = h5.dataset(core.READER, asset, "h5ex_d_gzip.h5", "/DS1", 0, true, core.INTEGER, 2, 0, core.ALL_ROWS)
-- reader for h5, h5 packages exposes dataset --> reader: needs asset, name, path, variable, row/col
-- still nothing...
r2 = core.reader(f2, dataq) -- actually reads
-- pass dataset into the stream (post into stream)

vals = rsps2:recvstring(3000) -- read out from listner
e1, e2, e3, e4 = string.unpack('iiii', vals) -- unpack as ints and check

runner.check(-2 == e1, "failed dataset read")
runner.check( 0 == e2, "failed dataset read")
runner.check( 2 == e3, "failed dataset read")
runner.check( 4 == e4, "failed dataset read")

rsps2:destroy()
r2:destroy()

-- Report Results --

runner.report()

