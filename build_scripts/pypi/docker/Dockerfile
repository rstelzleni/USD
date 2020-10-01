FROM quay.io/pypa/manylinux2014_x86_64

# openssl-devel is needed to build cmake
#
# azure pipelines requires that containers have sudo, because they're going
# to create a new user and do builds as that user inside the container. This
# has to be available before they instantiate the container, so that's why
# we build this in advance.
RUN yum install -y      \
        openssl-devel   \
        sudo            \
        wget

RUN yum clean all && \
    rm -rf /var/cache/yum/

ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en

# get a recent cmake
WORKDIR /opt
RUN wget https://cmake.org/files/v3.17/cmake-3.17.3.tar.gz && \
    tar -xzvf cmake-3.17.3.tar.gz && \
    cd cmake-3.17.3 && \
    ./bootstrap && \
    make -j$(nproc) && \
    make install

CMD bash
