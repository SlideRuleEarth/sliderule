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
# Get Polygon Query
#
def get_polygon_query(parms):
    poly = parms.get("poly")
    if poly != None:
        coords = [f'{coord["lon"]} {coord["lat"]}' for coord in poly]
        return f"POLYGON(({','.join(coords)}))"
    else:
        return None

#
# Build Polygon Query
#
def build_polygon_query(clause, poly):
    if poly != None:
        return f"{__check(clause)} ST_Intersects(geometry, ST_GeomFromText('{poly}'))"
    else:
        return ''

#
# Get Name Filter
#
def get_name_filter(parms):
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
            return f"\\_{rgt_filter}{cycle_filter}{region_filter}\\_"
        else:
            return None

#
# Build Name Filter
#
def build_name_filter(clause, name_filter):
    if name_filter != None:
        return rf"{__check(clause)} granule LIKE '{name_filter}' ESCAPE '\\'"
    else:
        return ''

#
# Build Time Query
#
def build_time_query(clause, parms):
    t0 = parms.get('t0')
    t1 = parms.get('t1')
    if t0 != None and t1 != None:
        return f"{__check(clause)} begin_time BETWEEN '{t0}' AND '{t1}'"
    elif t0 != None:
        return f"{__check(clause)} begin_time >= '{t0}'"
    elif t1 != None:
        return f"{__check(clause)} begin_time <= '{t1}'"
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
