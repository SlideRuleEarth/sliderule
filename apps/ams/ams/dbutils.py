import math

#
# Private: Check State
#
def __check(state):
    if type(state) == str:
        return state
    elif type(state) == dict:
        if not state['WHERE']:
            state['WHERE'] = True
            return 'WHERE'
        else:
            return 'AND'
    else:
        return ''

#
# Build Polygon Query
#
def build_polygon_query(clause, parms):
    poly = parms.get("poly")
    if poly != None:
        coords = [f'{coord["lon"]} {coord["lat"]}' for coord in poly]
        poly = f"POLYGON(({','.join(coords)}))"
        return f"{__check(clause)} ST_Intersects(geometry, ST_GeomFromText('{poly}'))"
    else:
        return ''

#
# Build Radius Query
#
def build_radius_query(clause, parms):
    lon = parms.get("lon")
    lat = parms.get("lat")
    r = parms.get("r")
    u = parms.get("u", "km")
    if lon is None or lat is None or r is None:
        return ''
    # coerce to float (guards against SQL injection via these fields)
    lon = float(lon)
    lat = float(lat)
    r = float(r)
    # convert radius to meters
    if u in ("km", "kilometers"):
        radius_m = r * 1000.0
    elif u in ("mi", "miles"):
        radius_m = r * 1609.344
    else:
        raise ValueError(f"unsupported radius units: {u}")
    # Explicit haversine great-circle distance (meters) using ST_X (lon) and ST_Y (lat)
    R_EARTH = 6371008.8  # mean earth radius (m)
    haversine = (
        f"2 * {R_EARTH} * asin(sqrt("
        f"pow(sin(radians((ST_Y(geometry) - {lat}) / 2.0)), 2) "
        f"+ cos(radians({lat})) * cos(radians(ST_Y(geometry))) "
        f"* pow(sin(radians((ST_X(geometry) - {lon}) / 2.0)), 2)"
        f"))"
    )
    return f"{__check(clause)} {haversine} <= {radius_m}"

#
# Get ICESat-2 Name Filter
#
def get_icesat2_name_filter(parms):
    if "name_filter" in parms:
        return parms["name_filter"]
    else:
        granule_parms = parms.get("granule") or {}
        rgt = parms.get("rgt") or granule_parms.get("rgt")
        cycle = parms.get("cycle") or granule_parms.get("cycle")
        region = parms.get("region") or granule_parms.get("region")
        if (rgt != None) or (cycle != None) or (region != None):
            rgt_filter = rgt and f"{rgt:04d}" or "____"
            cycle_filter = cycle and f"{cycle:02d}" or "__"
            region_filter = region and f"{region:02d}" or "__"
            return f"%$_{rgt_filter}{cycle_filter}{region_filter}$_%"
        else:
            return None

#
# Build Name Filter
#
def build_name_filter(clause, name_filter):
    if name_filter != None:
        return f"{__check(clause)} granule LIKE '{name_filter}' ESCAPE '$'"
    else:
        return ''

#
# Build Time Query
#
def build_time_query(clause, parms, time_field="begin_time"):
    t0 = parms.get('t0')
    t1 = parms.get('t1')
    if t0 != None and t1 != None:
        return f"{__check(clause)} {time_field} BETWEEN '{t0}' AND '{t1}'"
    elif t0 != None:
        return f"{__check(clause)} {time_field} >= '{t0}'"
    elif t1 != None:
        return f"{__check(clause)} {time_field} <= '{t1}'"
    else:
        return ''

#
# Build Range Query
#
def build_range_query(clause, parms, db_field, r0_field, r1_field):
    r0 = parms.get(r0_field)
    r1 = parms.get(r1_field)
    if r0 != None and r1 != None:
        return f"{__check(clause)} {db_field} BETWEEN {r0} AND {r1}"
    elif r0 != None:
        return f'{__check(clause)} {db_field} >= {r0}'
    elif r1 != None:
        return f'{__check(clause)} {db_field} <= {r1}'
    else:
        return ''

#
# Build Matching Query
#
def build_matching_query(clause, parms, db_field, m_field):
    m = parms.get(m_field)
    if m != None:
        return f'{__check(clause)} {db_field} == {m}'
    else:
        return ''
