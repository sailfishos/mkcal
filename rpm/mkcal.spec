# spec file for package mkcal

%define ver_maj 0
%define ver_min 3
%define ver_pat 11

Name:           mkcal
Version:        0.3.11
Release:        1
License:        LGPLv2
Summary:        Extended KDE kcal calendar library port for Maemo
Group:          System/Libraries
Source0:        %{name}-%{version}.tar.gz
BuildRequires:  doxygen
BuildRequires:  fdupes
#BuildRequires:  graphviz
BuildRequires:  pkgconfig(QtCore)
BuildRequires:  pkgconfig(libical)
BuildRequires:  pkgconfig(libkcalcoren)
BuildRequires:  pkgconfig(sqlite3)
#BuildRequires:  pkgconfig(tracker-client-0.10)
BuildRequires:  pkgconfig(uuid)
BuildRequires:  pkgconfig(qmfclient)
BuildRequires:  pkgconfig(QtSparql)

%description
%{summary}.

%package devel
Summary:        Development files for %{name}
Group:          Development/Libraries
Requires:       %{name} = %{version}

%description devel
%{summary}.

%package doc
Summary:        Documentation for %{name}
Group:          Documentation

%description doc
%{summary}.

%prep
%setup -q

%build
touch src/libmkcal.so.%{ver_maj}.%{ver_min}.%{ver_pat}
%qmake INCLUDEPATH="%{_includedir}/kcalcoren %{_includedir}/QtGui"
make VER_MAJ=%{ver_maj} VER_MIN=%{ver_min} VER_PAT=%{ver_pat} %{?_smp_mflags}
make doc

%install
%qmake_install VER_MAJ=%{ver_maj} VER_MIN=%{ver_min} VER_PAT=%{ver_pat}
%fdupes %{buildroot}%{_docdir}

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
%defattr(644,root,root,-)
%{_libdir}/libmkcal.so.*
%{_libdir}/calendar/mkcalplugins/*.so

%files devel
%defattr(-,root,root,-)
%{_includedir}/mkcal/*.h
%{_libdir}/libmkcal.so
%{_libdir}/pkgconfig/libmkcal.pc
%{_datadir}/qt4/mkspecs/features/mkcal.prf

%files doc
%defattr(-,root,root,-)
%doc %{_docdir}/libmkcal-doc/*
