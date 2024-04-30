local json  = require("json")

--
-- Setup Container Runtime Environment
--
local function setup ()

    local host_shared_directory = cre.settings()
    local unique_shared_directory = string.format("%s/%s", host_shared_directory, rqstid)
    if cre.createunique(unique_shared_directory) then
        sys.log(core.WARNING, "Overwriting existing unique shared directory "..unique_shared_directory)
    end
    return {unique_shared_directory=unique_shared_directory, host_shared_directory=host_shared_directory}
end

--
-- Execute Container
--
local function execute (parms, shared_directory, response_queue)

    -- write input parameter files
    if parms["parms"] then
        for filename,contents in pairs(parms["parms"]) do
            local unique_filename = string.format("%s/%s", shared_directory, filename)
            local input_file, msg = io.open(unique_filename, "w")
            if not input_file then 
                sys.log(core.CRITICAL, string.format("Failed to create input file %s: %s", unique_filename, msg))
                return false
            end
            input_file:write(json.encode(contents))
            input_file:close()
        end
    end

    -- create container runner (automatically executes container)
    local cre_parms = cre.parms(parms)
    local cre_runner = cre.container(cre_parms, shared_directory, response_queue)

    -- return container runner
    return cre_runner

end

--
-- Wait for Container
--
local function wait (parms, cre_runner)

    local timeout = parms["timeout"] or netsvc.NODE_TIMEOUT
    return cre_runner:waiton(timeout * 1000)

end

--
-- Cleanup Container Runtime Environment
--
local function cleanup (crenv)

    cre.deleteunique(crenv.unique_shared_directory)

end

--
-- Package
--
return {
    setup = setup,
    execute = execute,
    wait = wait,
    cleanup = cleanup
}