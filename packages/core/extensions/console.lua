local console_monitor = core.monitor(core.INFO, core.FMT_TEXT):name("console.monitor")

local function loglvl (lvl)
    console_monitor:config(lvl)
    sys.setlvl(core.LOG, lvl)
end

local package = {
    monitor = console_monitor,
    loglvl = loglvl
}

return package
