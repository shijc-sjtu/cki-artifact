FROM spirals/parsec-3.0:source as parsec-source

FROM ubuntu:22.04 as parsec-builder
COPY --from=parsec-source /home/parsec-3.0 /usr/src/parsec-3.0
RUN apt-get -y update
RUN apt-get -y install build-essential perl
COPY gcc.bldconf /usr/src/parsec-3.0/config/gcc.bldconf
RUN cd /usr/src/parsec-3.0/ && bin/parsecmgmt -a build -p freqmine
RUN cd /usr/src/parsec-3.0/pkgs/apps/freqmine/inputs/ && tar xf input_simlarge.tar.xz

FROM alpine:3.17
COPY --from=parsec-builder /usr/src/parsec-3.0/pkgs/apps/freqmine/inst/amd64-linux.gcc/bin/freqmine /bin/parsec/freqmine
COPY --from=parsec-builder /usr/src/parsec-3.0/pkgs/apps/freqmine/inputs/kosarak_990k.dat /var/parsec/freqmine/kosarak_990k.dat
COPY command /bin/command-default
