-- TODO:
-- replace zip h5 with nc
-- read diff variables + attrs
-- verify can read --> should fail
-- fix issues to read
-- verbose printing/writing to file
-- expect: 3 weeks~ understanding code and parsing

--[[
    from h5datasetdevice.cpp:
    create(<role>, 
            <asset>, 
            <resource>,
            <dataset name>, 
            [<id>], --> ?
            [<raw>], --> ?
            [<datatype>], 
            [col], 
            [startrow], 
            [numrows])
    args:
    - reader <role>
    - asset 
    - file path <resource>
    - variable <datasetname>
    - ? <id>
    - ? <raw>
    - dtype
    - col
    - start row
    - num rows
]]

local runner = require("test_executive")
local console = require("console") -- non trivial printing
local td = runner.rootdir(arg[0]) -- set dir

-- Setup --

-- monitor set; does it have ability to log this? need to attach inspection
-- mul monitors per time
-- looking at log mssg infos, include all and debug: DEBUG, INFO, WARNING, ERROR, CRITICAL
-- asset == driving file, config user/access, set up do-nothing for local
-- global level set to debug, print to screen

console.monitor:config(core.LOG, core.DEBUG) 
sys.setlvl(core.LOG, core.DEBUG)

asset = core.asset("local", "nil", "file", td, "empty.index")

-- Unit Test --

print('\n------------------\nTest: Read Dataset\n------------------')

-- produce: produce stream
-- consume: subscribe to stream
-- one subscribing, get anything coming on from point on

dataq = "dataq" -- stream based: data comes in, then consumed/produced
rsps2 = msg.subscribe(dataq) -- responses posted to dataq

-- Arguments & Call --

local resource_path = "OR_ABI-L2-FDCC-M3_G17_s20182390052191_e20182390054564_c20182390055159.nc"
local dataset_name = "/Power"
local id = 0
local raw = true
local dtype = core.INTEGER
local col = 0
local startrow = 0
local numrows = core.ALL_ROWS

f2 = h5.dataset(core.READER, 
                asset, 
                resource_path, 
                dataset_name, 
                id, 
                raw, 
                dtype, 
                col, 
                startrow, 
                numrows)

-- reader for h5, h5 packages exposes dataset --> reader: needs asset, name, path, variable, row/col
r2 = core.reader(f2, dataq) -- actually reads
-- pass dataset into the stream (post into stream)

vals = rsps2:recvstring(3000) -- read out from listner

print("\n vals recieved with type: ".. type(vals))

e1, e2, e3, e4 = string.unpack('iiii', vals) -- unpack as ints and check

print("\n e1 val: " .. e1)
print("e2 val: " ..e2)
print("e3 val: " ..e3)
print("e4 val: " ..e4)

-- runner.check(-2 == e1, "failed dataset read")
-- runner.check( 0 == e2, "failed dataset read")
-- runner.check( 2 == e3, "failed dataset read")
-- runner.check( 4 == e4, "failed dataset read") 

rsps2:destroy()
r2:destroy()

-- Report Results --

runner.report()

