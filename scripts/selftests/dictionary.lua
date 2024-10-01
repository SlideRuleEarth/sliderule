local runner = require("test_executive")
local td = runner.rootdir(arg[0])

-- Check If Present --
if not core.UNITTEST then return end

-- Dictionary Unit Test --

local ut = core.ut_dictionary()
runner.check(ut:add_wordset("small", td.."/alphabet_words.txt", 26))
runner.check(ut:add_wordset("large", td.."/english_words.txt", 354983))
runner.check(ut:functional("small"))
runner.check(ut:functional("large"))
runner.check(ut:iterator("small"))

-- Report Results --

runner.report()

