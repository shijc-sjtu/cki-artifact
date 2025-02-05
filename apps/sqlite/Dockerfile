FROM alpine:3.17 as builder
RUN apk add build-base git
RUN git clone --depth=1 https://mirror.ghproxy.com/https://github.com/ukontainer/sqlite-bench.git
RUN cd sqlite-bench && make

FROM alpine:3.17
COPY --from=builder /sqlite-bench/sqlite-bench /bin/sqlite-bench
COPy command /bin/command-default
