# Stage 1: Build stage
FROM ubuntu:focal AS build
LABEL description="Build stage for cwaggle"
LABEL maintainer="Cameron Durbin"
LABEL url="https://github.com/PlatinumCD/cwaggle"

# Set up timezone
RUN echo 'Etc/UTC' > /etc/timezone && \
    ln -s /usr/share/zoneinfo/Etc/UTC /etc/localtime

# Install dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    git \
    gcc \
    g++ \
    make \
    libssl-dev \
    wget \
    ca-certificates \
    tar \
    && rm -rf /var/lib/apt/lists/*

# Environment setup for external libs
WORKDIR /tmp

# Build and install CMake for any architecture
RUN ARCH=$(uname -m) && \
    if [ "$ARCH" = "x86_64" ]; then \
        wget https://github.com/Kitware/CMake/releases/download/v3.22.6/cmake-3.22.6-linux-x86_64.sh && \
        chmod +x cmake-3.22.6-linux-x86_64.sh && \
        ./cmake-3.22.6-linux-x86_64.sh --skip-license --prefix=/usr && \
        rm cmake-3.22.6-linux-x86_64.sh; \
    else \
        wget https://github.com/Kitware/CMake/releases/download/v3.22.6/cmake-3.22.6.tar.gz && \
        tar -xzf cmake-3.22.6.tar.gz && \
        cd cmake-3.22.6 && \
        ./bootstrap && \
        make -j$(nproc) && \
        make install && \
        rm -rf cmake-3.22.6*; \
    fi

# Clone, build, and install rabbitmq-c
RUN git clone https://github.com/alanxz/rabbitmq-c.git && \
    cd /tmp/rabbitmq-c && \
    mkdir build && cd build && \
    cmake -DCMAKE_INSTALL_PREFIX=/build/rabbitmq-c .. && \
    cmake --build . --config Release --target install && \
    rm -rf /tmp/rabitmq-c

# Clone, build, and install cJSON
RUN git clone https://github.com/DaveGamble/cJSON.git && \
    cd /tmp/cJSON && \
    mkdir build && cd build && \
    cmake \
        -DENABLE_CJSON_UTILS=On \
        -DENABLE_CJSON_TEST=Off \
        -DCMAKE_INSTALL_PREFIX=/build/cJSON .. && \
    make && \
    make install && \
    rm -rf /tmp/cJSON

# Clone and build the cwaggle binary
RUN git clone https://github.com/PlatinumCD/cwaggle.git && \
    cd /tmp/cwaggle && \
    mkdir build && cd build && \
    cmake \
        -DCMAKE_INSTALL_PREFIX=/build/cwaggle \
        -DRABBITMQ_DIR=/build/rabbitmq-c \
        -DCJSON_DIR=/build/cJSON \
        .. && \
    make install && \
    rm -rf /tmp/cwaggle

# Stage 2: Runtime stage
FROM ubuntu:focal AS runtime
LABEL description="Small runtime image containing cwaggle binary and external libraries"
LABEL maintainer="Cameron Durbin"
LABEL url="https://github.com/PlatinumCD/cwaggle"

# Set up timezone
RUN echo 'Etc/UTC' > /etc/timezone && \
    ln -s /usr/share/zoneinfo/Etc/UTC /etc/localtime

# Install runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc \
    nano \
    libc6-dev \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy cJSON libraries and headers
COPY --from=build /build/cJSON/lib/ /usr/lib/
COPY --from=build /build/cJSON/include/ /usr/include/

# Copy rabbitmq-c libraries and headers
COPY --from=build /build/rabbitmq-c/lib/ /usr/lib/
COPY --from=build /build/rabbitmq-c/include/ /usr/include/

# Copy cwaggle libraries and headers
COPY --from=build /build/cwaggle/lib/ /usr/lib/
COPY --from=build /build/cwaggle/include/ /usr/include/

COPY myapp.c /src/myapp.c
