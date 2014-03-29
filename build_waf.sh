#!/bin/sh

#WAF_VERSION="1.7.9"
WAF_VERSION="1.7.15"

pre_clean() {
    rm waf .waf-* -rf
}

download() {
    mkdir -p waf-tmp-build
    cd waf-tmp-build

    url="https://waf.googlecode.com/files/waf-$WAF_VERSION.tar.bz2"
    wget -q $url -O waf.bz2 || (echo ">>> Unable to download waf at $url" && exit -1)
}

unpack() {
    tar xjf waf.bz2 || (echo ">>> Unable to unpack waf.bz2 - Enough disk-space?" && exit -2)
    cd "waf-$WAF_VERSION"
}

build() {
    python waf-light configure build --tools=cython || (echo "Unable to build waf!" && exit -3)
    cp waf ../../
}

finish() {
    cd ../../ && rm waf-tmp-build -r
    echo ''
    echo ">>> Build System was set up."
    echo ">>> Now type 'waf configure build'"
}


pre_clean && download && unpack && build && finish
