--
-- ENDPOINT:    /source/cre
--
-- INPUT:       arg[1]
--              {
--                  "parms":
--                  {
--                      "image":  "<container image>",
--                      "timeout": <timeout in seconds>,
--                      "userdata": "<string passed to running container>"
--                  }
--              }
--
-- OUTPUT:      <output from container>
--

local json  = require("json")
local rqst  = json.decode(arg[1])
local parms = rqst["parms"]

-- initialize result
local result = "{}"

-- get settings
local shared_directory = cre.settings()
local unique_shared_directory = string.format("%s/%s", shared_directory, rqstid)

-- create unique shared directory
if not cre.createunique(unique_shared_directory) then goto cleanup end

-- write input parameter files
if parms["parms"] then
    for filename,contents in pairs(parms["parms"]) do
        local unique_filename = string.format("%s/%s", unique_shared_directory, filename)
        local input_file, msg = io.open(unique_filename, "w")
        if not input_file then 
            sys.log(core.CRITICAL, string.format("Failed to open input file %s: %s", unique_filename, msg))
            goto cleanup
        end
        input_file:write(json.encode(contents))
        input_file:close()
    end
end

-- execute container
do
    -- create container runner
    local cre_parms = cre.parms(parms)
    local cre_runner = cre.container(cre_parms, unique_shared_directory)

    -- initialize timeout variables
    local timeout = parms["node-timeout"] or parms["timeout"] or netsvc.NODE_TIMEOUT
    local duration = 0
    local interval = 10 < timeout and 10 or timeout -- seconds

    -- wait for container runner to complete
    while not cre_runner:waiton(interval * 1000) do
        duration = duration + interval
        if timeout >= 0 and duration >= timeout then
            sys.log(core.INFO, string.format("container <%s> request timed-out after %d seconds", parms["image"], duration))
            do return false end
        end
        sys.log(core.INFO, string.format("container <%s> continuing to run (after %d seconds)", parms["image"], duration))
    end
end

-- process output
do
    -- read output file
    local output_file, msg = io.open(unique_output_ctrl_filename, "r")
    if not output_file then 
        sys.log(core.CRITICAL, string.format("Failed to open output file %s: %s", unique_output_ctrl_filename, msg))
        goto cleanup 
    end
    local output_parms = json.decode(output_file:read())
    output_file:close()

    -- read results
    local output_files = output_parms["output_files"]
    if #output_files > 1 then
        local result_table = {}
        for index,filename in ipairs(output_files) do
            local unique_filename = string.format("%s/%s", unique_shared_directory, filename)
            local result_file, msg = io.open(unique_filename, "r")
            if not result_file then 
                sys.log(core.CRITICAL, string.format("Failed to open result file %s: %s", unique_filename, msg))
                goto cleanup 
            end
            result_table[string.format("output_%d", index)] = result_file:read()
            result_file:close()
            result = json.decode(result_table)
        end
    else
        local unique_filename = string.format("%s/%s", unique_shared_directory, output_files[1])
        local result_file, msg = io.open(unique_filename, "r")
        if not result_file then 
            sys.log(core.CRITICAL, string.format("Failed to open result file %s: %s", unique_filename, msg))
            goto cleanup 
        end
        result = result_file:read()
        result_file:close()
    end
end

-- clean up --
::cleanup::

-- delete unique shared directory
cre.deleteunique(unique_shared_directory)

return result
