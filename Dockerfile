FROM centos:latest as apps-builder
LABEL maintainer="micjoyce <micjoyce90@gmail.com>"

RUN yum install -y cmake gcc gcc-c++ gdb make nss curl libcurl

WORKDIR /home

COPY . /home/sisdb

RUN cd /home/sisdb && ls && make all

FROM centos:latest
LABEL maintainer="micjoyce <micjoyce90@gmail.com>"

COPY --from=apps-builder /home/sisdb /home/sisdb
WORKDIR /home/sisdb/bin

EXPOSE 7329
CMD ["./sisdb-server"]
