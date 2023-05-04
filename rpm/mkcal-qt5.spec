Name:       mkcal-qt5

Summary:    SQlite storage backend for KCalendarCore
Version:    0.7.24
Release:    1
License:    LGPLv2+
URL:        https://github.com/sailfishos/mkcal
Source0:    %{name}-%{version}.tar.bz2
Source1:    %{name}.privileges
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires:  cmake
BuildRequires:  extra-cmake-modules >= 5.75.0
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Gui)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(KF5CalendarCore)
BuildRequires:  pkgconfig(sqlite3)
BuildRequires:  pkgconfig(timed-qt5) >= 2.88
BuildRequires:  pkgconfig(QmfClient)

%description
Extends KDE calendar core library and provides an SQlite backend.


%package devel
Summary:    Development files for mkcal
Requires:   %{name} = %{version}-%{release}
Requires:   pkgconfig(KF5CalendarCore)

%description devel
This package contains the files necessary to develop
applications using mkcal

%package tests
Summary: Unit tests for mkcal
BuildRequires: pkgconfig(Qt5Test)
Requires: %{name} = %{version}-%{release}

%description tests
This package contains unit tests for extended KDE kcal calendar library.


%prep
%setup -q -n %{name}-%{version}

%build
%cmake -DINSTALL_TESTS=ON
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
%make_install

mkdir -p %{buildroot}%{_datadir}/mapplauncherd/privileges.d
install -m 644 -p %{SOURCE1} %{buildroot}%{_datadir}/mapplauncherd/privileges.d/

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%license LICENSE.LGPL2
%{_libdir}/lib%{name}.so.*
%{_libdir}/mkcalplugins/*.so
%{_bindir}/mkcaltool
%{_datadir}/mapplauncherd/privileges.d/*

%files devel
%defattr(-,root,root,-)
%{_includedir}/%{name}
%{_libdir}/lib%{name}.so
%{_libdir}/pkgconfig/*.pc

%files tests
%defattr(-,root,root,-)
/opt/tests/mkcal/tst_load
/opt/tests/mkcal/tst_perf
/opt/tests/mkcal/tst_storage
/opt/tests/mkcal/tst_backend
/opt/tests/mkcal/tests.xml
