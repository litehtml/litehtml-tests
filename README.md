# Tests for litehml

This repository contains tests for [litehtml](https://github.com/litehtml/litehtml).

Rendering tests depend on running environment. Currently tested and supported Linux distro: Ubuntu 24.04, Fedora 40.
The best way to run tests is using docker image. We've prepared Dockerfile to create the docker image, or use the
script ```run_render_tests.sh```. This script creates the docker image, build tests and run them.

## Running tests in the docker container

To run tests in the docker container run the script ```run_render_tests.sh```:

```shell
./run_render_tests.sh /path/to/litehtml] 4
```

Arguments:

1. Real path to the litehtml cloned repository
2. Number of threads to use for ```make``` and ```ctest``` commands

Script will create the docker image and run building and testing inside container.

## Running test locally

### Pre-requirements

To build tests locally the following libraries are required: gdk-3.0 cairo pango pangocairo fontconfig
If you are using Ubuntu runs command to install required packages:

```shell
apt-get install -yq --no-install-recommends --no-install-suggests libcairo2-dev libpango1.0-dev libgtk-3-dev
```

### Build tests

To build tests locally run commands:

```shell
mkdir build
cd build
cmake -DLITEHTML_PATH=/path/to/litehtml ../
make
ctest
```
Replace ```/path/to/litehtml``` with real path to the cloned

### Build and run tests from the litehtml repository

It is possible to run tests from [litehtml](https://github.com/litehtml/litehtml). Commands:

```shell
mkdir build
cd build
cmake -DLITEHTML_BUILD_TESTING=ON ../
make
ctest --test-dir litehtml-tests-build
```

**Note**: litehtml uses only predefined ```litehtml-tests``` commit
