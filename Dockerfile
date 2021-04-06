FROM gcc:latest

RUN apt-get update && apt-get -y install llvm-dev
RUN git clone https://github.com/c3d/xl.git
WORKDIR xl
RUN make install
WORKDIR ..
RUN mkdir app
WORKDIR app
ENTRYPOINT ["/usr/local/bin/xl", "-i", "/dev/stdin"]
