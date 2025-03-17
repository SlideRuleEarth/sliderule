local runner = require("test_executive")
local srcfile, dirpath = runner.srcscript()

-- Check Required Packages --

if not core.UNITTEST then
    print("Skipping dictionary self test")
    return
end

-- Dictionary Unit Test --

local ut = core.ut_dictionary()
runner.assert(ut:add_wordset("small", dirpath.."../data/alphabet_words.txt", 26))
runner.assert(ut:add_wordset("large", dirpath.."../data/english_words.txt", 354983))
runner.assert(ut:functional("small"))
runner.assert(ut:functional("large"))
runner.assert(ut:iterator("small"))

-- Report Results --

runner.report()

