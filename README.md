# libblackmagic

This library provides an OO-ish interface to the [Blackmagic DeckLink](https://www.blackmagicdesign.com/products/decklink) SDI interface cards.

The library build is coordinated with [fips](https://github.com/floooh/fips).

This library has a number of dependencies.  Most of them are handled by fips, but the [Blackmagic API](https://www.blackmagicdesign.com/developer/product/capture-and-playback) must be downloaded separately.  The file [cmake/FindBlackmagicSDK.cmake](cmake/FindBlackmagicSDK.cmake) searches for the API ... if needed, the environment variable `BLACKMAGIC_DIR` can be provided as hint.

__We are currently building against Blackmagic API version 11.0__

To build:

    ./fips fetch
    ./fips gen
    ./fips build

In general, this can be shortened to:

    ./fips build


This package builds a single binary, `bm_viewer` which can display video and control the cameras.


# License

This code is released under the [MIT License](LICENSE).
