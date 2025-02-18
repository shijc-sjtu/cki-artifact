FROM alpine:3.17
RUN apk add build-base
COPY main.c main.c
RUN gcc -o /bin/hello main.c && rm main.c
RUN apk del build-base
COPY command /bin/command-default
