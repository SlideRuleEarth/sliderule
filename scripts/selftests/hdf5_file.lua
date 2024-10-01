local runner = require("test_executive")
local console = require("console")
local td = runner.rootdir(arg[0])

local asset = core.asset("local", "nil", "file", td, "empty.index")

-- Unit Test --

print('\n------------------\nTest01: File\n------------------')

local f1 = h5.file(asset, "h5ex_d_gzip.h5")
local rsps1 = msg.subscribe("h5testq")
f1:read({{dataset="DS1", col=2}}, "h5testq")
local recdata = rsps1:recvrecord(3000)

runner.check(-2 == string.unpack("i", string.char(recdata:getvalue("data[0]"), recdata:getvalue("data[1]"), recdata:getvalue("data[2]"), recdata:getvalue("data[3]"))), "failed to read hdf5 file")
runner.check( 0 == string.unpack("i", string.char(recdata:getvalue("data[4]"), recdata:getvalue("data[5]"), recdata:getvalue("data[6]"), recdata:getvalue("data[7]"))), "failed to read hdf5 file")
runner.check( 2 == string.unpack("i", string.char(recdata:getvalue("data[8]"), recdata:getvalue("data[9]"), recdata:getvalue("data[10]"), recdata:getvalue("data[11]"))), "failed to read hdf5 file")
runner.check( 4 == string.unpack("i", string.char(recdata:getvalue("data[12]"), recdata:getvalue("data[13]"), recdata:getvalue("data[14]"), recdata:getvalue("data[15]"))), "failed to read hdf5 file")


rsps1:destroy()
f1:destroy()

print('\n------------------\nTest02: Read Dataset\n------------------')

local dataq = "dataq"
local rsps2 = msg.subscribe(dataq)

local f2 = h5.dataset(streaming.READER, asset, "h5ex_d_gzip.h5", "/DS1", 0, true, core.INTEGER, 2, 0, core.ALL_ROWS)
local r2 = streaming.reader(f2, dataq)

local vals = rsps2:recvstring(3000)
local e1, e2, e3, e4 = string.unpack('jjjj', vals)

runner.check(-2 == e1, string.format("failed dataset read, expected: %d, actual: %d", -2, e1))
runner.check( 0 == e2, string.format("failed dataset read, expected: %d, actual: %d", 0,  e2))
runner.check( 2 == e3, string.format("failed dataset read, expected: %d, actual: %d", 2,  e3))
runner.check( 4 == e4, string.format("failed dataset read, expected: %d, actual: %d", 4,  e4))

rsps2:destroy()
r2:destroy()

print('\n------------------\nTest03: Read Dataset as Record\n------------------')

local recq = "recq"
local rsps3 = msg.subscribe(recq)

local f3 = h5.dataset(streaming.READER, asset, "h5ex_d_gzip.h5", "/DS1", 5, false, core.INTEGER, 2, 0, core.ALL_ROWS)
local r3 = streaming.reader(f3, recq)

recdata = rsps3:recvrecord(3000)

local rectable = recdata:tabulate()
print("ID:     "..rectable.id)
print("OFFSET: "..rectable.offset)
print("SIZE:   "..rectable.size)

runner.check(rectable.id == 5)

rsps3:destroy()
r3:destroy()

print('\n------------------\nTest04: Read Dataset Raw\n------------------')

local h5_file = "h5ex_d_gzip.bin"
local o4 = streaming.writer(streaming.file(streaming.WRITER, streaming.BINARY, h5_file, streaming.FLUSHED), "h5rawq")
local f4 = h5.dataset(streaming.READER, asset, "h5ex_d_gzip.h5", "/DS1", 0, true, core.DYNAMIC, 2)
local r4 = streaming.reader(f4, "h5rawq")

r4:waiton()
r4:destroy()

o4:waiton()
o4:destroy()

local exp_val = -2 -- starting val in file
local status = true
local f = assert(io.open(h5_file, "rb"))
while status do
    local bytes = f:read(4)
    if not bytes then break end
    local val = string.unpack("<i4", bytes)
    status = runner.check(val == exp_val, string.format("unexpected value, %d != %d", val, exp_val))
    exp_val = exp_val + 2
end

f:close()
os.remove(h5_file)

-- Report Results --

runner.report()

