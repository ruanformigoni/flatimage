FROM alpine:latest

# Update repositories to edge
RUN echo https://dl-cdn.alpinelinux.org/alpine/edge/main/ > /etc/apk/repositories
RUN echo https://dl-cdn.alpinelinux.org/alpine/edge/community/ >> /etc/apk/repositories
RUN echo https://dl-cdn.alpinelinux.org/alpine/edge/testing/ >> /etc/apk/repositories

# Install deps
RUN apk update && apk upgrade
RUN apk add --no-cache build-base git libbsd-dev py3-pip cmake clang clang-dev \
  make e2fsprogs-dev e2fsprogs-libs e2fsprogs-static libcom_err musl musl-dev \
  bash pcre-tools boost-dev libjpeg-turbo-dev libjpeg-turbo-static libpng-dev \
  libpng-static zlib-static

# Install conan
RUN python3 -m venv /conan
RUN . /conan/bin/activate && pip3 install conan
ENV CONAN "/conan/bin/conan"

# Copy boot directory
ARG FIM_DIR
COPY . $FIM_DIR
WORKDIR $FIM_DIR/src/boot

# Compile
RUN "$CONAN" profile detect
RUN "$CONAN" install . --build=missing -g CMakeDeps -g CMakeToolchain
RUN cmake --preset conan-release -DCMAKE_BUILD_TYPE=Release
RUN cmake --build --preset conan-release

# Compile janitor
RUN g++ --std=c++23 -O3 -static -o janitor janitor.cpp
