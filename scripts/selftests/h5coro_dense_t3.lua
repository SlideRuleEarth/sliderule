-- H5CORO DENSE TEST 3 --
-- Variable: goes_imager_projection
-- Attributes: 
    -- latitude_of_projection_origin (64 bit float aka double)
    -- semi_major_axis (64 bit float aka double)

    local runner = require("test_executive")
    local console = require("console")
    local td = runner.rootdir(arg[0])
    
    -- Setup --
    console.monitor:config(core.LOG, core.DEBUG) 
    sys.setlvl(core.LOG, core.DEBUG)
    asset = core.asset("local", "nil", "file", td, "empty.index")
    
    -- Unit Test --
    print('\n------------------\nTest: Read Dataset\n------------------')
    
    dataq = "dataq"
    rsps2 = msg.subscribe(dataq)
    
    -- Arguments & Call --
    
    -- SUB TEST 1 - latitude_of_projection_origin -- 
    
    local resource_path = "OR_ABI-L2-FDCC-M3_G17_s20182390052191_e20182390054564_c20182390055159.nc"
    local dataset_name = "/goes_imager_projection/latitude_of_projection_origin"
    local id = 0
    local raw = true
    local dtype = core.DYNAMIC
    local col = 0
    local startrow = 0
    local numrows = core.ALL_ROWS
    
    f2 = h5.dataset(core.READER, 
                    asset, 
                    resource_path, 
                    dataset_name, 
                    id, 
                    raw, 
                    dtype, 
                    col, 
                    startrow, 
                    numrows)
    
    r2 = core.reader(f2, dataq)
    vals = rsps2:recvstring(3000) -- read out from listner
    e1 = string.unpack('d', vals)
    print("RESULT latitude_of_projection_origin: " .. e1)
    runner.check("0.0" == tostring(e1), "failed dataset read")

    rsps2:destroy()
    r2:destroy()
    
    -- SUB TEST 2 - semi_major_axis -- 
    
    asset2 = core.asset("local", "nil", "file", td, "empty.index")
    dataq = "dataq2"
    rsps2s2 = msg.subscribe(dataq)
    
    dataset_name = "/goes_imager_projection/semi_major_axis"
    dtype = core.DYNAMIC
    
    f3 = h5.dataset(core.READER, 
                    asset2, 
                    resource_path, 
                    dataset_name, 
                    id, 
                    raw, 
                    dtype, 
                    col, 
                    startrow, 
                    numrows)
    
    r3 = core.reader(f3, dataq)
    vals = rsps2s2:recvstring(3000) -- read out from listner
    e1 = string.unpack('d', vals)
    print("RESULT latitude_of_projection_origin: " .. e1)
    runner.check("6378137.0" == tostring(e1), "failed dataset read")
    
    rsps2s2:destroy()
    r3:destroy()
    
   -- Report Results --
    
    runner.report()