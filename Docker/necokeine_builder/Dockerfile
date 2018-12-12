FROM eosio/builder as builder
ARG branch=master
ARG symbol=SYS

RUN apt-get update && DEBIAN_FRONTENT=noninteractive apt-get -y install libcurl4-openssl-dev libsoci-dev

RUN git clone -b $branch https://github.com/aws/aws-sdk-cpp.git

RUN cd aws-sdk-cpp && cmake -DCMAKE_BUILD_TYPE=Release . \
    && make && make install && cd ..
