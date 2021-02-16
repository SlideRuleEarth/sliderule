local runner = require("test_executive")
local console = require("console")

-- Setup --

console.logger:config(core.INFO)

-- Unit Test --

print('\n------------------\nTest04: Read Dataset Raw\n------------------')

test_file = "test.bin"
o4 = core.writer(core.file(core.WRITER, core.BINARY, test_file, core.FLUSHED), "h5rawq")
f4 = h5.dataset(core.READER, "file:///data/ATLAS/ATL03_20200304065203_10470605_003_01.h5", "/orbit_info/sc_orient", 0, true)
r4 = core.reader(f4, "h5rawq")

r4:waiton()
r4:destroy()

o4:waiton()
o4:destroy()

local status = true
local f = assert(io.open(test_file, "rb"))
while status do
    local bytes = f:read(4)
    if not bytes then break end
    local value = string.unpack("<i1", bytes)
end

f:close()
os.remove(test_file)

-- Report Results --

runner.report()

