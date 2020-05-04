package.path = package.path .. ";./?.lua" -- put local scripts in require search path
cfg = require("sigcfg")
cfg.configureLab()
