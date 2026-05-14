#!/usr/bin/env python3
"""
Download one sample HDF5 granule per ICESat-2 product (ATL03, ATL06, ATL08, ATL13)
at version 007 for use with enumerate_h5_fields.py.

Searches for the smallest available granule per product to minimize download time.

Usage:
    python3 scripts/download_h5_granules.py [--output-dir ./granules]

Requirements:
    pip install earthaccess

After downloading, run:
    python3 scripts/enumerate_h5_fields.py \
        --atl03 granules/ATL03_*.h5 \
        --atl06 granules/ATL06_*.h5 \
        --atl08 granules/ATL08_*.h5 \
        --atl13 granules/ATL13_*.h5 \
        --output-dir ./schema_fields/
"""

import argparse
import sys
from pathlib import Path

try:
    import earthaccess
except ImportError:
    print("Required: pip install earthaccess")
    sys.exit(1)

PRODUCTS = ["ATL03", "ATL06", "ATL08", "ATL09", "ATL13"]
VERSION = "007"

# Search a wider window to find small granules, then pick the smallest
TEMPORAL = ("2022-06-01", "2022-07-01")
SEARCH_COUNT = 50  # number of candidates to consider per product


def get_granule_size(result):
    """Extract file size in bytes from earthaccess granule metadata."""
    try:
        for item in result['umm']['DataGranule']['ArchiveAndDistributionInformation']:
            if item.get('SizeUnit', '').startswith('MB'):
                return item.get('Size', float('inf')) * 1e6
            if item.get('SizeUnit', '').startswith('GB'):
                return item.get('Size', float('inf')) * 1e9
            if 'Size' in item:
                return item['Size']
    except (KeyError, TypeError):
        pass
    return float('inf')


def format_size(size_bytes):
    """Format byte count for display."""
    if size_bytes >= 1e9:
        return f"{size_bytes / 1e9:.1f} GB"
    return f"{size_bytes / 1e6:.0f} MB"


def main():
    parser = argparse.ArgumentParser(
        description="Download sample ICESat-2 v007 granules for field enumeration"
    )
    parser.add_argument(
        "--output-dir", default="./granules",
        help="Directory to save downloaded files (default: ./granules)"
    )
    args = parser.parse_args()

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    earthaccess.login()

    for product in PRODUCTS:
        print(f"\nSearching for {product} v{VERSION} ({SEARCH_COUNT} candidates)...")

        results = earthaccess.search_data(
            short_name=product,
            version=VERSION,
            temporal=TEMPORAL,
            count=SEARCH_COUNT,
        )

        if not results:
            print(f"  No granules found for {product} v{VERSION}")
            continue

        # Sort by file size and pick the smallest
        results.sort(key=get_granule_size)
        smallest = results[0]
        size = get_granule_size(smallest)

        print(f"  Found {len(results)} granules, smallest: {format_size(size)}")
        print(f"  Granule: {smallest['umm']['GranuleUR']}")
        print(f"  Downloading to {output_dir}/...")
        files = earthaccess.download([smallest], str(output_dir))

        if files:
            print(f"  Saved: {files[0]}")
        else:
            print(f"  Download failed for {product}")

    print(f"\nDone. Run enumerate_h5_fields.py with the downloaded files:")
    print(f"  python3 scripts/enumerate_h5_fields.py \\")
    print(f"    --atl03 {output_dir}/ATL03_*.h5 \\")
    print(f"    --atl06 {output_dir}/ATL06_*.h5 \\")
    print(f"    --atl08 {output_dir}/ATL08_*.h5 \\")
    print(f"    --atl09 {output_dir}/ATL09_*.h5 \\")
    print(f"    --atl13 {output_dir}/ATL13_*.h5 \\")
    print(f"    --output-dir ./schema_fields/")


if __name__ == "__main__":
    main()
