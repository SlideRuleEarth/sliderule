# docs

SlideRule Earth documentation.


## Building the SlideRule Website

The SlideRule **website** can be built and hosted locally for development purposes.


### Linux Build Prerequisites for building on local host

1. Sphinx

    See https://www.sphinx-doc.org/en/master/usage/installation.html for more details.

    ```bash
    $ pip install -U Sphinx docutils==0.16 sphinx_markdown_tables sphinx_panels sphinx_rtd_theme
    ```

    Note: docutils version 0.17.x breaks certain formatting in Sphinx (e.g. lists).  Therefore it is recommended that docutils version 0.16 be installed.

2. Jekyll

    See https://jekyllrb.com/docs/installation/ for more details.
    For Linux:
    ```bash
    $ sudo apt-get install ruby-full build-essential zlib1g-dev
    $ echo 'export GEM_HOME="$HOME/gems"' >> ~/.bashrc
    $ echo 'export PATH="$HOME/gems/bin:$PATH"' >> ~/.bashrc
    $ source ~/.bashrc
    $ gem install jekyll bundler
    ```

    Then go into the `jekyll` directory and install all depedencies.

    ```bash
    $ bundle install
    ```

    In the same Python environment that you installed `Sphinx` and its dependencies, you will also need to install the following dependencies.

    ```bash
    $ pip install recommonmark
    ```

3. Docker (see [UbuntuSetup](jekyll/_howtos/UbuntuSetup.md))


### MacOSx Build Prerequisites for building on local host

1. Sphinx

    See https://www.sphinx-doc.org/en/master/usage/installation.html for more details.

    ```bash
    $ pip install -U Sphinx docutils==0.16 sphinx_markdown_tables sphinx_panels sphinx_rtd_theme
    ```

    Note: docutils version 0.17.x breaks certain formatting in Sphinx (e.g. lists).  Therefore it is recommended that docutils version 0.16 be installed.
2. chruby using [Homebrew](https://brew.sh/) (install if neccessary)

    ```bash
    $ brew install chruby ruby-install
    ```
3. Jekyll using Homebrew

    See https://jekyllrb.com/docs/installation/macos/ for more details.

    ```bash
    $ brew install chruby ruby-install
    ```
    update .zshrc for chruby (NOTE: replace 'ruby-3.1.2' with the current version you installed):
    ```bash
    $ echo "source $(brew --prefix)/opt/chruby/share/chruby/chruby.sh" >> ~/.zshrc
    $ echo "source $(brew --prefix)/opt/chruby/share/chruby/auto.sh" >> ~/.zshrc
    $ echo "chruby ruby-3.1.2" >> ~/.zshrc
    ```

    Then install `jekyll`

    ```bash
    $ gem install jekyll
    ```

    Then go into the `jekyll` directory and install all depedencies.

    ```bash
    $ bundle install
    ```
    In the same Python environment that you installed `Sphinx` and its dependencies, you will also need to install the following dependencies.

    ```bash
    $ pip install recommonmark
    ```

4. Docker (see [Install Docker on Mac](https://docs.docker.com/desktop/mac/install/) )


### Build Instructions for local host

To build, in the slideruleearth-aws target directory:
```bash
$ make website
```

To run locally (exposed as http://localhost:4000) in the root of the repository:
```bash
$ make run
```


### Build Instructions for docker container

To build a fully contained docker container, in the root of the repository:
```bash
$ make website-docker
```
