
-- Global Data
local results = {}
local context = "global"
local argsave = {}

--[[
Function:   set_context
 Purpose:   sets global variables to proper context
   Notes:   none
]]
local function set_context (testname)
    if results[testname] == nil then
        results[testname] = {}
        results[testname]["skipped"] = false
        results[testname]["asserts"] = 0
        results[testname]["errors"] = 0
        results[testname]["messages"] = {}
    end
    context = testname
end

--[[
Function:   isglobal
 Purpose:   true if global context
   Notes:   none
]]
local function isglobal ()
    return context == "global"
end

--[[
Function:   rootdir
 Purpose:   pattern matching on path to return root directory of file
   Notes:   none
]]
local function rootdir (filepath)
    local path_index = string.find(filepath, "/[^/]*$")
    if path_index then
        return filepath:sub(1,path_index)
    else
        return "./"
    end
end

--[[
Function:   scriptsrc
 Purpose:   pattern matching on source info to return calling script
   Notes:   none
]]
local function srcscript (fullpath)
    local info = debug.getinfo(2,'S')
    if fullpath then
        return info.source
    else
        local src = info.source:sub(2):match("^.+/(.+)$")
        local dir = info.source:sub(2):match("(.*[/\\])") or "./"
        return src, dir
    end
end

--[[
Function:   command
 Purpose:   handle the execution of the cmd_str in the context of the executive
   Notes:   none
]]
local function command (cmd_str)
    results[context]["asserts"] = results[context]["asserts"] + 1
    local status = cmd.exec(cmd_str)
    if status < 0 then
        results[context]["messages"][results[context]["errors"]] = debug.traceback(cmd_str)
        results[context]["errors"] = results[context]["errors"] + 1
    end
    return status
end

--[[
Function:   assert
 Purpose:   run statement and check if it returns true in the context of the executive
   Notes:   status is boolean
]]
local function assert (expression, errmsg, raise_on_error)
    results[context]["asserts"] = results[context]["asserts"] + 1
    local status = true
    if type(expression) == "string" then
        local f = assert(load("return " .. expression))
        status = f() -- execute expression
    else
        status = expression
    end
    if status == false or status == nil then
        results[context]["messages"][results[context]["errors"]] = debug.traceback(errmsg)
        results[context]["errors"] = results[context]["errors"] + 1
        if raise_on_error then error(errmsg) end
    end
    return status
end

--[[
Function:   script
 Purpose:   handle the execution of the script_str in the context of the executive
   Notes:   none
]]
local function script (script_str, parms)
    local testname = script_str:match("([^/]+)$")
    local numparms = 0
    set_context(testname)
    print("\n############################################")
    print("Running Test Script: " .. testname)
    print("############################################\n")
    if parms then
        for k,v in pairs(parms) do
            arg[k] = v
            numparms = numparms + 1
        end
    end
    dofile(script_str)
    if parms then
        for i=1,numparms do
            arg[i] = nil
        end
        for k,v in pairs(argsave) do
            arg[k] = v
        end
    end
    set_context("global")
end

--[[
Function:   unittest
 Purpose:   handle the execution of a unittest function
   Notes:   none
]]
local function unittest (testname, testfunc, skip)
    if skip then return nil end
    print("Starting test: " .. testname)
    local status, result = pcall(testfunc)
    assert(status, testname)
    return result
end

--[[
Function:   skip
 Purpose:   identify current test as being skipped
   Notes:   none
]]
local function skip (testname)
    if testname ~= nil then
        set_context(testname)
    end
    print("Skipping test: " .. context)
    results[context]["skipped"] = true
    set_context("global")
    return true
end

--[[
Function:   compare
 Purpose:   do a binary comparison of two arbitrary strings
   Notes:   necessary to work around strings created from userdata
]]
local function compare(str1, str2, errmsg)
    results[context]["asserts"] = results[context]["asserts"] + 1
    local status = true
    local bytes1 = {string.byte(str1, 0, -1)}
    local bytes2 = {string.byte(str2, 0, -1)}
    if #bytes1 ~= #bytes2 then
        print("Strings are of unequal length: " .. tostring(#bytes1) .. " ~= " .. tostring(#bytes2))
        print(str1, str2)
        status = false
    else
        for i=1,#bytes1 do
            if bytes1[i] ~= bytes2[i] then
                print("Miscompare on byte " .. tostring(i) .. "of string: " .. str1)
                status = false
                break
            end
        end
    end
    if status == false then
        results[context]["messages"][results[context]["errors"]] = debug.traceback(errmsg)
        results[context]["errors"] = results[context]["errors"] + 1
    end
    return status
end

--[[
Function:   cmpfloat
 Purpose:   compares two floating point numbers within a tolerance
   Notes:   status is boolean
]]
local function cmpfloat (f1, f2, t)
    local status = true
    if math.abs(f1 - f2) > t then
        status = false
    end
    return status
end

--[[
Function:   report
 Purpose:   display the results of the asserts as a summary
   Notes:   none
]]
local function report ()
    local total_tests = 0
    local total_skipped = 0
    local total_asserts = 0
    local total_errors = 0
    if context == "global" then --suppress report until the end
        for testname in pairs(results) do
            total_tests = total_tests + 1
            total_asserts = total_asserts + results[testname]["asserts"]
            total_errors = total_errors + results[testname]["errors"]
            if results[testname]["skipped"] then
                total_skipped = total_skipped + 1
                print("\n---------------------------------")
                print("Skipped test: " .. testname)
                print("---------------------------------")
            else
                print("\nvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv")
                for i = 0, (results[testname]["errors"]-1) do
                    print("FAIL: " .. results[testname]["messages"][i])
                end
                print("---------------------------------")
                print("Executed test: " .. testname)
                print("Number of asserts: " .. tostring(results[testname]["asserts"]))
                print("Number of errors: " .. tostring(results[testname]["errors"]))
                print("---------------------------------")
            end
        end
        print("\n*********************************")
        print("Total number of tests: " .. tostring(total_tests))
        print("Total number of skipped tests: " .. tostring(total_skipped))
        print("Total number of asserts: " .. tostring(total_asserts))
        print("Total number of errors: " .. tostring(total_errors))
        print("*********************************")
    end
    return total_errors
end


-- Export Package

set_context(context)

for k,v in pairs(arg) do
    argsave[k] = v
end

local package = {
    isglobal = isglobal,
    rootdir = rootdir,
    srcscript = srcscript,
    command = command,
    assert = assert,
    script = script,
    unittest = unittest,
    skip = skip,
    compare = compare,
    cmpfloat = cmpfloat,
    report = report,
}

return package
