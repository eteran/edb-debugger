[![Build Status](https://travis-ci.org/eteran/edb-debugger.svg?branch=master)](https://travis-ci.org/eteran/edb-debugger)

edb is a cross platform x86/x86-64 debugger. It was inspired by [Ollydbg](http://www.ollydbg.de/ "Ollydbg"), but aims to function on x86 and x86-64 as well as multiple OS's. Linux is the only officially supported platform at the moment, but FreeBSD, OpenBSD, OSX and Windows ports are underway with varying degrees of functionality.

![Screenshot](http://codef00.com/img/debugger.png)

edb is available under the GPL 2 license, see the COPYING for details.

NOTE: This README now only covers the most essential documentation, for more
complete documentation see the [wiki](https://github.com/eteran/edb-debugger/wiki)


Cloning
-------

When cloning the repo, please use git's `--recursive` flag to ensure that the sub-modules will be properly cloned and updated to the correct versions. Here is an example:

`git clone --recursive git@github.com:eteran/edb-debugger.git`

Compiling
---------

Compiling edb is generally quite simple. edb currently depends on the 
following packages:

Dependency                                  | Version Required
------------------------------------------- | ----------------
[Qt](http://www.qt.io/)                     | >= 4.6
[Boost](http://boost.org) (Headers Only)    | >= 1.35
[Capstone](http://www.capstone-engine.org/) | >= 3.0

Many distributions already have packages that satisify these.

Once you have the necessary dependencies installed, there are two options for compiling:

### CMake (Recommended)

If you plan to just run edb out of the build directory, it's as simple as this:

	$ mkdir build
	$ cd build
	$ cmake ..
	$ make
	$ ./edb

If you would like to properly install edb on the system for all users, it's only a little different:

	$ mkdir build
	$ cd build
	$ cmake -DCMAKE_INSTALL_PREFIX=/usr/local/ ..
	$ make
	$ make install
	$ edb

### QMake (Planned to be phased out once cmake based building is proven reliable)

	$ qmake
	$ make

This will build the debugger along with all included plugins. On certain 
systems your qmake may be named slightly differently. For example, some distributions 
name the Qt4 version of qmake as `qmake-qt4`.

If you are planning on doing a make install, you likely want to specify the 
default plugin path, here's how you would do that.

	$ qmake -makefile DEFAULT_PLUGIN_PATH="/usr/lib/edb/"
	$ make

Also you can specify the install prefix like this:

	$ qmake PREFIX=/usr/
	$ make


Installing
----------

Basic installation is simple, you may run

	$ make install

In which case the plugins will be installed in `/usr/local/lib/edb` and the binaries
will be installed in `/usr/local/bin/` or, if you've specified `PREFIX`, then it'll be
used instead of `/usr/local/`. Finally, as described above, if you are doing a 
`make install`, you probably want to specify a default plugin path, this is done during 
the qmake process.

