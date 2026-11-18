FROM debian:trixie-slim

# unlighthouse 0.17.4 is the last compatible release with Debian 13
ENV UNLIGHTHOUSE_VERSION=0.17.4

ENV DEBIAN_FRONTEND=noninteractive
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8

# Custom repo
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
	libclang-rt-dev \
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
	nodejs \
	npm \
	chromium \
	# Custom repo
	sblg \
	sonar-scanner \
	lcov2cobertura \
	&& apt-get clean && rm -rf /var/lib/apt/lists/* \
	&& useradd --create-home --shell /bin/bash builder \
	&& mkdir -p /workspace \
	&& chown builder:builder /workspace \
	&& npm install -g "unlighthouse@${UNLIGHTHOUSE_VERSION}" \
	&& npm cache clean --force

# mini_httpd lives in /usr/sbin (not on Debian's default non-root PATH).
ENV PATH="/usr/local/bin:/usr/bin:/bin:/usr/sbin"

USER builder
WORKDIR /workspace
CMD ["bash"]
