## Install AWS SDK Library

The latest release can be found at https://github.com/aws/aws-sdk-cpp.  Additional instructions on how to build and install the library can be found at https://docs.aws.amazon.com/sdk-for-cpp/v1/developer-guide/aws-sdk-cpp-dg.pdf.

Make sure dependencies are installed:
```bash
$ sudo apt install libcurl4-openssl-dev libssl-dev uuid-dev zlib1g-dev
```

Clone the repo:
```bash
git clone git@github.com:aws/aws-sdk-cpp.git
```

Build and install the library:
```bash
mkdir aws_sdk_build
cd aws_sdk_build
cmake ../aws-sdk-cpp -DCMAKE_BUILD_TYPE=Release
make
sudo make install
```

## Using Version 1.8

Version 1.8 of the SDK provides greater control over what is being built and is recommended for use.

Switch to the version 1.8 branch:
```bash
git checkout version1.8
```

Use the following cmake line when building the code:
```bash
cmake ../aws-sdk-cpp -DCMAKE_BUILD_TYPE=Release -DBUILD_ONLY="s3" -DBUILD_SHARED_LIBS=OFF -DENABLE_TESTING=OFF
```
