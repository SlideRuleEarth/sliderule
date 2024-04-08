-- H5CORO DENSE TEST 1 -- Temp Variable: add_offset, scale_factor -- 

local runner = require("test_executive")
local console = require("console") -- non trivial printing
local td = runner.rootdir(arg[0]) -- set dir

-- Setup --

console.monitor:config(core.LOG, core.DEBUG) 
sys.setlvl(core.LOG, core.DEBUG)
asset = core.asset("local", "nil", "file", td, "empty.index")

-- Unit Test --
print('\n------------------\nTest: Read Dataset\n------------------')

dataq = "dataq" -- stream based: data comes in, then consumed/produced
rsps2 = msg.subscribe(dataq) -- responses posted to dataq

-- Arguments & Call --

-- SUB TEST 1 - add_offset -- 

local resource_path = "OR_ABI-L2-FDCC-M3_G17_s20182390052191_e20182390054564_c20182390055159.nc"
local dataset_name = "/Temp/add_offset"
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

r2 = core.reader(f2, dataq)
vals = rsps2:recvstring(3000) -- read out from listner
e1 = string.unpack('I4', vals)
print("RESULT add_offset: " .. e1)
runner.check(400 == e1, "failed dataset read")

rsps2:destroy()
r2:destroy()

-- SUB TEST 2 - scale_factor -- 

asset2 = core.asset("local", "nil", "file", td, "empty.index")
dataq = "dataq2" -- stream based: data comes in, then consumed/produced
rsps2s2 = msg.subscribe(dataq) -- responses posted to dataq

dataset_name = "/Temp/scale_factor"
dtype = core.DYNAMIC

f3 = h5.dataset(core.READER, 
                asset2, 
                resource_path, 
                dataset_name, 
                id, 
                raw, 
                dtype, 
                col, 
                startrow, 
                numrows)

r3 = core.reader(f3, dataq)
vals = rsps2s2:recvstring(3000) -- read out from listner
e1 = string.unpack('f', vals)
print("RESULT scale_factor: " .. e1)
runner.check("0.054936669766903" == tostring(e1), "failed dataset read")

rsps2s2:destroy()
r3:destroy()

-- SUB TEST 3 - valid_range -- 

asset3 = core.asset("local", "nil", "file", td, "empty.index")
dataq = "dataq3" -- stream based: data comes in, then consumed/produced
rsps2s3 = msg.subscribe(dataq) -- responses posted to dataq

dataset_name = "/Temp/valid_range"
dtype = core.INTEGER
-- col = 1

f3 = h5.dataset(core.READER, 
                asset3, 
                resource_path, 
                dataset_name, 
                id, 
                raw, 
                dtype, 
                col, 
                startrow, 
                numrows)

r4 = core.reader(f3, dataq)
vals = rsps2s3:recvstring(3000)
e1, e2, e3 = string.unpack('i2i2i2', vals)
print("RESULT v1: " .. e1)
print("RESULT v2: " .. e2)
print("RESULT v2: " .. e3)
runner.check(0 == e1, "failed dataset read")
runner.check(-6 == e3, "failed dataset read")

rsps2s3:destroy()
r4:destroy()

-- END --

-- Report Results --

runner.report()