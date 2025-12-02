--
-- Provides functions to query CMR, STAC, TNM
--
--  NOTES:  The code below uses libcurl to issue http requests

local json = require("json")

--
-- Constants
--

-- Asset Directory
ASSETS = {
    ["icesat2"] = {
        name        = "ATL03",
        identity    = "nsidc-cloud",
        driver      = "cumulus",
        path        = "nsidc-cumulus-prod-protected",
        region      = "us-west-2",
        provider    = "NSIDC_CPRD",
        version     = "007",
        api         = "cmr",
        formats     = {".h5"}
    },
    ["icesat2-atl06"] = {
        name        = "ATL06",
        identity    = "nsidc-cloud",
        driver      = "cumulus",
        path        = "nsidc-cumulus-prod-protected",
        region      = "us-west-2",
        provider    = "NSIDC_CPRD",
        version     = "007",
        api         = "cmr",
        formats     = {".h5"}
    },
    ["icesat2-atl08"] = {
        name        = "ATL08",
        identity    = "nsidc-cloud",
        driver      = "cumulus",
        path        = "nsidc-cumulus-prod-protected",
        region      = "us-west-2",
        provider    = "NSIDC_CPRD",
        version     = "007",
        api         = "cmr",
        formats     = {".h5"}
    },
    ["icesat2-atl09"] = {
        name        = "ATL09",
        identity    = "nsidc-cloud",
        driver      = "cumulus",
        path        = "nsidc-cumulus-prod-protected",
        region      = "us-west-2",
        provider    = "NSIDC_CPRD",
        version     = "006",
        api         = "cmr",
        formats     = {".h5"}
    },
    ["icesat2-atl13"] = {
        name        = "ATL13",
        identity    = "nsidc-cloud",
        driver      = "s3atl13",
        path        = "nsidc-cumulus-prod-protected",
        region      = "us-west-2",
        provider    = "NSIDC_CPRD",
        version     = "006",
        api         = "ams",
        formats     = {".h5"},
        endpoint    = "atl13"
    },
    ["icesat2-atl24"] = {
        name        = "ATL24_ARCHIVED",
        identity    = "nsidc-cloud",
        driver      = "s3atl24",
        path        = "nsidc-cumulus-prod-protected",
        region      = "us-west-2",
        provider    = "NSIDC_CPRD",
        version     = "001",
        api         = "cmr",
        formats     = {".h5"}
    },
    ["gedil4a"] = {
        name        = "GEDI_L4A_AGB_Density_V2_1_2056",
        identity    = "ornl-cloud",
        driver      = "s3",
        path        = "ornl-cumulus-prod-protected/gedi/GEDI_L4A_AGB_Density_V2_1/data",
        region      = "us-west-2",
        provider    = "ORNL_CLOUD",
        api         = "cmr",
        formats     = {".h5"}
    },
    ["gedil4b"] = {
        name        = "GEDI_L4B_Gridded_Biomass_2017",
        identity    = "ornl-cloud",
        driver      = "s3",
        path        = "/vsis3/ornl-cumulus-prod-protected/gedi/GEDI_L4B_Gridded_Biomass_V2_1/data/GEDI04_B_MW019MW223_02_002_02_R01000M_V2.tif",
        region      = "us-west-2",
        provider    = "ORNL_CLOUD",
        formats     = {".tiff"}
    },
    ["gedil3-elevation"] = {
        name        = "GEDI_L3_LandSurface_Metrics_V2_1952",
        identity    = "ornl-cloud",
        path        = "/vsis3/ornl-cumulus-prod-protected/gedi/GEDI_L3_LandSurface_Metrics_V2/data/GEDI03_elev_lowestmode_mean_2019108_2022019_002_03.tif",
        region      = "us-west-2",
        provider    = "ORNL_CLOUD",
        formats     = {".tiff"}
    },
    ["gedil3-canopy"] = {
        name        = "GEDI_L3_LandSurface_Metrics_V2_1952",
        identity    = "ornl-cloud",
        path        = "/vsis3/ornl-cumulus-prod-protected/gedi/GEDI_L3_LandSurface_Metrics_V2/data/GEDI03_rh100_mean_2019108_2022019_002_03.tif",
        region      = "us-west-2",
        provider    = "ORNL_CLOUD",
        formats     = {".tiff"}
    },
    ["gedil3-elevation-stddev"] = {
        name        = "GEDI_L3_LandSurface_Metrics_V2_1952",
        identity    = "ornl-cloud",
        path        = "/vsis3/ornl-cumulus-prod-protected/gedi/GEDI_L3_LandSurface_Metrics_V2/data/GEDI03_elev_lowestmode_stddev_2019108_2022019_002_03.tif",
        region      = "us-west-2",
        provider    = "ORNL_CLOUD",
        formats     = {".tiff"}
    },
    ["gedil3-canopy-stddev"] = {
        name        = "GEDI_L3_LandSurface_Metrics_V2_1952",
        identity    = "ornl-cloud",
        path        = "/vsis3/ornl-cumulus-prod-protected/gedi/GEDI_L3_LandSurface_Metrics_V2/data/GEDI03_rh100_stddev_2019108_2022019_002_03.tif",
        region      = "us-west-2",
        provider    = "ORNL_CLOUD",
        formats     = {".tiff"}
    },
    ["gedil3-counts"] = {
        name        = "GEDI_L3_LandSurface_Metrics_V2_1952",
        identity    = "ornl-cloud",
        path        = "/vsis3/ornl-cumulus-prod-protected/gedi/GEDI_L3_LandSurface_Metrics_V2/data/GEDI03_counts_2019108_2022019_002_03.tif",
        region      = "us-west-2",
        provider    = "ORNL_CLOUD",
        formats     = {".tiff"}
    },
    ["gedil2a"] = {
        name        = "GEDI02_A",
        identity    = "lpdaac-cloud",
        driver      = "s3gedi",
        path        = "lp-prod-protected",
        region      = "us-west-2",
        provider    = "LPCLOUD",
        version     = "002",
        api         = "cmr",
        formats     = {".h5"}
    },
    ["gedil1b"] = {
        name        = "GEDI01_B",
        identity    = "lpdaac-cloud",
        driver      = "s3gedi",
        path        = "lp-prod-protected",
        region      = "us-west-2",
        provider    = "LPCLOUD",
        version     = "002",
        api         = "cmr",
        formats     = {".h5"}
    },
    ["swot-sim-ecco-llc4320"] = {
        name        = "SWOT_SIMULATED_L2_KARIN_SSH_ECCO_LLC4320_CALVAL_V1",
        identity    = "podaac-cloud",
        driver      = "s3",
        path        = "podaac-ops-cumulus-protected/SWOT_SIMULATED_L2_KARIN_SSH_ECCO_LLC4320_CALVAL_V1",
        region      = "us-west-2",
        provider    = "POCLOUD",
        api         = "cmr",
        formats     = {".nc"},
        collections = {"C2147947806-POCLOUD"}
    },
    ["swot-sim-glorys"] = {
        name        = "SWOT_SIMULATED_L2_KARIN_SSH_GLORYS_CALVAL_V1",
        identity    = "podaac-cloud",
        driver      = "s3",
        path        = "podaac-ops-cumulus-protected/SWOT_SIMULATED_L2_KARIN_SSH_GLORYS_CALVAL_V1",
        region      = "us-west-2",
        provider    = "POCLOUD",
        api         = "cmr",
        formats     = {".nc"},
        collections = {"C2152046451-POCLOUD"}
    },
    ["usgs3dep-1meter-dem"] = {
        name        = "Digital Elevation Model (DEM) 1 meter",
        path        = "/vsis3/prd-tnm",
        region      = "us-west-2",
        provider    = "USGS",
        api         = "tnm",
        formats     = {".tiff"}
    },
    ["3dep1m"] = {
        name        = "3DEP",
        path        = "/vsis3/prd-tnm",
        region      = "us-west-2",
        provider    = "USGS",
        api         = "ams",
        formats     = {".tiff"},
        endpoint    = "3dep",
        stac        = true
    },
    ["usgs3dep-10meter-dem"] = {
        path        = "/vsis3/prd-tnm",
        index       = "/vsis3/prd-tnm/StagedProducts/Elevation/13/TIFF/USGS_Seamless_DEM_13.vrt",
        region      = "us-west-2",
    },
    ["esa-worldcover-10meter"] = {
        path        = "/vsis3/esa-worldcover/v200/2021/map",
        index       = "/vsis3/sliderule/data/WORLDCOVER/ESA_WorldCover_10m_2021_v200_Map.vrt",
        region      = "eu-central-1",
    },
    ["meta-globalcanopy-1meter"] = {
        path        = "/vsis3/dataforgood-fb-data/forests/v1/alsgedi_global_v6_float/",
        index       = "/vsis3/sliderule/data/GLOBALCANOPY/META_GlobalCanopyHeight_1m_2024_v1.vrt",
        region      = "us-east-1",
    },
    ["bluetopo-bathy"] = {
        identity    = "iam-role",
        path        = "/vsis3/noaa-ocs-nationalbathymetry-pds/BlueTopo/",
        index       = "_BlueTopo_Tile_Scheme",
        region      = "us-east-1",
    },
    ["landsat-hls"] = {
        name        = "HLS",
        identity    = "lpdaac-cloud",
        path        = "/vsis3/lp-prod-protected",
        region      = "us-west-2",
        provider    = "LPCLOUD",
        api         = "stac",
        formats     = {".tiff"},
        collections = {"HLSS30.v2.0", "HLSL30.v2.0"},
        url         = "https://cmr.earthdata.nasa.gov/stac/LPCLOUD/search"
    },
    ["arcticdem-mosaic"] = {
        path        = "/vsis3/pgc-opendata-dems/arcticdem/mosaics/v4.1",
        index       = "/vsis3/sliderule/data/PGC/arcticdem_2m_v4_1_tiles.vrt",
        region      = "us-west-2"
    },
    ["arcticdem-strips"] = {
        name        = "arcticdem-strips",
        path        = "/vsis3/pgc-opendata-dems/",
        region      = "us-west-2",
        provider    = "PGC",
        api         = "stac",
        formats     = {".tiff"},
        collections = {"arcticdem-strips-s2s041-2m"},
        url         = "https://stac.pgc.umn.edu/api/v1/search"
    },
    ["rema-mosaic"] = {
        path        = "/vsis3/pgc-opendata-dems/rema/mosaics/v2.0/2m",
        index       = "/vsis3/sliderule/data/PGC/rema_2m_v2_0_tiles.vrt",
        region      = "us-west-2"
    },
    ["rema-strips"] = {
        name        = "rema-strips",
        path        = "/vsis3/pgc-opendata-dems/",
        region      = "us-west-2",
        provider    = "PGC",
        api         = "stac",
        formats     = {".tiff"},
        collections = {"rema-strips-s2s041-2m"},
        url         = "https://stac.pgc.umn.edu/api/v1/search"
    },
    ["nisar-L2-geoff"] = {
        identity   = "asf-cloud",
        path        = "/vsis3/sds-n-cumulus-prod-nisar-sample-data",
        region      = "us-west-2"
    },
    ["gedtm-30meter"] = {
        identity   = "iam-role",
        path        = "/vsis3/sliderule/data/GEDTM/legendtm_rf_30m_m_s_20000101_20231231_go_epsg.4326_v20250130.tif",
        region      = "us-west-2"
    },
    ["gedtm-std"] = {
        identity   = "iam-role",
        path        = "/vsis3/sliderule/data/GEDTM/gendtm_rf_30m_std_s_20000101_20231231_go_epsg.4326_v20250209.tif",
        region      = "us-west-2"
    },
    ["gedtm-dfm"] = {
        identity   = "iam-role",
        path        = "/vsis3/sliderule/data/GEDTM/dfme_edtm_m_30m_s_20000101_20221231_go_epsg.4326_v20241230.tif",
        region      = "us-west-2"
    },
    ["atlas-s3"] = {
        identity    = "iam-role",
        driver      = "s3",
        path        = "sliderule/data/ATLAS",
        region      = "us-west-2"
    },
    ["atl24-s3"] = {
        identity    = "iam-role",
        driver      = "s3",
        path        = "sliderule/data/ATL24r2",
        region      = "us-west-2",
        provider    = "NSIDC_CPRD",
        version     = "002",
        api         = "ams",
        formats     = {".h5"},
        endpoint    = "atl24"
    },
    ["swot-s3"] = {
        identity    = "iam-role",
        driver      = "s3",
        path        = "sliderule/data/SWOT",
        region      = "us-west-2"
    },
    ["viirsj1-s3"] = {
        identity    = "iam-role",
        driver      = "s3",
        path        = "sliderule/data/VIIRSJ1",
        region      = "us-west-2"
    },
    ["merit-s3"] = {
        identity    = "iam-role",
        driver      = "s3",
        path        = "sliderule/data/MERIT",
        region      = "us-west-2"
    },
    ["gebco-s3"] = {
        identity    = "iam-role",
        driver      = "s3",
        path        = "/vsis3/sliderule/data/GEBCO",
        index       = "index.geojson",
        region      = "us-west-2"
    },
    ["atlas-local"] = {
        driver      = "file",
        path        = "/data/ATLAS",
    },
    ["gedi-local"] = {
        driver      = "file",
        path        = "/data/GEDI",
    },
    ["sliderule-stage"] = {
        identity    = "iam-role",
        driver      = "s3",
        path        = "sliderule-public",
        region      = "us-west-2"
    },
}

