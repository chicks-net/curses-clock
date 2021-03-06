curses-clock
============

[![Open Source Love png2](https://badges.frapsoft.com/os/v2/open-source.png?v=103)](https://github.com/ellerbrock/open-source-badges/)
[![GPLv2 license](https://img.shields.io/badge/License-GPLv2-blue.svg)](https://github.com/chicks-net/curses-clock/blob/master/LICENSE)
[![Maintenance](https://img.shields.io/badge/Maintained%3F-yes-green.svg)](https://github.com/chicks-net/curses-clock/graphs/commit-activity)

A clock based on curses in C that uses fixed-sized fonts (PSF) to allow varied displays.

![screen shot of default config](/docs/clock001.png)

Usage
-----

Run `make` to generate `cclock` and then run `./cclock`.  Enjoy.

Inspirations
------------

* tablespoon's clock written with tput in shell script
* George Carlin

Build Requirements
------------------

* ncurses headers and libs (package libncurses5-dev in debian, ncurses-devel in RH)
* glib-json (package libjson-glib-dev in debian, json-glib-devel in RH)

TODO
----

* BUG: handle window resizing
* BUG: segfaults on Fedora
* check for json config in home directory, read it in
* write out default config on request
* error handling for unknown timezones

Acknowledgements
----------------
* https://github.com/talamus/rw-psf provided the best way to learn how to parse PSF that I have found yet.  Perl++  http://linuxgazette.net/91/loozzr.html is also informative.
* http://stackoverflow.com/questions/111928/is-there-a-printf-converter-to-print-in-binary-format made a good starting place for my byte_to_binary()
* http://www.gtkforums.com/viewtopic.php?f=3&t=178486 for how to get time out of glib
