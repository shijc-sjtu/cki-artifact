FROM spirals/parsec-3.0:source as parsec-source

FROM alpine:3.17 as parsec-builder
COPY --from=parsec-source /home/parsec-3.0 /usr/src/parsec-3.0
RUN apk add build-base bash tar xz
RUN cd /usr/src/parsec-3.0/ && bin/parsecmgmt -a build -p fluidanimate
RUN cd /usr/src/parsec-3.0/pkgs/apps/fluidanimate/inputs/ && tar xf input_simlarge.tar.xz

FROM alpine:3.17
RUN apk add libstdc++
COPY --from=parsec-builder /usr/src/parsec-3.0/pkgs/apps/fluidanimate/inst/amd64-linux.gcc/bin/fluidanimate /bin/parsec/fluidanimate
COPY --from=parsec-builder /usr/src/parsec-3.0/pkgs/apps/fluidanimate/inputs/in_300K.fluid /var/parsec/fluidanimate/in_300K.fluid
COPY command /bin/command-default
