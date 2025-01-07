local runner = require("test_executive")
local prettyprint = require("prettyprint")

-- Dictionary Unit Test --

local table_1a = {a = {1,2,3,4}, b = {11,12,13,14}, c = {21,22,23,24}}
local df_1a = core.dataframe(table_1a)
local table_1b = df_1a:export()

prettyprint.display(table_1a)
prettyprint.display(table_1b)

-- Report Results --

runner.report()

