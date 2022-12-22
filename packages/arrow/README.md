# Arrow Package

This package provides classses for building and using Apache Arrow in-memory and file formats.

## Install Arrow Library

To get the latest information and releases for Apache Arrow, please refer to their website: https://arrow.apache.org/.  For detailed instructions on using their cmake build system, see: https://github.com/apache/arrow/blob/master/docs/source/developers/cpp/building.rst.

To use the package manager on Ubuntu 22.04
```bash
sudo apt update
sudo apt install -y ca-certificates lsb-release wget
wget https://apache.jfrog.io/artifactory/arrow/$(lsb_release --id --short | tr 'A-Z' 'a-z')/apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb
sudo apt install -y ./apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb
sudo apt update
sudo apt install -y libarrow-dev
sudo apt install -y libparquet-dev
```

To build from source on Ubuntu 22.04
```bash
clone https://github.com/apache/arrow.git
cd arrow/cpp
mkdir build
cd build
cmake .. -DARROW_PARQUET=ON -DARROW_WITH_ZLIB=ON
make -j8
make install
```

## Notes

* The ParquetBuilder class currently supports the GeoParquet specification version v1.0.0-beta.1.  For a detailed description of the specification, see: https://geoparquet.org/releases/v1.0.0-beta.1/.
