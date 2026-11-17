FROM debian:trixie-slim

ENV DEBIAN_FRONTEND=noninteractive
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8

# Repositori custom propi
ADD https://master.debci.antonialoytorrens.com/key.gpg \
	/etc/apt/keyrings/master.debci.antonialoytorrens.com.gpg
RUN apt-get update \
	&& apt-get install -yq --no-install-recommends ca-certificates \
	&& update-ca-certificates \
	&& chmod 644 /etc/apt/keyrings/master.debci.antonialoytorrens.com.gpg \
	&& printf 'deb [signed-by=/etc/apt/keyrings/master.debci.antonialoytorrens.com.gpg] https://master.debci.antonialoytorrens.com/debian trixie main\n' \
		> /etc/apt/sources.list.d/master.debci.antonialoytorrens.com.list \
	&& apt-get update \
	&& apt-get install -yq --no-install-recommends \
	make \
	clang \
	llvm \
	clang-tidy \
	libclang-rt-19-dev \
	pkgconf \
	libcjson-dev \
	lowdown \
	rsync \
	git \
	curl \
	libxml2-utils \
	mini-httpd \
	cppcheck \
	flawfinder \
	# Repositori custom
	sblg \
	sonar-scanner \
	lcov2cobertura \
	&& apt-get clean && rm -rf /var/lib/apt/lists/* \
	&& useradd --create-home --shell /bin/bash builder \
	&& mkdir -p /workspace \
	&& chown builder:builder /workspace

USER builder
WORKDIR /workspace
CMD ["bash"]
