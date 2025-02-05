FROM alpine:3.17
RUN apk add alpine-sdk git numactl-dev readline-dev linux-headers util-linux
RUN git clone --depth=1 https://github.com/mitosis-project/vmitosis-workloads.git dmt
COPY dmt.patch dmt.patch
RUN cd dmt && git apply ../dmt.patch && rm ../dmt.patch
RUN cd dmt && make bin/bench_btree_st
RUN cd dmt && make bin/bench_xsbench_mt
RUN mv dmt/bin/* /bin
RUN rm -r dmt
RUN apk del alpine-sdk git numactl-dev readline-dev linux-headers util-linux
COPY command-btree /bin/command-btree
COPY command-xsbench /bin/command-xsbench
COPY command-btree /bin/command-default
