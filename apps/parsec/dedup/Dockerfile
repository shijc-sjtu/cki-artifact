FROM spirals/parsec-3.0:source as parsec-source

FROM ubuntu:22.04 as parsec-builder
COPY --from=parsec-source /home/parsec-3.0 /usr/src/parsec-3.0
RUN apt-get -y update
RUN apt-get -y install build-essential perl
COPY gcc.bldconf /usr/src/parsec-3.0/config/gcc.bldconf
COPY decoder.c /usr/src/parsec-3.0/pkgs/kernels/dedup/src/decoder.c
COPY encoder.c /usr/src/parsec-3.0/pkgs/kernels/dedup/src/encoder.c
RUN cd /usr/src/parsec-3.0/ && bin/parsecmgmt -a build -p dedup
RUN cd /usr/src/parsec-3.0/pkgs/kernels/dedup/inputs/ && tar xf input_simmedium.tar.xz

FROM alpine:3.17
COPY --from=parsec-builder /usr/src/parsec-3.0/pkgs/kernels/dedup/inst/amd64-linux.gcc/bin/dedup /bin/parsec/dedup
COPY --from=parsec-builder /usr/src/parsec-3.0/pkgs/kernels/dedup/inputs/media.dat /var/parsec/dedup/media.dat
COPY command /bin/command-default
