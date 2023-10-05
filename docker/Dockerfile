FROM debian

RUN apt-get -y update \
  && apt-get install -y --fix-missing --no-install-recommends \
    build-essential ca-certificates \
    make cmake libz-dev \
    libsqlite3-dev sqlite3 \
    git

VOLUME /data

WORKDIR /source

RUN git clone https://github.com/kiesraad/eml2sql.git

WORKDIR /source/eml2sql

# Build emlconv
RUN cmake . && make

VOLUME /source

COPY emlconv.sh /usr/local/bin/

WORKDIR /data

ENTRYPOINT ["emlconv.sh"]
