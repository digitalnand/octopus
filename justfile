builddir := "bin/"
debugdir := "debug/"

# sets the build directory. target must be 'release' or 'debug'
setup target *FLAGS:
    #!/bin/sh
    if [ "{{target}}" != "debug" ] && [ "{{target}}" != "release" ]; then
        echo "setup: invalid target: {{target}}"
        exit
    fi

    if [[ "{{target}}" == "debug" ]]; then
        flags="{{FLAGS}} --buildtype=debug"
        dir="{{debugdir}}"
    else
        flags="{{FLAGS}} --buildtype=plain"
        dir="{{builddir}}"
    fi

    meson setup $flags $dir

# builds the project. target must be 'release' or 'debug'
build target:
    #!/bin/sh
    if [ "{{target}}" != "debug" ] && [ "{{target}}" != "release" ]; then
        echo "build: invalid target: {{target}}"
        exit
    fi

    dir="{{builddir}}"
    if [[ "{{target}}" == "debug" ]]; then
        dir="{{debugdir}}"
    fi

    meson compile -C $dir
