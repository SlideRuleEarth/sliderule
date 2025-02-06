local runner = require("test_executive")
local td = runner.rootdir(arg[0])

-- Check If Present --
if not core.UNITTEST then return end

-- Dictionary Unit Test --

local ut = core.ut_dictionary()
runner.assert(ut:add_wordset("small", td.."/alphabet_words.txt", 26))
runner.assert(ut:add_wordset("large", td.."/english_words.txt", 354983))
runner.assert(ut:functional("small"))
runner.assert(ut:functional("large"))
runner.assert(ut:iterator("small"))

-- Report Results --

runner.report()

