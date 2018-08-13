## OpenDLV Microservice to decode video frames from VP8 or VP9 into a shared memory

This repository provides source code to decode broadcasted video frames in
VP8 or VP9 format into a shared memory area for the OpenDLV software ecosystem.

[![License: GPLv3](https://img.shields.io/badge/license-GPL--3-blue.svg
)](https://www.gnu.org/licenses/gpl-3.0.txt)


## Table of Contents
* [Dependencies](#dependencies)
* [Usage](#usage)
* [Build from sources on the example of Ubuntu 16.04 LTS](#build-from-sources-on-the-example-of-ubuntu-1604-lts)
* [License](#license)


## Dependencies
You need a C++14-compliant compiler to compile this project.

The following dependency is part of the source distribution:
* [libcluon](https://github.com/chrberger/libcluon) - [![License: GPLv3](https://img.shields.io/badge/license-GPL--3-blue.svg
)](https://www.gnu.org/licenses/gpl-3.0.txt)

The following dependencies are downloaded and installed during the Docker-ized build:
* [libvpx 1.7.0](https://github.com/webmproject/libvpx/releases/tag/v1.7.0) - [![License: BSD 3-Clause](https://img.shields.io/badge/License-BSD%203--Clause-blue.svg)](https://opensource.org/licenses/BSD-3-Clause) - [Google Patent License Conditions](https://raw.githubusercontent.com/webmproject/libvpx/f80be22a1099b2a431c2796f529bb261064ec6b4/PATENTS)
* [libyuv](https://chromium.googlesource.com/libyuv/libyuv/+/master) - [![License: BSD 3-Clause](https://img.shields.io/badge/License-BSD%203--Clause-blue.svg)](https://opensource.org/licenses/BSD-3-Clause) - [Google Patent License Conditions](https://chromium.googlesource.com/libyuv/libyuv/+/master/PATENTS)


## Usage
To run this microservice using `docker-compose`, you can simply add the following
section to your `docker-compose.yml`:

```yml
version: '2' # Must be present exactly once at the beginning of the docker-compose.yml file
services:    # Must be present exactly once at the beginning of the docker-compose.yml file
    video-vpx-decoder:
        image: chalmersrevere/opendlv-video-vpx-decoder-multi:v0.0.5
        restart: on-failure
        network_mode: "host"
        ipc: "host"
        volumes:
        - /tmp:/tmp
        environment:
        - DISPLAY=${DISPLAY}
        command: "--cid=111 --name=imageData"
```

As this microservice is connecting to an OD4Session to receive VP8 or VP9 frames
to decode them into a shared memory area using SysV IPC, the `docker-compose.yml`
file specifies the use of `ipc:host`. The parameter `network_mode: "host"` is
necessary to receive VP8 or VP9 frames broadcasted from other microservices running
in an `OD4Session` from OpenDLV. The folder `/tmp` is shared into the Docker
container to provide tokens describing the shared memory area.
The parameters to the application are:

* `--cid=111`: Identifier of the OD4Session to listen for VP8 or VP9 frames
* `--id=2`: Optional identifier to listen only for those VP8 or VP9 frames with the matching senderStamp of the OD4Session
* `--name=XYZ`: Name of the shared memory area to create for storing the ARGB image data
* `--verbose`: Display decoding information and render the image to screen (requires X11; run `xhost +` to allow access to you X11 server)


## Build from sources on the example of Ubuntu 16.04 LTS
To build this software, you need cmake, C++14 or newer, libyuv, libvpx, and make.
Having these preconditions, just run `cmake` and `make` as follows:

```
mkdir build && cd build
cmake -D CMAKE_BUILD_TYPE=Release ..
make && make test && make install
```


## License

* This project is released under the terms of the GNU GPLv3 License

