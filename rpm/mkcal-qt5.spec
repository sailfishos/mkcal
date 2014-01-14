Name:       mkcal-qt5

Summary:    Extended KDE kcal calendar library port for Maemo
Version:    0.3.13
Release:    1
Group:      System/Libraries
License:    LGPLV2
URL:        https://github.com/mer-packages/mkcal
Source0:    %{name}-%{version}.tar.bz2
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Gui)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt5Test)
BuildRequires:  pkgconfig(libkcalcoren-qt5)
BuildRequires:  pkgconfig(sqlite3)
BuildRequires:  pkgconfig(dbus-1)
BuildRequires:  pkgconfig(uuid)
BuildRequires:  pkgconfig(libical)
BuildRequires:  pkgconfig(timed-qt5) >= 2.88

%description
Extended KDE kcal calendar library port for Maemo


%package devel
Summary:    Development files for mkcal
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description devel
This package contains the files necessary to develop
applications using mkcal


%prep
%setup -q -n %{name}-%{version}

%build
%qmake5
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
%qmake5_install

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_libdir}/libmkcal-qt5.so.*

%files devel
%defattr(-,root,root,-)
%{_includedir}/mkcal-qt5/*
%{_libdir}/libmkcal-qt5.so
%{_libdir}/pkgconfig/*.pc
