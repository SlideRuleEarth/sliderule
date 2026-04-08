#!/usr/bin/env python3
"""
Generate .columns.h files from JSON schema definitions.

Reads each JSON schema file in schemas/ and generates a C++ header with
FieldColumn and FieldElement declarations that can be #include'd by the
DataFrame .h file.

Also produces an index.json mapping api names to descriptions for Lua endpoints.

Usage:
    python3 scripts/generate_columns.py [--output-dir BUILD_DIR]

If --output-dir is not specified, generates files alongside the JSON schemas.
"""

import json
import sys
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCHEMAS_DIR = ROOT / "schemas"

# Map JSON type strings to C++ FieldColumn template types
# Types that are just C++ primitives pass through directly
# Complex types (FieldArray, FieldList) pass through as-is
PRIMITIVE_TYPES = {
    "bool", "int8_t", "int16_t", "int32_t", "int64_t",
    "uint8_t", "uint16_t", "uint32_t", "uint64_t",
    "float", "double", "time8_t", "string",
}

# Map flag strings to C++ Field:: constants
FLAG_MAP = {
    "TIME_COLUMN": "Field::TIME_COLUMN",
    "X_COLUMN":    "Field::X_COLUMN",
    "Y_COLUMN":    "Field::Y_COLUMN",
    "Z_COLUMN":    "Field::Z_COLUMN",
    "META_COLUMN": "Field::META_COLUMN",
}


def generate_column_decl(col):
    """Generate a FieldColumn declaration from a column dict."""
    name = col["name"]
    ctype = col["type"]
    desc = col["desc"]
    flags = col.get("flags")

    # Determine the template type
    if ctype in PRIMITIVE_TYPES:
        template = f"FieldColumn<{ctype}>"
    else:
        # Complex types like FieldArray<float,NUM_CANOPY_METRICS> or FieldList<float>
        template = f"FieldColumn<{ctype}>"

    # Determine the initializer
    if flags and flags in FLAG_MAP:
        init = f'{{{FLAG_MAP[flags]}, 0, "{desc}"}}'
    else:
        init = f'{{"{desc}"}}'

    # Pad for alignment
    decl_part = f"{template:<48s} {name}"
    return f"        {decl_part:<68s} {init};"


def generate_metadata_decl(meta):
    """Generate a FieldElement declaration from a metadata dict."""
    name = meta["name"]
    ctype = meta["type"]
    desc = meta["desc"]

    template = f"FieldElement<{ctype}>"
    init = f'{{0, Field::META_COLUMN, "{desc}"}}'

    decl_part = f"{template:<48s} {name}"
    return f"        {decl_part:<68s} {init};"


def generate_staged_decl(col):
    """Generate a FieldColumn declaration for a staged column."""
    # Same as regular column but these are conditionally added via addColumn()
    return generate_column_decl(col)


def generate_columns_header(schema, api_name):
    """Generate the full .columns.h content."""
    inherits_metadata = schema.get("inherits_metadata", False)

    lines = []
    lines.append(f"/* AUTO-GENERATED from schemas/{api_name}.json — DO NOT EDIT */")
    lines.append("")

    # Fixed columns
    if schema.get("columns"):
        lines.append("        /* DataFrame Columns */")
        for col in schema["columns"]:
            lines.append(generate_column_decl(col))

    # Staged columns (member declarations — addColumn calls are hand-written)
    if schema.get("staged"):
        lines.append("")
        lines.append("        /* Staged Columns (conditionally added via addColumn) */")
        for stage in schema["staged"]:
            for col in stage["columns"]:
                lines.append(generate_staged_decl(col))

    # Metadata (skip if inherited from base class)
    if not inherits_metadata:
        lines.append("")
        lines.append("        /* DataFrame MetaData */")
        for meta in schema.get("metadata", []):
            lines.append(generate_metadata_decl(meta))
        # Always add granule (META_SOURCE_ID, no description needed)
        lines.append(f"        {'FieldElement<string>':<48s} {'granule':<68s};")

    lines.append("")
    return "\n".join(lines)


def main():
    output_dir = None
    if "--output-dir" in sys.argv:
        idx = sys.argv.index("--output-dir")
        output_dir = Path(sys.argv[idx + 1])
        output_dir.mkdir(parents=True, exist_ok=True)

    schema_files = sorted(SCHEMAS_DIR.glob("*.json"))
    if not schema_files:
        print(f"No schema files found in {SCHEMAS_DIR}", file=sys.stderr)
        sys.exit(1)

    index = {}
    generated_count = 0

    for schema_file in schema_files:
        with open(schema_file) as f:
            schema = json.load(f)

        api_name = schema["api"]
        index[api_name] = schema["description"]

        # Skip code generation for schemas marked codegen=false
        # (e.g. FrameRunner schemas whose columns are built dynamically)
        if not schema.get("codegen", True):
            continue

        content = generate_columns_header(schema, api_name)

        if output_dir:
            out_file = output_dir / f"{api_name}.columns.h"
        else:
            out_file = SCHEMAS_DIR / f"{api_name}.columns.h"

        # Only write if changed (avoid unnecessary rebuilds)
        if out_file.exists() and out_file.read_text() == content:
            continue

        out_file.write_text(content)
        print(f"  Generated {out_file.name}")
        generated_count += 1

    # Write index.json (api_name -> description mapping for Lua endpoints)
    index_path = (output_dir or SCHEMAS_DIR) / "index.json"
    index_content = json.dumps(index, indent=2) + "\n"
    if not index_path.exists() or index_path.read_text() != index_content:
        index_path.write_text(index_content)
        print(f"  Generated index.json")

    print(f"Processed {len(schema_files)} schema files ({generated_count} headers generated)")


if __name__ == "__main__":
    main()
