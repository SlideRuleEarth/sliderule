## Install Mongoose Library

See https://github.com/cesanta/mongoose for the latest code and instructions.

To download the latest available release, clone the repository over github.
```bash
$ git clone https://github.com/cesanta/mongoose.git
```

Then, run the provided build and installation script provided by this package to build and install the library:
```bash
$ cd {sliderule}/plugins/mongoose
$ ./build_mongoose_lib.sh {PATH_TO_MONGOOSE_REPO} {PATH_TO_INSTALL}
```

The `PATH_TO_MONGOOSE_REPO` is the path to the git repository cloned in the first step.  The `PATH_TO_INSTALL` is the libmongoose.a library and cesanta/mongoose.h header will be installed.  The `PATH_TO_MONGOOSE_REPO` is required, but the `PATH_TO_INSTALL` is optional and defaults to ___/usr/local___.

Note that the mongoose code is distributed as a single source and header file almalgamation.  The provided build and installation script decouples the mongoose source code from the SlideRule project repository.