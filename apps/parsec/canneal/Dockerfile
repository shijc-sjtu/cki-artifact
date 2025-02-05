FROM spirals/parsec-3.0:source as parsec-source

FROM ubuntu:22.04 as parsec-builder
COPY --from=parsec-source /home/parsec-3.0 /usr/src/parsec-3.0
RUN apt-get -y update
RUN apt-get -y install build-essential perl
COPY gcc.bldconf /usr/src/parsec-3.0/config/gcc.bldconf
COPY main.cpp /usr/src/parsec-3.0/pkgs/kernels/canneal/src/main.cpp
RUN cd /usr/src/parsec-3.0/ && bin/parsecmgmt -a build -p canneal
RUN cd /usr/src/parsec-3.0/pkgs/kernels/canneal/inputs/ && tar xf input_simlarge.tar.xz

FROM alpine:3.17
COPY --from=parsec-builder /usr/src/parsec-3.0/pkgs/kernels/canneal/inst/amd64-linux.gcc/bin/canneal /bin/parsec/canneal
COPY --from=parsec-builder /usr/src/parsec-3.0/pkgs/kernels/canneal/inputs/400000.nets /var/parsec/canneal/400000.nets
COPY command /bin/command-default
