# CWaggle

A Waggle library for C applications.

## Building with CWaggle

Refer to the `docker/Dockerfile` for detailed instructions on the build process.

### 1. Build the Library

#### Requirements

- [CMake](https://cmake.org/)
- [cJSON](https://github.com/DaveGamble/cJSON)
- [rabbitmq-c](https://github.com/alanxz/rabbitmq-c)

```bash
mkdir build && cd build
cmake \
    -DCMAKE_INSTALL_PREFIX=<prefix> \
    -DRABBITMQ_DIR=<rabbitmq-c dir> \
    -DCJSON_DIR=<cJSON dir> \
    ..
make install
```

### 2. Build Your Application with CWaggle

```bash
gcc -o myapp myapp.c -lwaggle
```

## Docker

The Docker image, created from the `docker/Dockerfile`, is available at: [Docker Hub - plugin-cwaggle-base](https://hub.docker.com/r/platinumcd/plugin-cwaggle-base).

This image facilitates efficient development for Sage Plugins. Below is an example of how to use it:

```
FROM platinumcd/plugin-cwaggle-base:1.0.0-base

WORKDIR /app

RUN git clone https://github.com/<git_username>/<git_repo>.git && \
    cd <application> && \
    gcc myapp.c -lwaggle

ENTRYPOINT ["/app/<application>/a.out"]
```
