#!/usr/bin/env python3
"""Generate the MyST Markdown API reference pages from the sliderule client docstrings.

Replaces Sphinx's autodoc/napoleon, which MyST has no equivalent for. Run from the
directory containing myst.yml, with the sliderule python client installed.
"""

import importlib
import inspect
import os
import re
import sys

OUTPUT_DIR = "api_reference"

# (page, module, intro, functions) -- function order matches the pre-MyST Sphinx pages
PAGES = [
    (
        "sliderule",
        "sliderule",
        "The SlideRule Python API `sliderule.py` is used to access the services provided by the base SlideRule server. From Python, the module can be imported via:",
        "import sliderule",
        ["init", "source", "set_url", "set_verbose", "set_rqst_timeout",
         "update_available_servers", "scaleout", "gps2utc", "get_version",
         "check_version", "toregion", "run"],
    ),
    (
        "icesat2",
        "sliderule.icesat2",
        "The ICESat-2 Python API `icesat2.py` is used to access the services provided by the **icesat2** plugin for SlideRule. From Python, the module can be imported via:",
        "from sliderule import icesat2",
        ["init", "atl03s", "atl03sp", "atl03v", "atl03vp", "atl06", "atl06p",
         "atl06s", "atl06sp", "atl08", "atl08p", "atl13s", "atl13sp", "atl24v"],
    ),
    (
        "gedi",
        "sliderule.gedi",
        "The GEDI Python API `gedi.py` is used to access the services provided by the **gedi** plugin for SlideRule. From Python, the module can be imported via:",
        "from sliderule import gedi",
        ["init", "gedi04a", "gedi04ap", "gedi02a", "gedi02ap", "gedi01b", "gedi01bp"],
    ),
    (
        "earthdata",
        "sliderule.earthdata",
        "The SlideRule Python API `earthdata.py` is used to access the indexing services provided institutions that maintain Earth science datasets (for example, NASA's EarthData Common Metadata Repository, and USGS's The National Map). From Python, the module can be imported via:",
        "from sliderule import earthdata",
        ["set_max_resources", "cmr", "stac", "tnm", "search"],
    ),
    (
        "h5",
        "sliderule.h5",
        "The SlideRule Python API `h5.py` is used to access the H5Coro services provided by SlideRule. From Python, the module can be imported via:",
        "from sliderule import h5",
        ["h5", "h5p", "h5x"],
    ),
    (
        "raster",
        "sliderule.raster",
        "The SlideRule Python API `raster.py` is used to sample and (in the future) subset raster datasets registered with SlideRule. See [Raster Sampling](../user_guide/raster_sampling.md) for more details. From Python, the module can be imported via:",
        "from sliderule import raster",
        ["sample", "subset"],
    ),
]

SECTION_UNDERLINE = re.compile(r"^-{3,}\s*$")


def split_sections(doc):
    """Split a numpydoc docstring into (summary, [(section_name, lines), ...])."""
    lines = doc.splitlines()
    summary, sections, current = [], [], None
    i = 0
    while i < len(lines):
        if i + 1 < len(lines) and SECTION_UNDERLINE.match(lines[i + 1]) and lines[i].strip():
            current = (lines[i].strip(), [])
            sections.append(current)
            i += 2
            continue
        (current[1] if current else summary).append(lines[i])
        i += 1
    return "\n".join(summary).strip(), sections


def dedent(lines):
    body = [ln for ln in lines if ln.strip()]
    if not body:
        return []
    pad = min(len(ln) - len(ln.lstrip()) for ln in body)
    return [ln[pad:] if ln.strip() else "" for ln in lines]


def render_parameters(lines):
    """Render `name: type` entries with indented descriptions as a Markdown list."""
    out = []
    for line in dedent(lines):
        if not line.strip():
            continue
        if not line[0].isspace() and ":" in line:
            name, _, rest = line.partition(":")
            kind = rest.strip()
            out.append(f"- **{name.strip()}**" + (f" (*{kind}*)" if kind else ""))
        elif out:
            out.append(f"  {line.strip()}")
    return out


def render_returns(lines):
    """The first unindented line is the type; everything below it is the description."""
    body = dedent(lines)
    while body and not body[0].strip():
        body.pop(0)
    if not body:
        return []
    out = [f"*{body[0].strip()}*", ""]
    out.extend(dedent(body[1:]))
    return out


def render_examples(lines):
    body = dedent(lines)
    while body and not body[0].strip():
        body.pop(0)
    while body and not body[-1].strip():
        body.pop()
    return ["```python", *body, "```"] if body else []


def render_function(module, module_name, name):
    fn = getattr(module, name)
    summary, sections = split_sections(inspect.getdoc(fn) or "")

    out = [f"## {name}", "", "```python",
           f"{module_name}.{name}{inspect.signature(fn)}", "```", ""]
    if summary:
        out.extend([summary, ""])

    for title, lines in sections:
        if title == "Parameters":
            rendered = render_parameters(lines)
        elif title == "Returns":
            rendered = render_returns(lines)
        elif title == "Examples":
            rendered = render_examples(lines)
        else:
            rendered = dedent(lines)
        if rendered:
            out.extend([f"**{title}**", "", *rendered, ""])
    return out


def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    for page, module_name, intro, import_stmt, functions in PAGES:
        module = importlib.import_module(module_name)
        lines = [f"# {page}", "", intro, "", "```python", import_stmt, "```", ""]
        for name in functions:
            lines.extend(render_function(module, module_name, name))
        path = os.path.join(OUTPUT_DIR, f"{page}.md")
        with open(path, "w") as fh:
            fh.write("\n".join(lines).rstrip() + "\n")
        print(f"generated {path} ({len(functions)} functions)")


if __name__ == "__main__":
    sys.exit(main())
