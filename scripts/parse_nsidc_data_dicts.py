#!/usr/bin/env python3
"""
Extract HDF5 field names from NSIDC data dictionary PDFs for ICESat-2 products.

Downloads the v007 data dictionary PDFs from NSIDC (public, no credentials needed),
extracts the text, and parses out dataset names for the HDF5 subgroups that SlideRule
uses as ancillary fields. Outputs JSON files compatible with enumerate_h5_fields.py.

Usage:
    python3 scripts/download_h5_granules.py [--output-dir ./schema_fields]

Requirements:
    pdftotext (install via: brew install poppler  OR  apt-get install poppler-utils)
"""

import argparse
import json
import re
import subprocess
import sys
import tempfile
import urllib.request
from pathlib import Path

VERSION = "007"

NSIDC_PDF_URL = (
    "https://nsidc.org/sites/default/files/documents/"
    "technical-reference/icesat2_{product}_data_dict_v{version}.pdf"
)

# SlideRule ancillary field selectors mapped to HDF5 groups.
# "recursive" means we also collect datasets from child groups.
PRODUCT_GROUPS = {
    "ATL03": {
        "atl03_ph":     {"groups": ["/gtx/heights"],      "recursive": False,
                         "description": "Per-photon fields from ATL03 heights group."},
        "atl03_geo":    {"groups": ["/gtx/geolocation"],   "recursive": False,
                         "description": "Geolocation fields from ATL03 (per-segment)."},
        "atl03_corr":   {"groups": ["/gtx/geophys_corr"],  "recursive": False,
                         "description": "Geophysical correction fields from ATL03 (per-segment)."},
        "atl03_bckgrd": {"groups": ["/gtx/bckgrd_atlas"],  "recursive": False,
                         "description": "Background photon rate fields from ATL03."},
    },
    "ATL06": {
        "atl06": {"groups": ["/gtx/land_ice_segments"], "recursive": True,
                  "description": "Land ice segment fields from ATL06, including sub-groups."},
    },
    "ATL08": {
        "atl08": {"groups": ["/gtx/land_segments"], "recursive": True,
                  "description": "Land/vegetation segment fields from ATL08, including sub-groups."},
    },
    "ATL13": {
        "atl13": {"groups": ["/gtx"], "recursive": False,
                  "description": "Inland water surface fields from ATL13."},
    },
}


def check_pdftotext():
    """Verify pdftotext is installed."""
    try:
        subprocess.run(["pdftotext", "-v"], capture_output=True, check=False)
    except FileNotFoundError:
        print("Error: pdftotext not found.", file=sys.stderr)
        print("Install poppler:  brew install poppler  OR  apt-get install poppler-utils",
              file=sys.stderr)
        sys.exit(1)


def download_pdf(product, dest_dir):
    """Download a data dictionary PDF from NSIDC. Returns the local path."""
    url = NSIDC_PDF_URL.format(product=product.lower(), version=VERSION)
    dest = dest_dir / f"icesat2_{product.lower()}_data_dict_v{VERSION}.pdf"
    print(f"  Downloading {url}")
    urllib.request.urlretrieve(url, dest)
    return dest


def pdf_to_text(pdf_path):
    """Convert PDF to text using pdftotext -layout."""
    result = subprocess.run(
        ["pdftotext", "-layout", str(pdf_path), "-"],
        capture_output=True, text=True, check=True,
    )
    return result.stdout


def parse_groups_and_datasets(text):
    """
    Parse data dictionary text into {group_path: [dataset_dicts]}.
    Each dataset dict has 'name' and 'type' keys.
    """
    groups = {}
    current_group = None
    in_datasets = False

    for line in text.split('\n'):
        stripped = line.strip()

        # Detect group header: "Group: /some/path"
        group_match = re.match(r'\s*Group:\s*(/\S+)', stripped)
        if group_match:
            current_group = group_match.group(1)
            groups.setdefault(current_group, [])
            in_datasets = False
            continue

        # Detect numbered "Datasets" section header
        if re.match(r'^\s*\d+\.\d+(\.\d+)?\s+Datasets', stripped):
            in_datasets = True
            continue

        # Detect numbered "Attributes" section header (end of datasets)
        if re.match(r'^\s*\d+\.\d+(\.\d+)?\s+Attributes', stripped):
            in_datasets = False
            continue

        if not (in_datasets and current_group):
            continue

        # Skip header/blank/page lines
        if not stripped or stripped.startswith('Name') or stripped.startswith('Standard') or stripped.startswith('Page'):
            continue

        # Dataset line: "name   TYPE(dims)   units   description"
        ds_match = re.match(
            r'^(\w+)\s+(FLOAT|DOUBLE|INT|UINT|SHORT|BYTE|STRING|CHAR|ENUM|INTEGER)',
            stripped,
        )
        if ds_match:
            groups[current_group].append({
                "name": ds_match.group(1),
                "type": ds_match.group(2),
            })

    return groups


def collect_fields(groups, target_group, recursive):
    """Collect fields from a target group (and child groups if recursive)."""
    fields = []
    for group_path, datasets in groups.items():
        if recursive:
            if not group_path.startswith(target_group):
                continue
        else:
            if group_path != target_group:
                continue

        prefix = ""
        if group_path != target_group:
            prefix = group_path[len(target_group) + 1:] + "/"

        for ds in datasets:
            fields.append({"name": prefix + ds["name"], "type": ds["type"]})

    return sorted(fields, key=lambda x: x["name"])


def main():
    parser = argparse.ArgumentParser(
        description="Extract ICESat-2 v007 HDF5 field names from NSIDC data dictionary PDFs"
    )
    parser.add_argument(
        "--output-dir", default="./schema_fields",
        help="Output directory for JSON files (default: ./schema_fields)",
    )
    args = parser.parse_args()

    check_pdftotext()

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    all_results = {}

    with tempfile.TemporaryDirectory() as tmp:
        tmp_dir = Path(tmp)

        for product, selectors in PRODUCT_GROUPS.items():
            print(f"\n{product}:")
            pdf_path = download_pdf(product, tmp_dir)
            text = pdf_to_text(pdf_path)
            groups = parse_groups_and_datasets(text)
            print(f"  Parsed {len(groups)} HDF5 groups from data dictionary")

            for selector_name, config in selectors.items():
                all_fields = []
                for tg in config["groups"]:
                    all_fields.extend(collect_fields(groups, tg, config["recursive"]))

                result = {
                    "selector": selector_name,
                    "hdf5_subgroup": config["groups"][0],
                    "description": config["description"],
                    "product": product,
                    "version": VERSION,
                    "field_count": len(all_fields),
                    "fields": all_fields,
                }
                all_results[selector_name] = result

                # Write individual file
                individual_path = output_dir / f"fields_{selector_name}.json"
                with open(individual_path, "w") as f:
                    json.dump(result, f, indent=2)

                print(f"  {selector_name}: {len(all_fields)} fields -> {individual_path}")

    # Write combined file
    combined = {
        "description": "Available HDF5 fields for each SlideRule ancillary field selector. "
                       f"Parsed from NSIDC data dictionary PDFs (v{VERSION}).",
        "selectors": all_results,
    }
    combined_path = output_dir / "schema_icesat2_fields.json"
    with open(combined_path, "w") as f:
        json.dump(combined, f, indent=2)

    total = sum(s["field_count"] for s in all_results.values())
    print(f"\nWrote {combined_path}")
    print(f"Total: {total} fields across {len(all_results)} selectors")


if __name__ == "__main__":
    main()
