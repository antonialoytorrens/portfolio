# syntax=docker/dockerfile:1
FROM debian:trixie-slim AS ci

ARG SONAR_SCANNER_VERSION=8.0.1.6346-1
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -yq --no-install-recommends \
        ca-certificates curl git make jq unzip pkgconf \
        default-jre-headless \
        gcc g++ clang clang-tidy clang-tools cppcheck gcovr valgrind bear flawfinder \
        shellcheck python3 python3-pip python3-venv python3-dev  \
    && rm -rf /var/lib/apt/lists/*

ADD https://dports.antonialoytorrens.com/aat-linux-repository/pool/main/s/sonar-scanner/sonar-scanner_8.0.1.6346-1~aatdeb13u1_all.deb /tmp/sonar-scanner.deb
RUN dpkg -i /tmp/sonar-scanner.deb || (apt-get update && apt-get install -yqf) \
    && rm -f /tmp/sonar-scanner.deb \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

COPY requirements-tools.txt /tmp/reqs-tools.txt
RUN python3 -m venv /opt/venv && /opt/venv/bin/pip install --upgrade pip && /opt/venv/bin/pip install -r /tmp/reqs-tools.txt
ENV PATH=/opt/venv/bin:$PATH

WORKDIR /workspace
CMD ["bash"]
