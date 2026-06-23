import re
import json
import urllib.request
import trafilatura

#
# Convert Jupyter Notebook to Markdown
#
def notebook_to_markdown(ipynb):
    notebook = json.loads(ipynb)
    markdown = []

    def cell_source(cell):
        source = cell.get("source", "")
        if isinstance(source, list):
            return "".join(source)
        return source

    for cell in notebook.get("cells", []):
        source = cell_source(cell).strip()
        if not source:
            continue

        if cell.get("cell_type") == "markdown":
            markdown.append(source)
        elif cell.get("cell_type") == "code":
            fence = "````" if "```" in source else "```"
            markdown.append(f"{fence}python\n{source}\n{fence}")

    return "\n\n".join(markdown)

#
# Get List of Resources
#
def resources():
    with open("resources/resources.json") as file:
        return json.load(file)

#
# Get List of Templates
#
def templates():
    with open("resources/templates.json") as file:
        return json.load(file)

#
# Read Resources
#
def read(uri):

    # get matched resource
    available_resources = resources() + templates()
    matched_resource = None
    for resource in available_resources:
        if "uri" in resource:
            if uri == resource["uri"]:
                matched_resource = resource
                break
        elif "uriTemplate" in resource:
            pattern = re.sub(r'\{(\w+)\}', r'(?P<\1>[^/]+)', re.escape(resource["uriTemplate"]))
            pattern = re.compile(f'^{pattern}$')
            if pattern.match(uri):
                matched_resource = resource
                break

    # check matched resource
    if not matched_resource:
        raise RuntimeError("Error: unable to locate resource")

    # get resource path
    resource_path = uri.split("://")[-1].split("/")

    # mcp dataset resource
    if resource_path[0] == "mcp" and resource_path[1] == "datasets":
        text = "" # TODO

    # mcp job resource
    elif resource_path[0] == "mcp" and resource_path[1] == "jobs":
        text = "" # TODO

    # mcp raw resource
    elif resource_path[0] == "mcp":
        with open(f"resources/{'/'.join(resource_path[1:])}") as file:
            text = file.read()

    # openapi resource
    elif resource_path[0] == "openapi":
        with urllib.request.urlopen(f"https://sliderule.slideruleearth.io/openapi/{'/'.join(resource_path[1:])}") as rsp:
            text = rsp.read().decode("utf-8")

    # doc/python resource
    elif resource_path[0] == "docs" and resource_path[1] == "python":
        with urllib.request.urlopen(f"https://docs.slideruleearth.io/{'/'.join(resource_path[2:])}.html") as resp:
            html = resp.read().decode("utf-8")
        text = trafilatura.extract(html, output_format="markdown")

    # example/python resource
    elif resource_path[0] == "examples" and resource_path[1] == "python":
        with urllib.request.urlopen(f"https://docs.slideruleearth.io/_static/{'/'.join(resource_path[2:])}.ipynb") as rsp:
            ipynb = rsp.read().decode("utf-8")
        text = notebook_to_markdown(ipynb)

    # unhandled resource
    else:
        raise RuntimeError("Internal error: failed to parse resource path")

    # return resource contents
    return {
        "uri": uri,
        "mimeType": matched_resource["mimeType"],
        "title": matched_resource["name"],
        "text": text
    }
