
-- Global Data
local results = {}
local context = "global"
local argsave = {}
local exit_on_error = false

--[[
Function:   set_context
 Purpose:   sets global variables to proper context
   Notes:   none
]]
local function set_context (testname)
    if results[testname] == nil then
        results[testname] = {}
        results[testname]["asserts"] = 0
        results[testname]["errors"] = 0
        results[testname]["messages"] = {}
    end
    context = testname
end

--[[
Function:   set_exit_on_error
 Purpose:   if set will exit lua interpreter on first error
   Notes:   none
]]
local function set_exit_on_error (enable)
    exit_on_error = enable
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
local function srcscript ()
    local info = debug.getinfo(2,'S');
    local src = info.source:match("^.+/(.+)$")
    return src
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
    if status == false then
        if exit_on_error then os.exit() end
    end
    return status
end

--[[
Function:   check
 Purpose:   run statement and check if it returns true in the context of the executive
   Notes:   status is boolean
]]
local function check (expression, errmsg)
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
        if exit_on_error then os.exit() end
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
    print("\n--------------------------------------------")
    print("Running Test Script: " .. testname)
    print("--------------------------------------------\n")
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
Function:   compare
 Purpose:   do a binary comparison of two arbitrary strings
   Notes:   necessary to work around strings created from userdata
]]
local function compare(str1, str2) 
    local status = true
    bytes1 = {string.byte(str1, 0, -1)}
    bytes2 = {string.byte(str2, 0, -1)}
    if #bytes1 ~= #bytes2 then
        print("Strings are of unequal length: " .. tostring(#bytes1) .. " ~= " .. tostring(#bytes2))
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
        if exit_on_error then os.exit() end
    end
    return status
end

--[[
Function:   report
 Purpose:   display the results of the asserts as a summary
   Notes:   none
]]
local function report ()
    local total_asserts = 0
    local total_errors = 0
    if context == "global" then --suppress report until the end
        for testname in pairs(results) do
            set_context(testname)
            total_asserts = total_asserts + results[context]["asserts"]
            total_errors = total_errors + results[context]["errors"]
            print("\n---------------------------------")
            for i = 0, (results[context]["errors"]-1) do
                print("FAIL: " .. results[context]["messages"][i])
            end 
            print("---------------------------------")
            print("Executed test: " .. context)
            print("Number of asserts: " .. tostring(results[context]["asserts"]))
            print("Number of errors: " .. tostring(results[context]["errors"]))
            print("---------------------------------")
        end
        print("\n*********************************")
        print("Total number of asserts: " .. tostring(total_asserts))
        print("Total number of errors: " .. tostring(total_errors))
        print("*********************************")
    end
end


-- Export Package

set_context(context)

for k,v in pairs(arg) do
    argsave[k] = v        
end

local package = {
    set_exit_on_error = set_exit_on_error,
    rootdir = rootdir,
    srcscript = srcscript,
    command = command,
    check = check,
    script = script,
    compare = compare,
    report = report,
}

return package
