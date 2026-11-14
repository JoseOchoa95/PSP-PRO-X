#!/bin/sh
set -eu
cd "$1"
build_id=$2
archive=vendor/mbedtls-3.6.7.tar.bz2
printf '%s  %s\n' a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6 "$archive" | sha256sum -c -
mkdir -p vendor/mbedtls
tar -xjf "$archive" -C vendor/mbedtls --strip-components=1
make -C vendor/mbedtls/library -j4 CC=psp-gcc AR=psp-ar \
    CFLAGS="-Os -G0 -DMBEDTLS_CONFIG_FILE='\"prox_mbedtls_config.h\"' -I$(pwd)" \
    all
if [ "${3:-client}" = library-only ]; then exit 0; fi
make BUILD_ID="$build_id"
