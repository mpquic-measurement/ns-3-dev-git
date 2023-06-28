FROM ubuntu:22.04 AS ubuntu-base

ARG TARGETPLATFORM
RUN echo "TARGETPLATFORM : $TARGETPLATFORM"

RUN echo "wireshark-common wireshark-common/install-setuid boolean true" | debconf-set-selections && \
    apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y cmake sudo git wget curl python3 python3-dev python3-pip pkg-config \
                                                    cmake-format libxml2-dev libeigen3-dev nano libsqlite3-dev vim htop \
                                                    net-tools iputils-ping tcpdump ethtool iproute2 tshark gdb \
                                                    software-properties-common && \
                                    add-apt-repository ppa:longsleep/golang-backports -y && \
                                    apt update && \
                                    apt install -y golang-go

FROM ubuntu-base as ns3-base

ENV VERS 3.38
RUN wget https://www.nsnam.org/release/ns-allinone-$VERS.tar.bz2
RUN tar xjf ns-allinone-$VERS.tar.bz2 && rm ns-allinone-$VERS.tar.bz2
RUN mv /ns-allinone-$VERS/ns-$VERS /ns3

WORKDIR /ns3

RUN ./ns3 configure
RUN ./ns3 build -j$(nproc)


FROM ns3-base as builder

## make including of the QuicNetworkSimulatorHelper class possible
COPY CMakeLists.txt.patch .
RUN patch -p1 < CMakeLists.txt.patch

RUN rm -r scratch/subdir scratch/nested-subdir scratch/scratch-simulator.cc

WORKDIR /ns3/src
COPY src/quic/ns3.patch .
RUN cd /ns3 && git apply < src/ns3.patch
# RUN git clone https://github.com/mpquic-measurement/ns3-mpquic-module.git quic && \
#     cp -r quic/quic-applications/helper/. applications/helper/. && \
#     cp -r quic/quic-applications/model/. applications/model/. && \
#     mv quic/examples ../examples/quic && \
#     cd /ns3 && \
#     git apply < src/quic/ns3.patch

WORKDIR /ns3

COPY src/quic src/quic
COPY src/applications src/applications

RUN ./ns3 build -j$(nproc)

WORKDIR /

COPY setup.sh .
RUN chmod +x setup.sh

FROM builder

WORKDIR /ns3

COPY scratch/CMakeLists.txt scratch/
COPY scratch/*.cc scratch/

WORKDIR /ns3

RUN ./ns3 build -j$(nproc)

RUN mkdir /logs

