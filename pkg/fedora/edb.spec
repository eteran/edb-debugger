Name:           edb
Version:        1.4.0
Release:        2%{?dist}
Summary:        A debugger based on the ptrace API and Qt

License:        GPLv2+
URL:            https://github.com/eteran/edb-debugger
Source0:        https://github.com/eteran/edb-debugger/releases/download/%{version}/edb-debugger-%{version}.tgz

BuildRequires:  gcc-c++
BuildRequires:  desktop-file-utils
BuildRequires:  libappstream-glib
BuildRequires:  qt5-qtbase-devel
BuildRequires:  qt5-qtsvg
BuildRequires:  qt5-qtsvg-devel
BuildRequires:  qt5-qtxmlpatterns
BuildRequires:  qt5-qtxmlpatterns-devel
BuildRequires:  cmake
BuildRequires:  boost-devel
BuildRequires:  capstone-devel

# as edb is an x86 debugger
ExclusiveArch:  %{ix86} x86_64

%description
edb a debugger based on the ptrace API.

One of the main goals of this debugger is modularity.
The interface is written in Qt and thus source portable to many platforms.
The debugger core is a plugin and the platform specific code is isolated
to just a few files, porting to a new OS would require porting these few
files and implementing a plugin which implements the
DebuggerCoreInterface interface. Also, because the plugins are based
on the QPlugin API, and do their work through the DebuggerCoreInterface
object, they are almost always portable with just a simple recompile.


%prep
%autosetup -n edb-debugger -p1

%build
%cmake .
make -C %{__cmake_builddir} %{?_smp_mflags}


%install
make -C %{__cmake_builddir} install DESTDIR=%{buildroot}
desktop-file-validate %{buildroot}/%{_datadir}/applications/%{name}.desktop

#install appdata file
mkdir -p %{buildroot}%{_datadir}/metainfo/
install -m 644 %{name}.appdata.xml %{buildroot}%{_datadir}/metainfo/
appstream-util validate-relax --nonet %{buildroot}%{_datadir}/metainfo/%{name}.appdata.xml

%files
%doc CHANGELOG COPYING README.md TODO
%{_bindir}/%{name}
%{_libdir}/%{name}
%{_datadir}/metainfo/%{name}.appdata.xml
%{_datadir}/applications/%{name}.desktop
%{_datadir}/pixmaps/%{name}.png
%{_mandir}/man1/%{name}.1*

%changelog
* Sun July 30 2023 Evan Teran <evan.teran@gmail.com> - 1.4.0
- Version bump

* Sun Dec 12 2021 Pekka Oinas <peoinas@gmail.com> - 1.3.0-2
- Fix building on Fedora 35

* Sun Apr 11 2021 Pekka Oinas <peoinas@gmail.com> - 1.3.0-1
- Updated to new version

* Tue Jan 28 2020 Fedora Release Engineering <releng@fedoraproject.org> - 0.9.21-5
- Rebuilt for https://fedoraproject.org/wiki/Fedora_32_Mass_Rebuild

* Wed Jul 24 2019 Fedora Release Engineering <releng@fedoraproject.org> - 0.9.21-4
- Rebuilt for https://fedoraproject.org/wiki/Fedora_31_Mass_Rebuild

* Thu Jan 31 2019 Fedora Release Engineering <releng@fedoraproject.org> - 0.9.21-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_30_Mass_Rebuild

* Thu Jul 12 2018 Fedora Release Engineering <releng@fedoraproject.org> - 0.9.21-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_29_Mass_Rebuild

* Fri Feb 16 2018 Michael Cullen <mich181189@fedoraproject.org> - 0.9.21-1
- Added GraphViz dependency for added functionality
- Updated to new version
- Changed to use Qt5 instead of Qt4
* Wed Feb 07 2018 Fedora Release Engineering <releng@fedoraproject.org> - 0.9.18-24
- Rebuilt for https://fedoraproject.org/wiki/Fedora_28_Mass_Rebuild

* Wed Aug 02 2017 Fedora Release Engineering <releng@fedoraproject.org> - 0.9.18-23
- Rebuilt for https://fedoraproject.org/wiki/Fedora_27_Binutils_Mass_Rebuild

* Wed Jul 26 2017 Fedora Release Engineering <releng@fedoraproject.org> - 0.9.18-22
- Rebuilt for https://fedoraproject.org/wiki/Fedora_27_Mass_Rebuild

* Sun Jun 18 2017 Filipe Rosset <rosset.filipe@gmail.com> - 0.9.18-21
- Spec cleanup

* Fri Feb 10 2017 Fedora Release Engineering <releng@fedoraproject.org> - 0.9.18-20
- Rebuilt for https://fedoraproject.org/wiki/Fedora_26_Mass_Rebuild

* Wed Feb 03 2016 Fedora Release Engineering <releng@fedoraproject.org> - 0.9.18-19
- Rebuilt for https://fedoraproject.org/wiki/Fedora_24_Mass_Rebuild

* Tue Feb 02 2016 Rex Dieter <rdieter@fedoraproject.org> - 0.9.18-18
- use %%qmake_qt4 macro to ensure proper build flags

* Thu Aug 27 2015 Jonathan Wakely <jwakely@redhat.com> - 0.9.18-17
- Rebuilt for Boost 1.59

