local console = require("console")

local resource = arg[1] or "/data/ATL03/ATL03_20181017222812_02950102_003_01.h5"
local dataset = arg[2] or "/gt2l/heights/h_ph"
local col = tonumber(arg[3]) or 0
local startrow = tonumber(arg[4]) or 13665185
local numrows = tonumber(arg[5]) or 89624
local dataset_file = arg[6] or dataset:sub(string.find(dataset, "/[^/]*$") + 1)

local file_writer = core.writer(core.file(core.WRITER, core.BINARY, dataset_file..".bin", core.FLUSHED), "h5rawq")
local file_reader = core.reader(h5.dataset(core.READER, "file://"..resource, dataset, 0, true, core.DYNAMIC, col, startrow, numrows), "h5rawq")

file_reader:waiton()
file_reader:destroy()

file_writer:waiton()
file_writer:destroy()
