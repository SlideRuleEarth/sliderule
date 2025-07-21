# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Path setup --------------------------------------------------------------

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
#
# import os
# import sys
import datetime
# sys.path.insert(0, os.path.abspath('.'))


# -- Project information -----------------------------------------------------

project = 'sliderule'
year = datetime.date.today().year
copyright = f"2020\u2013{year}, University of Washington"
author = 'The SlideRule Developers'

# The full version, including alpha/beta/rc tags
with open('../../sliderule/clients/python/version.txt') as fh:
    release = fh.read().strip()
    version = release[1:]

rst_prolog = """
.. |LatestRelease| replace:: {releasestr}
""".format(
releasestr = release,
)



# -- General configuration ---------------------------------------------------

# Markdown Support
source_suffix = {
    '.rst': 'restructuredtext',
    '.md': 'markdown',
}

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = [
    "sphinx.ext.autodoc",
    'sphinx.ext.graphviz',
    "sphinx.ext.viewcode",
    "sphinx.ext.napoleon",
    'sphinx_markdown_tables',
    "sphinx_panels",
    "sphinx_redirects",
    "myst_parser",
    "nbsphinx"
]

myst_enable_extensions = [
    "amsmath",
    "colon_fence",  # Enables ::: note, ::: warning, etc.
    "deflist",
    "dollarmath",
]

redirects = {
    'dataframe.html': 'x_series_apis.html',
}

# Add any paths that contain templates here, relative to this directory.
templates_path = ['_templates']

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = ['**.ipynb_checkpoints']

# location of master document (by default sphinx looks for contents.rst)
master_doc = 'index'

# -- Configuration options ---------------------------------------------------
autosummary_generate = True
#jupyter_execute_notebooks = "force"

# -- Options for HTML output -------------------------------------------------

# html_title = "SlideRule"
html_short_title = "SlideRule"
html_show_sourcelink = False
html_show_sphinx = True
html_show_copyright = True

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
html_theme = 'sphinx_rtd_theme'
html_theme_options = {
    "logo_only": True,
}

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_logo = "../../jekyll/assets/images/SlideRule-blueBG.png"
html_favicon = "../../jekyll/favicon.ico"
html_static_path = ['_static']
html_css_files = ['custom.css']
repository_url = f"https://github.com/SlideRuleEarth/sliderule"
html_context = {
    "menu_links": [
        (
            '<i class="fa fa-github fa-fw"></i> Source Code',
            repository_url,
        ),
        (
            '<i class="fa fa-book fa-fw"></i> License',
            f"{repository_url}/blob/main/LICENSE",
        ),
        (
            '<i class="fas fa-link"></i> Website',
            "/",
        )
    ],
    "build_id": f' {release}',
    "build_url": "https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.0.1"
}

# Load the custom CSS files (needs sphinx >= 1.6 for this to work)
def setup(app):
    app.add_css_file("style.css")