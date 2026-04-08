#!/usr/bin/env python3
"""
Validate GeoDataFrame schema registrations against the C++ source code.

Parses the actual DataFrame source files to extract column names from:
  - Constructor initializer lists passed to GeoDataFrame(...)
  - Conditional addColumn() calls in constructors
  - addExistingColumn() calls in FrameRunner::run() methods (PhoREAL, SurfaceFitter, etc.)
  - FieldElement metadata entries in constructor meta lists

Then parses the registerSchema() calls in plugin init files and compares.

Usage:
    python3 scripts/validate_schema.py

Exit code 0 = all schemas valid, 1 = mismatches found.
"""

import re
import sys
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# ─────────────────────────────────────────────
# Source parsers
# ─────────────────────────────────────────────

def extract_initializer_columns(cpp_text, class_name):
    """Extract column names from GeoDataFrame constructor initializer list.
    Looks for the pattern: GeoDataFrame(L, ..., { {"col_name", &var}, ... }, { {"meta_name", &var}, ... })
    """
    columns = []
    elements = []

    # Find constructor definition
    ctor_pattern = rf'{class_name}::{class_name}\s*\('
    match = re.search(ctor_pattern, cpp_text)
    if not match:
        return columns, elements

    # Find the first initializer list (columns)
    text_from_ctor = cpp_text[match.start():]

    # Extract column names from {{"name", &var}, ...} blocks
    # Find all brace-delimited initializer lists after GeoDataFrame(
    brace_lists = []
    depth = 0
    current_list_start = None
    gdf_call = re.search(r'GeoDataFrame\s*\(', text_from_ctor)
    if not gdf_call:
        # Try base class name (e.g. GediDataFrame)
        gdf_call = re.search(r'GediDataFrame\s*\(', text_from_ctor)
    if not gdf_call:
        return columns, elements

    pos = gdf_call.end()
    paren_depth = 1  # we're inside GeoDataFrame(

    while pos < len(text_from_ctor) and paren_depth > 0:
        ch = text_from_ctor[pos]
        if ch == '(':
            paren_depth += 1
        elif ch == ')':
            paren_depth -= 1
        elif ch == '{' and depth == 0:
            depth = 1
            current_list_start = pos
        elif ch == '{':
            depth += 1
        elif ch == '}' and depth == 1:
            depth = 0
            brace_lists.append(text_from_ctor[current_list_start:pos+1])
        elif ch == '}':
            depth -= 1
        pos += 1

    # First brace list = columns, second = meta/elements
    for i, blist in enumerate(brace_lists[:2]):
        names = re.findall(r'\{\s*"(\w+)"', blist)
        if i == 0:
            columns.extend(names)
        else:
            elements.extend(names)

    return columns, elements


def extract_addcolumn_calls(cpp_text, class_name):
    """Extract column names from addColumn() calls in constructor body."""
    columns = []
    # Find constructor body — it's the last top-level brace block after
    # the member initializer list. We look for the pattern where paren_depth
    # returns to 0 (end of ctor params) then find braces.
    ctor_pattern = rf'{class_name}::{class_name}\s*\('
    match = re.search(ctor_pattern, cpp_text)
    if not match:
        return columns

    text = cpp_text[match.start():]

    # Track through the constructor: skip params, skip member init list,
    # find the body (first top-level brace block where paren_depth == 0)
    paren_depth = 0
    past_params = False
    brace_depth = 0
    body_start = None
    i = 0
    while i < len(text):
        ch = text[i]
        if ch == '(' and past_params:
            paren_depth += 1
        elif ch == ')' and past_params:
            paren_depth -= 1
        elif not past_params:
            if ch == '(':
                paren_depth += 1
            elif ch == ')':
                paren_depth -= 1
                if paren_depth == 0:
                    past_params = True
                    paren_depth = 0
        if past_params and paren_depth == 0:
            if ch == '{':
                brace_depth += 1
                if brace_depth == 1:
                    body_start = i
            elif ch == '}':
                brace_depth -= 1
                if brace_depth == 0 and body_start is not None:
                    body = text[body_start:i+1]
                    for m in re.finditer(r'addColumn\s*\(\s*"(\w+)"', body):
                        columns.append(m.group(1))
                    return columns
        i += 1
    return columns


def extract_addexistingcolumn_calls(cpp_text, method_name):
    """Extract column names from addExistingColumn() in a FrameRunner::run()."""
    columns = []
    pattern = rf'{method_name}\s*\('
    match = re.search(pattern, cpp_text)
    if not match:
        return columns

    text = cpp_text[match.start():]
    # Find body
    brace_depth = 0
    body_start = 0
    for i, ch in enumerate(text):
        if ch == '{':
            brace_depth += 1
            if brace_depth == 1:
                body_start = i
        elif ch == '}':
            brace_depth -= 1
            if brace_depth == 0:
                body = text[body_start:i+1]
                for m in re.finditer(r'addExistingColumn\s*\(\s*"(\w+)"', body):
                    columns.append(m.group(1))
                break
    return columns


