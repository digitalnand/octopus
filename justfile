setup:
    meson setup bin

build:
    meson compile -C bin

run:
    bin/octop