-- default maximum number of resources to process in one request
DEFAULT_MAX_REQUESTED_RESOURCES = 300

-- upper limit on resources returned from CMR query per request
CMR_PAGE_SIZE = 2000

-- upper limit on resources returned from STAC query per request
STAC_PAGE_SIZE = 100

-- upper limit on resources returned from TNM query per request
TNM_PAGE_SIZE = 100

-- response codes for all of the package functions
RC_SUCCESS = 0
RC_FATAL_ERROR = -1
RC_RQST_FAILED = -2
RC_RSPS_UNPARSEABLE = -3
RC_RSPS_UNEXPECTED = -4
RC_RSPS_TRUNCATED = -5
RC_UNSUPPORTED = -6
RC_INVALID_GEOMETRY = -7

-- reverse lookup for short names
DATASETS = (function()
    local name_lookup = {}
    for asset,info in pairs(ASSETS) do
        if info["name"] then
            name_lookup[info["name"]] = ASSETS[asset]
        end
    end
    return name_lookup
end)()

--
-- Build GeoJSON
--
local function build_geojson(rsps)
    local status, geotable = pcall(json.decode, rsps)
    local next_page = {link=nil, body=nil}
    if status then
        -- populate next page (for paged results)
        if geotable["links"] then
            for _,link in ipairs(geotable["links"]) do
                if link["rel"] == "next" then
                    next_page = {link=link["href"], body=(link["body"] and json.encode(link["body"]) or nil)}
                end
            end
        end
        -- trim unneeded elements of the table
        geotable["links"] = nil
        geotable["numberMatched"] = nil
        geotable["numberReturned"] = nil
        for i = 1, (geotable["features"] and #geotable["features"] or 0) do
            local feature = geotable["features"][i]
            local feature_assets = feature["assets"]
            local feature_properties = feature["properties"]
            feature["links"] = nil
            feature["stac_version"] = nil
            feature["stac_extensions"] = nil
            feature["collection"] = nil
            feature["bbox"] = nil
            feature_assets["browse"] = nil
            feature_assets["metadata"] = nil
            for k,_ in pairs(feature_assets) do
                feature_properties[k] = feature_assets[k]["href"]
            end
            feature["assets"] = nil
        end
    end
    -- return geotable
    return status, geotable, next_page
end

--
-- Clean Polygon
--
local function clean_polygon(poly)
    -- check type
    if type(poly) ~= 'table' then
        sys.log(core.ERROR, "Invalid type for polygon detected: " .. type(poly))
        return nil
    end
    -- check size
    local num_coords = #poly
    if num_coords <= 3 then
        sys.log(core.ERROR, "Invalid length of polygon detected: " .. tostring(num_coords))
        return nil
    end
    -- close polygon if necessary
    if (poly[1]["lat"] ~= poly[num_coords]["lat"]) and
       (poly[1]["lon"] ~= poly[num_coords]["lon"]) then
        sys.log(core.ERROR, "Polygon is not closed")
        return nil
    end
    -- determine winding of polygon: sum((x2 - x1) * (y2 + y1))
    local wind = 0
    for i=1,(num_coords-1) do
        wind = wind + ((poly[i+1]["lon"] - poly[i]["lon"]) * (poly[i+1]["lat"] + poly[i]["lat"]))
    end
    -- reverse direction (make counter-clockwise) if necessary
    if wind > 0 then
        for i = 1,math.floor(num_coords/2) do
            poly[i], poly[num_coords-i+1] = poly[num_coords-i+1], poly[i]
        end
    end
    -- return cleaned version of polygon (in addition to changing in place)
    return poly
end

--
-- urlify
--
local function urlify(str)
    str = string.gsub(str, "\n", "\r\n")
    str = string.gsub(str, "([^%w %-%_%.%~])", function(c) return string.format("%%%02X", string.byte(c)) end)
    str = string.gsub(str, " ", "+")
    return str
end

---------------------------------------------------------------
-- PUBLIC PACKAGE
---------------------------------------------------------------

--
-- Create global assets
--
local function load ()
    local assets = {}
    for k,v in pairs(ASSETS) do
        local asset = core.getbyname(k) -- see if asset already exists
        assets[k] = asset or core.asset(k, v["identity"], v["driver"], v["path"], v["index"], v["region"], v["endpoint"]):global(k)
    end
    return assets
end

--
-- AMS
--
local function ams (parms, poly, _with_meta, _short_name)

    -- get dataset
    local dataset       = DATASETS[_short_name or parms["short_name"]] or ASSETS[parms["asset"]]
    local endpoint      = dataset["endpoint"]
    local max_resources = parms["max_resources"] or DEFAULT_MAX_REQUESTED_RESOURCES
    local with_meta     = _with_meta or parms["with_meta"] or dataset["stac"]

    -- build local parameters that combine top level parms with endpoint (e.g. atl13) specific parms
    local ams_parms             = parms[endpoint] or {}
    ams_parms["t0"]             = ams_parms["t0"] or parms["t0"]
    ams_parms["t1"]             = ams_parms["t1"] or parms["t1"]
    ams_parms["poly"]           = poly or ams_parms["poly"] or parms["poly"]
    ams_parms["name_filter"]    = ams_parms["name_filter"] or parms["name_filter"]
    ams_parms["rgt"]            = ams_parms["rgt"] or parms["rgt"] -- backwards compatibility
    ams_parms["cycle"]          = ams_parms["cycle"] or parms["cycle"] -- backwards compatibility
    ams_parms["region"]         = ams_parms["region"] or parms["region"] -- backwards compatibility

    -- make request and process response
    local status, response = core.ams("POST", dataset["endpoint"], json.encode(ams_parms))
    if status then
        local rc, data = pcall(json.decode, response)
        if rc then
            if with_meta then -- returns full response from the AMS
                return RC_SUCCESS, data
            elseif data["granules"] then -- pulls out just the granules from the AMS
                local num_granules = #data["granules"]
                if num_granules > max_resources then
                    return RC_RSPS_TRUNCATED, string.format("%s resources exceeded maximum allowed: %d > %d", dataset["name"] or "unknown", num_granules, max_resources)
                else
                    return RC_SUCCESS, data["granules"]
                end
            else
                return RC_RSPS_UNEXPECTED, "granules not found in response from AMS"
            end
        else
            return RC_RSPS_UNPARSEABLE, "could not parse json in response from AMS"
        end
    else
        return RC_FATAL_ERROR, string.format("request to AMS failed: %s", response)
    end

end

--
-- CMR
--
local function cmr (parms, poly, _with_meta, _short_name)

    local linktable = {}

    -- get parameters of request
    local dataset       = DATASETS[_short_name or parms["short_name"]] or ASSETS[parms["asset"]]
    local provider      = dataset["provider"] or error("unable to determine provider for query")
    local cmr_parms     = parms["cmr"] or {}
    local version       = cmr_parms["version"] or dataset["version"]
    local polygon       = poly or parms["poly"]
    local t0            = parms["t0"]
    local t1            = parms["t1"]
    local max_resources = parms["max_resources"] or DEFAULT_MAX_REQUESTED_RESOURCES
    local with_meta     = _with_meta or parms["with_meta"]

    -- build name filter
    local name_filter   = parms["name_filter"]
    local rgt           = parms["rgt"]
    local cycle         = parms["cycle"]
    local region        = parms["region"]
    if (not name_filter) and
       (rgt or cycle or region) then
        local rgt_filter = rgt and string.format("%04d", rgt) or '????'
        local cycle_filter = cycle and string.format("%02d", cycle) or '??'
        local region_filter = region and string.format("%02d", region) or '??'
        name_filter = '*_' .. rgt_filter .. cycle_filter .. region_filter .. '_*'
    end

    -- flatten polygon
    local polystr = ""
    if polygon then
        for i,coord in ipairs(polygon) do
            if i < #polygon then
                polystr = polystr .. string.format("%.15f,%.15f,", coord["lon"], coord["lat"]) -- with trailing comma
            else
                polystr = polystr .. string.format("%.15f,%.15f", coord["lon"], coord["lat"]) -- without trailing comma
            end
        end
    end

    -- build query request
    local cmr_query_url = table.concat({
        "https://cmr.earthdata.nasa.gov/search/granules.json",
        string.format("?provider=%s", provider),
        "&sort_key[]=start_date&sort_key[]=producer_granule_id",
        string.format("&page_size=%s", CMR_PAGE_SIZE),
        string.format("&short_name=%s", dataset["name"]),
        version and string.format("&version=%s", version) or "",
        (t0 and t1) and string.format("&temporal[]=%s,%s", t0, t1) or "",
        polygon and string.format("&polygon=%s", polystr) or "",
        name_filter and string.format("&options[producer_granule_id][pattern]=true&producer_granule_id[]=%s", name_filter) or "",
    })

    -- make requests and page through responses
    local headers = {}
    local total_links = 0
    while true do

        -- issue http request
        local rsps, hdrs, status = core.get(cmr_query_url, nil, headers, false, false, true)
        if not status then
            return RC_RQST_FAILED, string.format("http request to CMR failed <%s>", rsps)
        end

        -- add search after link for future page requests
        headers["cmr-search-after"] = hdrs["cmr-search-after"]

        -- get and check number of cmr hits
        local cmr_hits = tonumber(hdrs["cmr-hits"])
        if cmr_hits > max_resources then
            return RC_RSPS_TRUNCATED, string.format("number of CMR hits <%d> exceeded maximum allowed <%d> for %s", cmr_hits, max_resources, dataset["name"])
        elseif total_links > max_resources then
            return RC_RSPS_TRUNCATED, string.format("number of total links found <%d> exceeded maximum allowed <%d> for %s", total_links, max_resources, dataset["name"])
        end

        -- decode response text (json) into lua table
        local rc, results = pcall(json.decode, rsps)
        if not rc then
            return RC_RSPS_UNPARSEABLE, "could not parse json in response from CMR"
        end

        -- confirm necessary fields are present in the response
        if not results["feed"] or not results["feed"]["entry"] then
            return RC_RSPS_UNEXPECTED, "could not find expected fields in response from CMR"
        end

        -- loop through each response entry and pull out the desired links
        local num_links = 0
        for _,e in ipairs(results["feed"]["entry"]) do
            -- check total links
            if total_links >= max_resources then
                break
            end
            -- build metadata with polygon and time stamps
            local poly_table = {}
            if e["polygons"] then
                local polygon_list = {}
                local pattern = string.format("([^%s]+)", " ") -- builds a delimeter (' ')
                e["polygons"][1][1]:gsub(pattern, function(substring) -- splits string over delimeter
                    table.insert(polygon_list, substring)
                end)
                for i = 1,#polygon_list,2 do
                    table.insert(poly_table, {lat=polygon_list[i], lon=polygon_list[i+1]})
                end
            end
            local metadata = { poly = poly_table, t0 = e["time_start"], t1 = e["time_end"] }
            -- loop through links to see if a link matching the criteria is found
            local links = e["links"]
            if links then
                for _,l in ipairs(links) do
                    local link = l["href"]
                    if link then
                        -- exclude opendap links - this is a hack, but is the only way
                        -- to currently remove these duplicate responses (and is the way
                        -- CMR code currently does it as well)
                        if not string.find(link, "opendap") then
                            -- grab only links with the desired format (checks extension match)
                            for _,format in ipairs(dataset["formats"]) do
                                if link:sub(-#format) == format then
                                    -- split link over '/' and grab last element
                                    local url = link
                                    for token in link:gmatch("[^" .. "/"  .. "]+") do
                                        url = token
                                    end
                                    -- add url to table of links with metadata
                                    if not linktable[url] then
                                        linktable[url] = metadata
                                        num_links = num_links + 1
                                        total_links = total_links + 1
                                    end
                                    -- desired link is found
                                    break
                                end
                            end
                        end
                    end
                end
            end
        end

        -- check completion
        if cmr_hits ~= nil and total_links >= cmr_hits then -- number of hits reached
            break -- all done
        elseif num_links == 0 then -- no links added with last request
            break -- all done
        end
    end

    -- return results
    if with_meta then
        return RC_SUCCESS, linktable
    else
        local urls = {}
        for link,_ in pairs(linktable) do
            table.insert(urls, link)
        end
        return RC_SUCCESS, urls
    end

end

--
-- STAC
--
local function stac (parms, poly)

    local geotable = {}

    -- get parameters of request
    local dataset       = DATASETS[parms["short_name"]] or ASSETS[parms["asset"]]
    local url           = dataset["url"]
    local collections   = parms["collections"] or dataset["collections"]
    local polygon       = poly or parms["poly"]
    local t0            = parms["t0"] or '2018-01-01T00:00:00Z'
    local t1            = parms["t1"] or string.format('%04d-%02d-%02dT%02d:%02d:%02dZ', time.gps2date(time.gps()))
    local max_resources = parms["max_resources"] or DEFAULT_MAX_REQUESTED_RESOURCES

    -- build stac request
    local headers = {["Content-Type"] = "application/json"}
    local rqst = {
        ["limit"] = STAC_PAGE_SIZE,
        ["datetime"] = string.format("%s/%s", t0, t1),
        ["collections"] = collections,
    }

    -- add polygon if provided
    if polygon then
        local coordinates = {}
        for _,v in pairs(polygon) do
            table.insert(coordinates, {v["lon"], v["lat"]})
        end
        if #coordinates > 1 then
            rqst["intersects"] = {
                ["type"] = "Polygon",
                ["coordinates"] = {coordinates}
            }
        else
            rqst["intersects"] = {
                ["type"] = "Point",
                ["coordinates"] = coordinates[1]
            }
        end
    end
    -- post initial request
    local rsps, status = core.post(url, json.encode(rqst), headers)
    if not status then
        return RC_FATAL_ERROR, "http request to stac server failed -> " .. rsps
    end

    -- parse response into a table
    local next_page = {}
    status, geotable, next_page = build_geojson(rsps)
    if not status then
        return RC_RSPS_UNPARSEABLE, "could not parse json in response from stac"
    end

    -- check number of matches
    local num_matched = geotable["features"] and #geotable["features"] or 0
    if num_matched > max_resources then
        return RC_RSPS_TRUNCATED, string.format("number of matched resources %d exceeded limit %d", num_matched, max_resources)
    end

    -- iterate through additional pages if not all returned
    while next_page["link"] do
        -- post paged request
        rsps, status = core.post(next_page["link"], next_page["body"], headers)
        if not status then
            return RC_FATAL_ERROR, "http request to stac server failed -> " .. rsps
        end

        -- parse paged response into table
        local paged_geotable = nil
        status, paged_geotable, next_page = build_geojson(rsps)
        if not status then
            return RC_RSPS_UNPARSEABLE, "could not parse json in response from stac server"
        end

        -- check updated number of matches
        num_matched = num_matched + (paged_geotable["features"] and #paged_geotable["features"] or 0)
        if num_matched > max_resources then
            return RC_RSPS_TRUNCATED, string.format("number of matched resources %d exceeded limit %d", num_matched, max_resources)
        end

        -- add features to geotable
        table.move(paged_geotable["features"], 1, #paged_geotable["features"], #geotable["features"] + 1, geotable["features"])
    end

    -- return response
    return RC_SUCCESS, geotable

end

--
-- TNM
--
local function tnm (parms, poly)

    local geotable = {
        ["type"] = "FeatureCollection",
        ["features"] = {}
    }

    -- get parameters of request
    local dataset       = ASSETS[parms["asset"]]
    local short_name    = parms["short_name"] or dataset["name"]
    local polygon       = poly or parms["poly"]
    local t0            = parms["t0"] or '2018-01-01'
    local t1            = parms["t1"] or string.format('%04d-%02d-%02d', time.gps2date(time.gps()))
    local max_resources = parms["max_resources"] or DEFAULT_MAX_REQUESTED_RESOURCES

    -- flatten polygon
    local polystr = ""
    if polygon then
        for i,coord in ipairs(polygon) do
            if i < #polygon then
                polystr = polystr .. string.format("%f %f,", coord["lon"], coord["lat"]) -- with trailing comma
            else
                polystr = polystr .. string.format("%f %f", coord["lon"], coord["lat"]) -- without trailing comma
            end
        end
    end

    -- get GPS times of t0 and t1
    local t0_gps = time.gmt2gps(string.gsub(t0, '-', ':')..':00:00:00')
    local t1_gps = time.gmt2gps(string.gsub(t1, '-', ':')..':00:00:00')

    -- scroll through requests to get items
    local num_items = 0
    while true do

        -- build query request
        local tnm_query_url = table.concat({
            "https://tnmaccess.nationalmap.gov/api/v1/products",
            string.format("?datasets=%s", urlify(short_name)),
            string.format("&polygon=%s", urlify(polystr)),
            string.format("&prodFormats=%s", "GeoTIFF"),
            string.format("&outputFormat=%s", "JSON"),
            string.format("&max=%d", TNM_PAGE_SIZE),
            string.format("&offset=%d", num_items),
            string.format("&start=%s", t0),
            string.format("&stop=%s", t1),
        })

        -- make https request
        local rsps, rsps_status = core.get(tnm_query_url, nil, {})
        if not rsps_status then
            return RC_FATAL_ERROR, "http request to tnm failed -> " .. rsps
        end

        -- build table from response
        local status, results = pcall(json.decode, rsps)
        if not status then
            return RC_RSPS_UNPARSEABLE, rsps
        end
        if not results["items"] then
            return RC_RSPS_UNEXPECTED, rsps
        end

        -- then build the geojson from the response
        for _,item in ipairs(results["items"]) do
            -- filter on time
            local feature_gps = time.gmt2gps(item["lastUpdated"])
            if feature_gps < t0_gps or feature_gps > t1_gps then
                goto continue
            end
            -- build feature
            local minX = item["boundingBox"]["minX"]
            local maxX = item["boundingBox"]["maxX"]
            local minY = item["boundingBox"]["minY"]
            local maxY = item["boundingBox"]["maxY"]
            local feature = {
                ["type"] = "Feature",
                ["id"] = item["sourceId"],
                ["geometry"] = {
                    ["type"] = "Polygon",
                    ["coordinates"] = {{{minX, minY}, {maxX, minY}, {maxX, maxY}, {minX, maxY}, {minX, minY}}}
                },
                ["properties"] = {
                    ["datetime"] = string.format("%04d-%02d-%02dT%02d:%02d:%02d.%03d+00:00", time.gps2date(feature_gps)),
                    ["url"] = item["urls"]["TIFF"]
                }
            }
            -- add feature
            table.insert(geotable["features"], feature)
            ::continue::
        end

        -- check if complete
        num_items = num_items + #results["items"]
        if num_items == results["total"] then
            break
        elseif num_items >= max_resources then
            sys.log(core.WARNING, string.format("Number of matched resources truncated from %d to %d", results["total"], num_items))
            break
        end
    end

    -- return geotable
    return RC_SUCCESS, geotable

end

--
-- search
--
local function search (parms, _poly)
    local dataset       = DATASETS[parms["short_name"]] or ASSETS[parms["asset"]]
    local api           = parms["api"] or dataset["api"]
    local handlers      = { ams=ams, cmr=cmr, stac=stac, tnm=tnm }
    local handler       = handlers[api]
    local tolerances    = { 0.0001, 0.001, 0.01, 0.1, 1.0 }
    local poly          = _poly or parms["poly"]
    local max_attempts  = (poly and __geo__) and (#tolerances + 1) or 1
    local attempt       = 1
    while handler do
        local rc, rsps = handler(parms, poly)
        if rc ~= RC_RQST_FAILED then -- success/no retries
            return rc, rsps
        elseif attempt >= max_attempts then -- failure
            return rc, string.format("earthdata query failed after %d attempts", attempt)
        else -- simplify and retry (RC_RQST_FAILED)
            sys.log(core.WARNING, string.format("%s: simplifying polygon with tolerance of %f", rsps, tolerances[attempt]))
            poly = geo.simplify(poly, tolerances[attempt], tolerances[attempt])
            poly = clean_polygon(poly)
            if poly == nil then
                return RC_INVALID_GEOMETRY, string.format("invalid polygon")
            end
            attempt = attempt + 1
        end
    end
    return RC_UNSUPPORTED, string.format("unsupported api <%s>", api)
end

--
-- Return Package --
--
return {
    load = load,
    ams = ams,
    cmr = cmr,
    stac = stac,
    tnm = tnm,
    search = search,
    SUCCESS = RC_SUCCESS,
    FATAL_ERROR = RC_FATAL_ERROR,
    RQST_FAILED = RC_RQST_FAILED,
    RSPS_UNPARSEABLE = RC_RSPS_UNPARSEABLE,
    RSPS_UNEXPECTED = RC_RSPS_UNEXPECTED,
    RSPS_TRUNCATED = RC_RSPS_TRUNCATED,
    UNSUPPORTED = RC_UNSUPPORTED,
    INVALID_GEOMETRY = RC_INVALID_GEOMETRY
}