* Wed Jul 29 2015 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.9.18-16
- Rebuilt for https://fedoraproject.org/wiki/Changes/F23Boost159

* Wed Jul 22 2015 David Tardon <dtardon@redhat.com> - 0.9.18-15
- rebuild for Boost 1.58

* Wed Jun 17 2015 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.9.18-14
- Rebuilt for https://fedoraproject.org/wiki/Fedora_23_Mass_Rebuild

* Sat May 02 2015 Kalev Lember <kalevlember@gmail.com> - 0.9.18-13
- Rebuilt for GCC 5 C++11 ABI change

* Thu Mar 26 2015 Richard Hughes <rhughes@redhat.com> - 0.9.18-12
- Add an AppData file for the software center

* Mon Jan 26 2015 Petr Machata <pmachata@redhat.com> - 0.9.18-11
- Rebuild for boost 1.57.0

* Sat Aug 16 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.9.18-10
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_22_Mass_Rebuild

* Sat Jun 07 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.9.18-9
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_Mass_Rebuild

* Thu May 22 2014 Petr Machata <pmachata@redhat.com> - 0.9.18-8
- Rebuild for boost 1.54.0

* Sat Aug 03 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.9.18-7
- Rebuilt for https://fedoraproject.org/wiki/Fedora_20_Mass_Rebuild

* Tue Jul 30 2013 Petr Machata <pmachata@redhat.com> - 0.9.18-6
- Rebuild for boost 1.54.0

* Wed Feb 13 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.9.18-5
- Rebuilt for https://fedoraproject.org/wiki/Fedora_19_Mass_Rebuild

* Wed Jul 18 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.9.18-4
- Rebuilt for https://fedoraproject.org/wiki/Fedora_18_Mass_Rebuild

* Tue Feb 28 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.9.18-3
- Rebuilt for c++ ABI breakage

* Fri Jan 13 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.9.18-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_17_Mass_Rebuild

* Sun Jan  1 2012 Nicoleau Fabien <nicoleau.fabien@gmail.com> - 0.9.18-1
- Update to 0.9.18
* Sat Apr 23 2011 Nicoleau Fabien <nicoleau.fabien@gmail.com> - 0.9.17-1
- Update to 0.9.17
* Tue Feb 08 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.9.16-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_15_Mass_Rebuild
* Sun Nov  7 2010 Dan Horák <dan[at]danny.cz> - 0.9.16-2
- switch to ExclusiveArch
* Thu Oct 28 2010 Nicoleau Fabien <nicoleau.fabien@gmail.com> - 0.9.16-1
- Update to 0.9.16
* Wed Jun  2 2010 Nicoleau Fabien <nicoleau.fabien@gmail.com> 0.9.15-1
- Update to 0.9.15
* Sat Feb 27 2010 Nicoleau Fabien <nicoleau.fabien@gmail.com> 0.9.13-1
- Update to 0.9.13
* Thu Feb 18 2010 Nicoleau Fabien <nicoleau.fabien@gmail.com> 0.9.12-1
- Update to 0.9.12
* Tue Jan 12 2010 Nicoleau Fabien <nicoleau.fabien@gmail.com> 0.9.11-1
- Update to 0.9.11
* Fri Jul 24 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.9.10-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_12_Mass_Rebuild
* Sat Jul 11 2009 Nicoleau Fabien <nicoleau.fabien@gmail.com> 0.9.10-1
- Rebuild for 0.9.10
* Wed May 27 2009 Nicoleau Fabien <nicoleau.fabien@gmail.com> 0.9.9-1
- Rebuild for 0.9.9
* Wed Apr 22 2009 Kedar Sovani <kedars@marvell.com> 0.9.8-2
- ExcludeArch ARM
* Sat Apr  4 2009 Nicoleau Fabien <nicoleau.fabien@gmail.com> 0.9.8-1
- Rebuild for 0.9.8
* Tue Mar 17 2009 Nicoleau Fabien <nicoleau.fabien@gmail.com> 0.9.7-1
- Rebuild for 0.9.7
* Tue Feb 24 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.9.6-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_11_Mass_Rebuild
- added an include
* Sun Nov 23 2008 Nicoleau Fabien <nicoleau.fabien@gmail.com> 0.9.6-2
- Licence fix
- Add desktop file
- Removed separate plugin package
* Sun Nov 16 2008 Nicoleau Fabien <nicoleau.fabien@gmail.com> 0.9.6-1
- Rebuild for 0.9.6
* Mon Sep 29 2008 Nicoleau Fabien <nicoleau.fabien@gmail.com> 0.9.5-1
- rebuild for 0.9.5
* Wed Aug 13 2008 Nicoleau Fabien <nicoleau.fabien@gmail.com> 0.9.4-1
- rebuild for 0.9.4
* Sat Aug  9 2008 Nicoleau Fabien <nicoleau.fabien@gmail.com> 0.9.3-1
* rebuild for 0.9.3
* Thu Jul 31 2008 Nicoleau Fabien <nicoleau.fabien@gmail.com> 0.9.2-1
- rebuild for 0.9.2
* Mon Jul 28 2008 Nicoleau Fabien <nicoleau.fabien@gmail.com> 0.9.1-1
- Rebuild for 0.9.1
* Mon Jul 21 2008 Nicoleau Fabien <nicoleau.fabien@gmail.com> 0.9.0-1
- Initital build
