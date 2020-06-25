## Install Pistache Library

See https://github.com/oktal/pistache for the latest instructions on how to install.

To download the latest available release, clone the repository over github.
```bash
$ git clone https://github.com/oktal/pistache.git
```

Then, init the submodules:
```bash
$ cd pistache
$ git submodule update --init
```

Update system packages needed to build the code:
```bash
$ sudo apt install libssl-dev
```

Now, compile and install the library:
```bash
$ mkdir -p build
$ cd build
$ cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DPISTACHE_USE_SSL=true ..
$ make
$ make install
$ sudo ldconfig
```