FROM ffinder-image-tools:local
RUN apt-get update && apt-get install -y --no-install-recommends gcc-arm-linux-gnueabi binutils-arm-linux-gnueabi libc6-dev-armel-cross \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /work
