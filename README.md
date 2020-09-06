[![Gitter](https://badges.gitter.im/eteran/edb-debugger.svg)](https://gitter.im/eteran/edb-debugger?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge)
[![Build Status](https://travis-ci.org/eteran/edb-debugger.svg?branch=master)](https://travis-ci.org/eteran/edb-debugger)
[![Build status](https://ci.appveyor.com/api/projects/status/91jfbbyg075wk96q?svg=true)](https://ci.appveyor.com/project/eteran/edb-debugger)
[![License](https://img.shields.io/badge/license-GPL2-blue.svg)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
[![Paypal](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=evan%2eteran%40gmail%2ecom&lc=US&item_name=edb%2ddebugger&currency_code=USD&bn=PP%2dDonationsBF%3a68747470733a2f2f696d672e736869656c64732e696f2f62616467652f446f6e6174652d50617950616c2d677265656e2e737667%3aNonHosted)


edb is a cross platform AArch32/x86/x86-64 debugger. It was inspired by [Ollydbg](http://www.ollydbg.de/ "Ollydbg"), 
but aims to function on AArch32, x86, and x86-64 as well as multiple OS's. Linux is the 
only officially supported platform at the moment, but FreeBSD, OpenBSD, OSX and 
Windows ports are underway with varying degrees of functionality.

![Screenshot](https://raw.githubusercontent.com/wiki/eteran/edb-debugger/img/edb_interface-2019.png)

edb is available under the GPL 2 license, see the COPYING for details.

NOTE: This README now only covers the most essential documentation, for more
complete documentation see the [wiki](https://github.com/eteran/edb-debugger/wiki)


Cloning
-------

When cloning the repo, please use git's `--recursive` flag to ensure that the 
sub-modules will be properly cloned and updated to the correct versions. 
Here is an example:

`git clone --recursive https://github.com/eteran/edb-debugger.git`

Compiling
---------

Compiling edb is generally quite simple. edb currently depends on the 
following packages:

Dependency                                  | Version Required
------------------------------------------- | ----------------
[Qt](http://www.qt.io/)                     | >= 5.2
[Boost](http://boost.org) (Headers Only)    | >= 1.35
[Capstone](http://www.capstone-engine.org/) | >= 3.0
[Graphviz](http://www.graphviz.org/)        | >= 2.38.0 (Optional)

Many distributions already have packages that satisify these. The wiki contains 
examples for some popular distributions:

* https://github.com/eteran/edb-debugger/wiki/Compiling-(Fedora)
* https://github.com/eteran/edb-debugger/wiki/Compiling-(Ubuntu)
* https://github.com/eteran/edb-debugger/wiki/Compiling-(Debian)

Once you have the necessary dependencies installed, compilation is done with 
`cmake`:

### CMake

If you plan to just run edb out of the build directory, it's as simple as this:

	$ mkdir build
	$ cd build
	$ cmake ..
	$ make
	$ ./edb

If you would like to properly install edb on the system for all users, it's 
only a little different:

	$ mkdir build
	$ cd build
	$ cmake -DCMAKE_INSTALL_PREFIX=/usr/local/ ..
	$ make
	$ make install
	$ edb

Installing
----------

Basic installation is simple, you may run

	$ make install

In which case the plugins will be installed in `/usr/local/lib/edb` and the 
binaries will be installed in `/usr/local/bin/`.

![Analytics](https://www.google-analytics.com/collect?v=1&tid=UA-2958870-1&ds=web&t=pageview&dl=https%3A%2F%2Fgithub.com%2Feteran%2Fedb-debugger&dh=github.com&dp=%2Feteran%2Fedb-debugger&cid=555)
