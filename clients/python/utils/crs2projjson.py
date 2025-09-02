# Copyright (c) 2021, University of Washington
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the University of Washington nor the names of its
#    contributors may be used to endorse or promote products derived from this
#    software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF WASHINGTON AND CONTRIBUTORS
# “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF WASHINGTON OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Imports
import sys
import json
import argparse
from pyproj import CRS

# Command Line Arguments
parser = argparse.ArgumentParser(description="""Convert a CRS into PROJJSON.
Takes a base CRS (EPSG:xxxx, WKT, PROJ string, or PROJJSON). If a vertical
datum (EGM08 | NAVD88) is provided, outputs a CompoundCRS (horizontal + vertical).
Result is pretty-printed PROJJSON (stdout or --outfile).""")
parser.add_argument('crs',       type=str, help='Base CRS (EPSG:xxxx, WKT, PROJ, or PROJJSON)')
parser.add_argument('datum',     type=str, nargs='?', default='NONE', help='Vertical datum: EGM08 | NAVD88 | NONE')
parser.add_argument('--outfile', type=str, default=None, help='Write output to file instead of stdout')
args,_ = parser.parse_known_args()

##############################
# Helper Functions
##############################

def _vertical_epsg_for(datum: str) -> str | None:
    key = (datum or 'NONE').strip().upper()
    if key == 'EGM08':
        return 'EPSG:3855'  # EGM2008 height
    if key == 'NAVD88':
        return 'EPSG:5703'  # NAVD88 height (metre)
    if key in ('', 'NONE', 'UNSPECIFIED'):
        return None
    raise ValueError(f"Unsupported vertical datum '{datum}'. Supported: EGM08, NAVD88, NONE")

def _to_projjson(crs: CRS) -> str:
    return json.dumps(crs.to_json_dict(), indent=2)

def _build_crs(base_text: str, datum: str, warnings: list[str]) -> CRS:
    # Parse horizontal/base CRS
    base = CRS.from_user_input(base_text)

    # No vertical datum → just return base
    v_epsg = _vertical_epsg_for(datum)
    if not v_epsg:
        return base

    # If base CRS is 3D, downgrade to 2D
    if len(base.axis_info) > 2:
        try:
            auth = base.to_authority()
            if auth == ('EPSG', '9989'):  # ITRF2020 3D → use official 2D
                downgraded = CRS.from_user_input('EPSG:9990')
            elif auth == ('EPSG', '7789'):  # ITRF2014 3D → use official 2D
                downgraded = CRS.from_user_input('EPSG:7790')
            else:
                downgraded = base.to_2d()

            base = downgraded
        except Exception:
            raise RuntimeError(
                f"Base CRS '{base_text}' is 3D and cannot be combined with a vertical CRS; "
                "try specifying the 2D equivalent directly (e.g. EPSG:9990 for ITRF2020, EPSG:7790 for ITRF2014)."
            )

    # Build vertical CRS
    vert = CRS.from_user_input(v_epsg)

    # Construct CompoundCRS
    compound_dict = {
        "type": "CompoundCRS",
        "name": f"Horizontal + {datum.upper()} height",
        "components": [
            base.to_json_dict(),
            vert.to_json_dict()
        ]
    }
    return CRS.from_json_dict(compound_dict)

##############################
# Main
##############################

def main():
    warnings: list[str] = []
    try:
        crs_obj = _build_crs(args.crs, args.datum, warnings)
        projjson_text = _to_projjson(crs_obj)

        if args.outfile:
            with open(args.outfile, 'w', encoding='utf-8') as f:
                f.write(projjson_text)
            for w in warnings:
                sys.stderr.write(f"WARNING: {w}\n")
        else:
            for w in warnings:
                sys.stderr.write(f"WARNING: {w}\n")
            print(projjson_text)

    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(2)

if __name__ == '__main__':
    main()
