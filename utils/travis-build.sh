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

function retry {
    success=0
    for backoff in 0 5 25 30 60 120 180; do
        if (( $backoff > 0 )); then
            echo "$2 failed, retry in $backoff seconds..."
            sleep $backoff
        fi
        if eval $1; then
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
    ./waf install --destdir=build-regular-$CC/tmp
}

function linux_build_coverage {
    export PATH=/usr/bin:$PATH

    ./waf configure -o build-coverage  --prefix=/usr --without-optionals=s4 --enable-gcov --generate-coverage
    ./waf build --generate-coverage --alltests

    function upload_coverage {
        coveralls -x .c \
                  -b build-coverage \
                  -i src/xmms \
                  -i src/lib \
                  -e src/lib/s4/src/tools \
                  -E '.*test.*' \
                  --gcov-options '\-lp'
    }

    retry "upload_coverage" "Upload of coverage metrics"
}

function linux_build_analysis {
    export PATH=/usr/bin:$PATH
    config="-analyzer-config stable-report-filename=true"

    # TODO: Should really be in install, needs to be made conditional, and really
    #       via addons: as it is for precise, see apt-source-whitelist #199.
    echo "deb http://llvm.org/apt/trusty/ llvm-toolchain-trusty-3.7 main" | sudo tee -a /etc/apt/sources.list
    sudo apt-get update -q
    sudo apt-get -yq --no-install-suggests --no-install-recommends --force-yes install clang-3.7

    scan-build-3.7 $config -o build-analysis/clang \
                   ./waf configure -o build-analysis --without-optionals=python --with-custom-version=clang-analysis

    # wipe the report from the configure phase, just need to set the tool paths.
    rm -rf build-analysis/clang

    scan-build-3.7 $config -o build-analysis/clang \
                   ./waf build --notests

    # Generate core / clientlib documentation, strip timestamps
    echo "</body></html>" > /tmp/footer.html
    (cat Doxyfile && echo "HTML_FOOTER=/tmp/footer.html") | doxygen -
    (cd src/clients/lib/xmmsclient && (cat Doxyfile && echo "HTML_FOOTER=/tmp/footer.html") | doxygen -)

    # Generate ruby bindings docs, strip timestamps
    REALLY_GEM_UPDATE_SYSTEM=1 sudo -E gem update --system
    gem install rdoc
    rdoc src/clients/lib/ruby/*.{c,rb} -o doc/ruby
    find doc/ruby \( -name 'created.rid' -or -name '*.gz' \) -delete

    if [[ -n $CI_USER_TOKEN ]]; then
        function github_docs_clone {
            git clone https://$CI_USER_TOKEN@github.com/xmms2/docs.git github-docs
        }

        retry "github_docs_clone" "Fetching xmms2/docs.git repo"

        rm -rf github-docs/clang github-docs/api

        mv build-analysis/clang/* github-docs/clang

        mkdir github-docs/api
        mv doc/xmms2/html github-docs/api/xmms2
        mv doc/xmmsclient/html github-docs/api/xmmsclient
        mv doc/ruby github-docs/api/ruby

        cd github-docs
        git add clang api
        git commit -a -m "Automatic update of API docs / Clang Static Analysis"

        function github_docs_push {
            git push
        }

        retry "github_docs_push" "Pushing to xmms2/docs.git repo"
    fi
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
    ./waf install --destdir=build-regular-$CC/tmp
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
            "analysis")
                linux_build_analysis ;;
            *)
                echo "ERROR: No build:coverage target for '$TARGET_OS_NAME'"
                exit 1 ;;
        esac ;;
    *)
        echo "ERROR: Unknown command '$1'"
        exit 1 ;;
esac

exit 0
