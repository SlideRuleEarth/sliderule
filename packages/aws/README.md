## Install AWS SDK Library

The latest release can be found at https://github.com/aws/aws-sdk-cpp.  Additional instructions on how to build and install the library can be found at https://docs.aws.amazon.com/sdk-for-cpp/v1/developer-guide/aws-sdk-cpp-dg.pdf.

Due to issues with the latest version of the AWS SDK (version 1.9), version 1.8.x is used; but to get it to compile on Ubuntu 22.04, some patches to the code are needed.  Therefore, the instructions below use a fork of the AWS SDK with those patches applied.

Make sure dependencies are installed:
```bash
sudo apt install libcurl4-openssl-dev libssl3 uuid-dev zlib1g-dev
```

Clone the repo:
```bash
git clone git@github.com:ICESat2-SlideRule/aws-sdk-cpp.git
cd aws-sdk-cpp
git checkout 1.8.x-sliderule
```

Build and install the library:
```bash
mkdir build
cd build
cmake .. -DCMAKE_CXX_FLAGS="-Wno-error=deprecated-declarations" -DCMAKE_BUILD_TYPE=Release -DBUILD_ONLY="s3;transfer" -DBUILD_SHARED_LIBS=OFF -DENABLE_TESTING=OFF
make
sudo make install
```