def extract_registered_schema(cpp_text, schema_name):
    """Extract column names from a registerSchema() call."""
    columns = []
    pattern = rf'registerSchema\s*\(\s*"{re.escape(schema_name)}"'
    match = re.search(pattern, cpp_text)
    if not match:
        return None

    # Find the brace-delimited field list
    text = cpp_text[match.start():]
    # Find the initializer list { {...}, {...}, ... }
    depth = 0
    start = None
    for i, ch in enumerate(text):
        if ch == '{' and depth == 0:
            depth = 1
            start = i
        elif ch == '{':
            depth += 1
        elif ch == '}' and depth == 1:
            block = text[start:i+1]
            for m in re.finditer(r'\{\s*"(\w[\w.{}]*)"', block):
                columns.append(m.group(1))
            return columns
        elif ch == '}':
            depth -= 1

    return columns


# ─────────────────────────────────────────────
# Schema definitions: what to check
# ─────────────────────────────────────────────

# Each entry: (schema_name, source_file, extraction_method, method_arg)
# "granule" elements and "srcid" are auto-managed (not in source column lists)
AUTO_COLUMNS = {"srcid"}
AUTO_ELEMENTS = {"granule"}

SCHEMAS = [
    {
        "name": "atl03x",
        "description": "Base atl03x columns + all conditional columns",
        "source": "datasets/icesat2/package/Atl03DataFrame.cpp",
        "extract": lambda text: (
            lambda cols, elems: (
                cols + extract_addcolumn_calls(text, "Atl03DataFrame"),
                elems
            )
        )(*extract_initializer_columns(text, "Atl03DataFrame")),
        "register_file": "datasets/icesat2/package/icesat2.cpp",
    },
    {
        "name": "atl03x-phoreal",
        "description": "PhoREAL replacement columns",
        "source": "datasets/icesat2/package/PhoReal.cpp",
        "extract": lambda text: (
            extract_addexistingcolumn_calls(text, "PhoReal::run"),
            []
        ),
        "register_file": "datasets/icesat2/package/icesat2.cpp",
        # Elements inherited from base Atl03DataFrame
        "inherited_elements": "datasets/icesat2/package/Atl03DataFrame.cpp",
        "inherited_class": "Atl03DataFrame",
    },
    {
        "name": "atl03x-surface",
        "description": "SurfaceFitter replacement columns",
        "source": "datasets/icesat2/package/SurfaceFitter.cpp",
        "extract": lambda text: (
            extract_addexistingcolumn_calls(text, "SurfaceFitter::run"),
            []
        ),
        "register_file": "datasets/icesat2/package/icesat2.cpp",
        "inherited_elements": "datasets/icesat2/package/Atl03DataFrame.cpp",
        "inherited_class": "Atl03DataFrame",
    },
    {
        "name": "atl03x-blanket",
        "description": "SurfaceBlanket replacement columns",
        "source": "datasets/icesat2/package/SurfaceBlanket.cpp",
        "extract": lambda text: (
            extract_addexistingcolumn_calls(text, "SurfaceBlanket::run"),
            []
        ),
        "register_file": "datasets/icesat2/package/icesat2.cpp",
        "inherited_elements": "datasets/icesat2/package/Atl03DataFrame.cpp",
        "inherited_class": "Atl03DataFrame",
    },
    {
        "name": "atl06x",
        "description": "ATL06 DataFrame columns",
        "source": "datasets/icesat2/package/Atl06DataFrame.cpp",
        "extract": lambda text: extract_initializer_columns(text, "Atl06DataFrame"),
        "register_file": "datasets/icesat2/package/icesat2.cpp",
    },
    {
        "name": "atl08x",
        "description": "ATL08 DataFrame columns",
        "source": "datasets/icesat2/package/Atl08DataFrame.cpp",
        "extract": lambda text: extract_initializer_columns(text, "Atl08DataFrame"),
        "register_file": "datasets/icesat2/package/icesat2.cpp",
    },
    {
        "name": "atl13x",
        "description": "ATL13 DataFrame columns",
        "source": "datasets/icesat2/package/Atl13DataFrame.cpp",
        "extract": lambda text: extract_initializer_columns(text, "Atl13DataFrame"),
        "register_file": "datasets/icesat2/package/icesat2.cpp",
    },
    {
        "name": "atl24x",
        "description": "ATL24 DataFrame columns",
        "source": "datasets/icesat2/package/Atl24DataFrame.cpp",
        "extract": lambda text: (
            lambda cols, elems: (
                cols + extract_addcolumn_calls(text, "Atl24DataFrame"),
                elems
            )
        )(*extract_initializer_columns(text, "Atl24DataFrame")),
        "register_file": "datasets/icesat2/package/icesat2.cpp",
        # These columns only appear when compact=false; OK to omit from base schema
        "optional_columns": {"ellipse_h", "invalid_kd", "invalid_wind_speed",
                             "low_confidence_flag", "night_flag",
                             "sensor_depth_exceeded", "sigma_thu", "sigma_tvu"},
    },
    {
        "name": "gedil4ax",
        "description": "GEDI L4A DataFrame columns",
        "source": "datasets/gedi/package/Gedi04aDataFrame.cpp",
        "extract": lambda text: extract_initializer_columns(text, "Gedi04aDataFrame"),
        "register_file": "datasets/gedi/package/gedi.cpp",
        "base_meta_source": "datasets/gedi/package/GediDataFrame.cpp",
        "base_meta_class": "GediDataFrame",
    },
]


