local runner = require("test_executive")
local srcfile, dirpath = runner.srcscript()
local h5_input_file = "../data/h5ex_d_gzip.h5"

-- Setup --

local asset = core.asset("local", "nil", "file", dirpath, "empty.index")

-- Self Test --

print('\n------------------\nTest01: File\n------------------')

local f1 = h5.file(asset, h5_input_file)
local rsps1 = msg.subscribe("h5testq")
f1:read({{dataset="DS1", col=2}}, "h5testq")
local recdata = rsps1:recvrecord(3000)

runner.assert(-2 == string.unpack("i", string.char(recdata:getvalue("data[0]"), recdata:getvalue("data[1]"), recdata:getvalue("data[2]"), recdata:getvalue("data[3]"))), "failed to read hdf5 file")
runner.assert( 0 == string.unpack("i", string.char(recdata:getvalue("data[4]"), recdata:getvalue("data[5]"), recdata:getvalue("data[6]"), recdata:getvalue("data[7]"))), "failed to read hdf5 file")
runner.assert( 2 == string.unpack("i", string.char(recdata:getvalue("data[8]"), recdata:getvalue("data[9]"), recdata:getvalue("data[10]"), recdata:getvalue("data[11]"))), "failed to read hdf5 file")
runner.assert( 4 == string.unpack("i", string.char(recdata:getvalue("data[12]"), recdata:getvalue("data[13]"), recdata:getvalue("data[14]"), recdata:getvalue("data[15]"))), "failed to read hdf5 file")

rsps1:destroy()
f1:destroy()

print('\n------------------\nTest02: Read Dataset\n------------------')

local dataq = "dataq"
local rsps2 = msg.subscribe(dataq)

local f2 = h5.dataset(streaming.READER, asset, h5_input_file, "/DS1", 0, true, core.INTEGER, 2, 0, core.ALL_ROWS)
local r2 = streaming.reader(f2, dataq)

local vals = rsps2:recvstring(3000)
local e1, e2, e3, e4 = string.unpack('jjjj', vals)

runner.assert(-2 == e1, string.format("failed dataset read, expected: %d, actual: %d", -2, e1))
runner.assert( 0 == e2, string.format("failed dataset read, expected: %d, actual: %d", 0,  e2))
runner.assert( 2 == e3, string.format("failed dataset read, expected: %d, actual: %d", 2,  e3))
runner.assert( 4 == e4, string.format("failed dataset read, expected: %d, actual: %d", 4,  e4))

rsps2:destroy()
r2:destroy()

print('\n------------------\nTest03: Read Dataset as Record\n------------------')

local recq = "recq"
local rsps3 = msg.subscribe(recq)

local f3 = h5.dataset(streaming.READER, asset, h5_input_file, "/DS1", 5, false, core.INTEGER, 2, 0, core.ALL_ROWS)
local r3 = streaming.reader(f3, recq)

recdata = rsps3:recvrecord(3000)

local rectable = recdata:tabulate()
print("ID:     "..rectable.id)
print("OFFSET: "..rectable.offset)
print("SIZE:   "..rectable.size)

runner.assert(rectable.id == 5)

rsps3:destroy()
r3:destroy()

print('\n------------------\nTest04: Read Dataset Raw\n------------------')

local h5_file = "h5ex_d_gzip.bin"
local o4 = streaming.writer(streaming.file(streaming.WRITER, streaming.BINARY, h5_file, streaming.FLUSHED), "h5rawq")
local f4 = h5.dataset(streaming.READER, asset, h5_input_file, "/DS1", 0, true, core.DYNAMIC, 2)
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
    status = runner.assert(val == exp_val, string.format("unexpected value, %d != %d", val, exp_val))
    exp_val = exp_val + 2
end

f:close()
os.remove(h5_file)

print('\n------------------\nTest05: Slice Column via 2D Slice (1D result)\n------------------')

