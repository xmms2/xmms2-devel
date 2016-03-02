#!/bin/bash

set -xe

trap exit ERR

DEBIAN_PKGS=(
    "libavahi-client-dev"
    "libavahi-glib-dev"
    "libcdio-cdda-dev"
    "libdiscid0-dev"
    "libmpg123-dev"
    "libopusfile-dev"
)

MACPORTS_PKGS=(
    "boost"
    "cunit"
    "curl"
    "cython_select"
    "expat"
    "faad2"
    # "ffmpeg" too slow to build
    "fftw-3"
    "flac"
    "fluidsynth"
    "glib2"
    "jack"
    "libao"
    "libcdio"
    "libdiscid"
    "libmad"
    "libmms"
    "libmodplug"
    "libmpcdec"
    # "libofa" broken package
    "libsamplerate"
    "libsdl"
    "libshout"
    "libsndfile"
    "libvorbis"
    "libxml2"
    "mpg123"
    "mpg123"
    "openssl"
    "opusfile"
    "pkgconfig"
    "py27-cython"
    "readline"
    "samba3"
    # "sidplay" pulls in llvm, and in the end fails to build
    "sqlite3"
    "wavpack"
)

function upload_coverage {
    success=0
    for backoff in 0 5 25 30 60 120 180; do
        if (( $backoff > 0 )); then
            echo "Upload of coverage metrics failed, retry in $backoff seconds..."
            sleep $backoff
        fi
        if coveralls -x .c -b build-coverage -i src/xmms -i src/lib -e src/lib/s4/src/tools -E '.*test.*' --gcov-options '\-lp'; then
            success=1
            break
        fi
    done

    if (( $success != 1 )); then
        exit 1
    fi
}

function linux_install {
    sudo apt-get -yq --no-install-suggests --no-install-recommends --force-yes install ${DEBIAN_PKGS[*]}
    sudo pip install git+git://github.com/dsvensson/cpp-coveralls.git
}

function linux_build_regular {
    export PATH=/usr/bin:$PATH

    ./waf configure -o build-regular-$CC --prefix=/usr
    ./waf build
    ./waf install --destdir=destdir-regular-$CC
}

function linux_build_coverage {
    export PATH=/usr/bin:$PATH

    ./waf configure -o build-coverage  --prefix=/usr --without-optionals=s4 --enable-gcov --generate-coverage
    ./waf build --generate-coverage --alltests

    upload_coverage
}

function darwin_install {
    filename=MacPorts-2.3.4-10.11-ElCapitan.pkg
    curl -O "https://distfiles.macports.org/MacPorts/$filename"
    sudo installer -pkg $filename -target /

    export PATH=/opt/local/bin:$PATH
    export COLUMNS=2048

    sudo port selfupdate
    for pkg in "${MACPORTS_PKGS[@]}"
    do
        sudo port install --no-rev-upgrade $pkg
    done

    sudo port select --set python python27
    sudo port select --set cython cython27
}

function darwin_build_regular {
    export PATH=/opt/local/bin:$PATH

    ./waf configure --conf-prefix=/opt/local -o build-regular-$CC --prefix=/usr
    ./waf build
    ./waf install --destdir=destdir-regular-$CC
}

case "$1" in
    "install")
        case "$TRAVIS_OS_NAME" in
            "osx")
                darwin_install ;;
            "linux")
                linux_install ;;
            *)
                echo "ERROR: No install target for '$TRAVIS_OS_NAME'"
                exit 1 ;;
        esac ;;
    "build")
        case "$BUILD_MODE" in
            "regular")
                case "$TRAVIS_OS_NAME" in
                    "osx")
                        darwin_build_regular ;;
                    "linux")
                        linux_build_regular ;;
                    *)
                        echo "ERROR: No build:regular target for '$TRAVIS_OS_NAME'"
                        exit 1 ;;
                esac ;;
            "coverage")
                linux_build_coverage ;;
            *)
                echo "ERROR: No build:coverage target for '$TARGET_OS_NAME'"
                exit 1 ;;
        esac ;;
    *)
        echo "ERROR: Unknown command '$1'"
        exit 1 ;;
esac

exit 0