def validate_schema(schema_def):
    """Validate a single schema. Returns (ok, messages)."""
    messages = []
    ok = True

    name = schema_def["name"]

    # Read source file
    source_path = ROOT / schema_def["source"]
    if not source_path.exists():
        return False, [f"  SOURCE FILE NOT FOUND: {source_path}"]
    source_text = source_path.read_text()

    # Extract columns from source
    source_columns, source_elements = schema_def["extract"](source_text)

    # For GEDI, also read the base class meta (may appear in cols or elems list
    # depending on how the base class forwards the initializer list)
    if "base_meta_source" in schema_def:
        base_path = ROOT / schema_def["base_meta_source"]
        base_text = base_path.read_text()
        base_cols, base_elems = extract_initializer_columns(base_text, schema_def["base_meta_class"])
        source_elements = base_cols + base_elems  # treat all as elements

    # For FrameRunners, elements are inherited from the base DataFrame
    if "inherited_elements" in schema_def:
        inh_path = ROOT / schema_def["inherited_elements"]
        inh_text = inh_path.read_text()
        _, inh_elements = extract_initializer_columns(inh_text, schema_def["inherited_class"])
        source_elements = inh_elements

    # Read register file
    reg_path = ROOT / schema_def["register_file"]
    reg_text = reg_path.read_text()

    # Extract registered schema
    registered = extract_registered_schema(reg_text, name)
    if registered is None:
        return False, [f"  NO registerSchema() CALL FOUND for '{name}'"]

    # Build expected set: source columns + elements - auto-managed + auto-managed
    expected_columns = set(source_columns)
    expected_elements = set(source_elements) - AUTO_ELEMENTS
    expected_all = expected_columns | expected_elements | AUTO_COLUMNS

    registered_set = set(registered)

    # Subtract known optional columns (conditional columns that may not be in base schema)
    optional = schema_def.get("optional_columns", set())

    # Compare
    missing_from_schema = expected_all - registered_set - optional
    extra_in_schema = registered_set - expected_all

    if missing_from_schema:
        ok = False
        messages.append(f"  MISSING from registerSchema (present in source): {sorted(missing_from_schema)}")

    if extra_in_schema:
        # Not necessarily an error (could be intentional), but warn
        messages.append(f"  EXTRA in registerSchema (not in source): {sorted(extra_in_schema)}")

    if ok and not messages:
        messages.append(f"  OK ({len(registered)} fields)")

    return ok, messages


def main():
    print(f"SlideRule Schema Validation")
    print(f"Root: {ROOT}\n")

    all_ok = True
    for schema_def in SCHEMAS:
        name = schema_def["name"]
        print(f"[{name}] {schema_def['description']}")
        ok, messages = validate_schema(schema_def)
        for msg in messages:
            print(msg)
        if not ok:
            all_ok = False
        print()

    # Also check: are there any FrameRunner subclasses we haven't covered?
    print("[coverage] Checking for unregistered FrameRunner subclasses...")
    runner_pattern = re.compile(r'class\s+(\w+)\s*:\s*public\s+GeoDataFrame::FrameRunner')
    registered_runners = {"PhoReal", "SurfaceFitter", "SurfaceBlanket"}  # have registerSchema entries
    non_schema_runners = {
        "DeduplicateRunner",          # infrastructure: deduplicates rows, doesn't change columns
        "Atl09Sampler",               # adds atmospheric columns to existing DataFrame
        "DataFrameSampler",           # adds raster sample columns to existing DataFrame
        "BathyRefractionCorrector",   # internal ATL24 pipeline stage
        "BathySeaSurfaceFinder",      # internal ATL24 pipeline stage
        "BathySignalStrength",        # internal ATL24 pipeline stage
        "BathyUncertaintyCalculator", # internal ATL24 pipeline stage
    }
    found_runners = set()

    for cpp_file in ROOT.rglob("*.h"):
        text = cpp_file.read_text()
        for m in runner_pattern.finditer(text):
            found_runners.add(m.group(1))

    uncovered = found_runners - registered_runners - non_schema_runners
    if uncovered:
        print(f"  WARNING: FrameRunner subclasses without schema coverage: {sorted(uncovered)}")
        print(f"  These may need registerSchema() calls if they replace DataFrame columns.")
        all_ok = False
    else:
        print(f"  OK (all {len(registered_runners)} FrameRunners covered)")

    print()
    if all_ok:
        print("PASS: All schemas validated.")
        return 0
    else:
        print("FAIL: Schema mismatches detected.")
        return 1


if __name__ == "__main__":
    sys.exit(main())
