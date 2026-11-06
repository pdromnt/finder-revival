FROM node:24.22.1-trixie-slim
RUN apt-get update && apt-get install -y --no-install-recommends python3 e2fsprogs mtools \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /work
