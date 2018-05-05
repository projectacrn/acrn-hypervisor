# Build container based on Ubuntu 16.04
FROM ubuntu:16.04

# Install dependencies.
RUN apt-get update \
    && apt-get install -y gcc make git \
                          gnu-efi \
                          libssl-dev \
                          libpciaccess-dev \
                          uuid-dev \
    && apt-get clean

WORKDIR '/code'
ENTRYPOINT ["make"]
CMD ["all"]
