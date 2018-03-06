# A Docker image for testing in a CI environment.

FROM debian

RUN apt-get update
RUN apt-get install -y build-essential git libsdl2-dev meson texinfo
WORKDIR /root/chip8
COPY ./ ./