local f5 = h5.file(asset, h5_input_file)
local rsps5 = msg.subscribe("h5slice1q")
-- Use a 2D slice to pull a single column (index 2) for rows 0..3 -> expected [-2, 0, 2, 4]
local expected1d = {-2, 0, 2, 4}
f5:read({{dataset="DS1", slice={{0,4}, {2,3}}}}, "h5slice1q")
recdata = rsps5:recvrecord(3000)
runner.assert(recdata ~= nil, "slice read did not return data")
for i = 0, (#expected1d - 1) do
    local offset = i * 4
    local val = string.unpack("i", string.char(recdata:getvalue("data["..offset.."]"),
                                              recdata:getvalue("data["..(offset+1).."]"),
                                              recdata:getvalue("data["..(offset+2).."]"),
                                              recdata:getvalue("data["..(offset+3).."]")))
    runner.assert(val == expected1d[i + 1], string.format("slice mismatch idx %d: %d != %d", i, val, expected1d[i + 1]))
end
rsps5:destroy()
f5:destroy()

print('\n------------------\nTest06: Slice 2D Block\n------------------')

local f6 = h5.file(asset, h5_input_file)
local rsps6 = msg.subscribe("h5slice2q")
-- 3x3 block from rows 1..3, cols 1..3 (end exclusive)
local expected2d = {0, 0, 0,
                    1, 2, 3,
                    2, 4, 6}
f6:read({{dataset="DS1", slice={{1,4}, {1,4}}}}, "h5slice2q")
recdata = rsps6:recvrecord(3000)
runner.assert(recdata ~= nil, "slice 2d read did not return data")
for i = 0, (#expected2d - 1) do
    local offset = i * 4
    local val = string.unpack("i", string.char(recdata:getvalue("data["..offset.."]"),
                                              recdata:getvalue("data["..(offset+1).."]"),
                                              recdata:getvalue("data["..(offset+2).."]"),
                                              recdata:getvalue("data["..(offset+3).."]")))
    runner.assert(val == expected2d[i + 1], string.format("slice 2d mismatch idx %d: %d != %d", i, val, expected2d[i + 1]))
end
rsps6:destroy()
f6:destroy()

print('\n------------------\nTest07: Slice All Via Slice Semantics (1D)\n------------------')
-- Use a slice that mirrors the default column read (col 0, all rows)
local f7a = h5.file(asset, h5_input_file)
local rsps7a_full = msg.subscribe("h5slice5q_full")
f7a:read({{dataset="DS1"}}, "h5slice5q_full")
local full_rec = rsps7a_full:recvrecord(3000)
rsps7a_full:destroy()

local rsps7a_slice = msg.subscribe("h5slice5q_slice")
-- slice dims: rows 0..end, col 0..1 (single column)
f7a:read({{dataset="DS1", slice={{0, h5.ALL_ROWS}, {0, 1}}}}, "h5slice5q_slice")
local slice_rec = rsps7a_slice:recvrecord(3000)
rsps7a_slice:destroy()

runner.assert(full_rec ~= nil and slice_rec ~= nil, "full or slice record missing")
local full_size = full_rec:getvalue("size")
local slice_size = slice_rec:getvalue("size")
runner.assert(full_size == slice_size, "full and slice sizes differ")
for i = 0, full_size - 1 do
    runner.assert(full_rec:getvalue("data["..i.."]") == slice_rec:getvalue("data["..i.."]"), "full/slice byte mismatch at index "..i)
end
f7a:destroy()

print('\n------------------\nTest08: Slice All Via Slice Semantics (2D)\n------------------')

-- Full 2D slice: check size and a small known block
local f8 = h5.file(asset, h5_input_file)
local rsps8 = msg.subscribe("h5slice2d")
f8:read({{dataset="DS1", slice={{0, h5.ALL_ROWS}, {0, h5.ALL_ROWS}}}}, "h5slice2d")
local rec2d = rsps8:recvrecord(3000)
rsps8:destroy()
runner.assert(rec2d ~= nil, "2D full slice read failed")

local expected_total_bytes = 32 * 64 * 4 -- rows * cols * int32
runner.assert(rec2d:getvalue("size") == expected_total_bytes, "unexpected size for 2D full slice")

-- Spot-check top-left 3x3 block within the flattened buffer
local expected2d_full = {0, 0, 0,
                         1, 2, 3,
                         2, 4, 6}
local function get_int(rec, row, col)
    local offset = (row * 64 + col) * 4
    return string.unpack("i", string.char(rec:getvalue("data["..offset.."]"),
                                          rec:getvalue("data["..(offset+1).."]"),
                                          rec:getvalue("data["..(offset+2).."]"),
                                          rec:getvalue("data["..(offset+3).."]")))
end

local idx = 1
for r = 1, 3 do
    for c = 1, 3 do
        local val = get_int(rec2d, r, c)
        runner.assert(val == expected2d_full[idx], string.format("2D full slice mismatch at (%d,%d): %d != %d", r, c, val, expected2d_full[idx]))
        idx = idx + 1
    end
end
f8:destroy()

print('\n------------------\nTest09: Slice From Row 2 to End (1D)\n------------------')
local f9 = h5.file(asset, h5_input_file)
local rsps9_full = msg.subscribe("h5slice6q_full")
f9:read({{dataset="DS1"}}, "h5slice6q_full")
local full_rec2 = rsps9_full:recvrecord(3000)
rsps9_full:destroy()
runner.assert(full_rec2 ~= nil, "full read failed for row-2 slice comparison")

local rsps9_slice = msg.subscribe("h5slice6q_slice")
-- start at row 2, all columns
f9:read({{dataset="DS1", slice={{2, h5.ALL_ROWS}, {0, 1}}}}, "h5slice6q_slice")
local slice_rec2 = rsps9_slice:recvrecord(3000)
rsps9_slice:destroy()
runner.assert(slice_rec2 ~= nil, "slice read failed for row-2 slice comparison")

-- Compare a prefix of the full buffer offset by row 2
local elements_per_row = 1 -- single column slice
local bytes_per_row = elements_per_row * 4 -- int32
local expected_size = (32 - 2) * bytes_per_row
runner.assert(slice_rec2:getvalue("size") == expected_size, "unexpected slice size")
for i = 0, expected_size - 1 do
    local full_idx = (2 * bytes_per_row) + i
    runner.assert(slice_rec2:getvalue("data["..i.."]") == full_rec2:getvalue("data["..full_idx.."]"), "row-2 slice mismatch at byte "..i)
end
f9:destroy()

print('\n------------------\nTest10: Slice From Row 2 to End (2D)\n------------------')
-- Take rows 2..end, all columns; check size and leading values
local f10a = h5.file(asset, h5_input_file)
local rsps10a = msg.subscribe("h5slice2d_tail_slice")
f10a:read({{dataset="DS1", slice={{2, h5.ALL_ROWS}, {0, h5.ALL_ROWS}}}}, "h5slice2d_tail_slice")
local slice_rec3 = rsps10a:recvrecord(3000)
rsps10a:destroy()
runner.assert(slice_rec3 ~= nil, "2d tail slice read failed")

local bytes_per_row_2d = 64 * 4 -- full row width * int32
local expected_size_2d = (32 - 2) * bytes_per_row_2d
runner.assert(slice_rec3:getvalue("size") == expected_size_2d, "unexpected 2d tail slice size")

-- First four ints should be row 2 columns 0..3: 0,1,2,3
local function get_int_from_slice(rec, offset_bytes)
    return string.unpack("i", string.char(rec:getvalue("data["..offset_bytes.."]"),
                                          rec:getvalue("data["..(offset_bytes+1).."]"),
                                          rec:getvalue("data["..(offset_bytes+2).."]"),
                                          rec:getvalue("data["..(offset_bytes+3).."]")))
end
local tail_expected = {0, 1, 2, 3}
for i = 0, (#tail_expected - 1) do
    local offset = i * 4
    local val = get_int_from_slice(slice_rec3, offset)
    runner.assert(val == tail_expected[i + 1], string.format("2d tail slice mismatch at element %d: %d != %d", i, val, tail_expected[i + 1]))
end
f10a:destroy()

print('\n------------------\nTest11: Slice Out of Bounds (Negative)\n------------------')

local f10 = h5.file(asset, h5_input_file)
local rsps10 = msg.subscribe("h5slice3q")
-- request rows well beyond dataset dims to force failure
f10:read({{dataset="DS1", slice={{1000,1010}, {0,1}}}}, "h5slice3q")
recdata = rsps10:recvrecord(1000)
runner.assert(recdata == nil, "expected no data for out-of-bounds slice")
rsps10:destroy()
f10:destroy()

print('\n------------------\nTest12: Slice Start Greater Than End (Negative)\n------------------')

local f11n = h5.file(asset, h5_input_file)
local rsps11n = msg.subscribe("h5slice4q")
-- invalid slice where start > end
f11n:read({{dataset="DS1", slice={{5,1}, {0,1}}}}, "h5slice4q")
recdata = rsps11n:recvrecord(1000)
runner.assert(recdata == nil, "expected no data for invalid slice with start > end")
rsps11n:destroy()
f11n:destroy()

print('\n------------------\nTest13: Slice With Extra Dimension (Truncates)\n------------------')
-- Providing a third dimension to a 2D dataset should ignore the extra dim and still return data
local f12 = h5.file(asset, h5_input_file)
local rsps12 = msg.subscribe("h5slice_toomany_dims")
f12:read({{dataset="DS1", slice={{0,1}, {0,1}, {0,1}}}}, "h5slice_toomany_dims")
recdata = rsps12:recvrecord(1000)
runner.assert(recdata ~= nil, "unexpectedly failed to read with extra dimension")
-- expect a single element (row 0, col 0)
local val_extra = string.unpack("i", string.char(recdata:getvalue("data[0]"),
                                                 recdata:getvalue("data[1]"),
                                                 recdata:getvalue("data[2]"),
                                                 recdata:getvalue("data[3]")))
runner.assert(val_extra == 0, string.format("extra-dim slice returned unexpected value: %d", val_extra))
rsps12:destroy()
f12:destroy()

print('\n------------------\nTest14: Slice 3D (Not Applicable)\n------------------')
-- DS1 in h5ex_d_gzip.h5 is 2D; no 3D dataset available in this fixture.
-- Keep placeholder to document lack of 3D coverage here.
runner.assert(true, "skipping 3D slice test (fixture is 2D)")


-- Clean Up --

-- Report Results --

runner.report()

