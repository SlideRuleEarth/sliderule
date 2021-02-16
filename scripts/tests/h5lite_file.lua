local runner = require("test_executive")
local console = require("console")

-- Setup --

console.logger:config(core.INFO)

-- Unit Test --

print('\n------------------\nTest04: Read Dataset Raw\n------------------')

segment_file = "segment.bin"
o4 = core.writer(core.file(core.WRITER, core.BINARY, segment_file, core.FLUSHED), "h5rawq")
f4 = h5.dataset(core.READER, "file:///data/ATLAS/ATL03_20200304065203_10470605_003_01.h5", "gt1l/geolocation/segment_ph_cnt", 0, true)
r4 = core.reader(f4, "h5rawq")

r4:waiton()
r4:destroy()

o4:waiton()
o4:destroy()

--local expected_segment_id = 555764 -- starting segment id in file
local status = true
local f = assert(io.open(segment_file, "rb"))
while status do
    local bytes = f:read(4)
    if not bytes then break end
    local segment_id = string.unpack("<i4", bytes)
--    status = runner.check(segment_id == expected_segment_id, string.format("unexpected segment id, %d != %d", segment_id, expected_segment_id))
--    expected_segment_id = expected_segment_id + 1
end

f:close()
--os.remove(segment_file)

-- Report Results --

runner.report()

