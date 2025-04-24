Name:           trafix
Version:        __VERSION__
Release:        1%{?dist}
Summary:        A simple network monitoring tool for Linux

License:        MIT
URL:            https://github.com/msoodb/trafix
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  ncurses-devel
BuildRequires:  libpcap-devel
BuildRequires:  lm_sensors

Requires:       ncurses-libs
Requires:       libpcap
Requires:       lm_sensors


%global debug_package %{nil}

%description
Trafix is a lightweight monitoring tool for Linux that provides real-time insights into active connections and offers quick visibility into your network performance through an intuitive command-line interface.

%prep
%autosetup

%build
make

%install
make install DESTDIR=%{buildroot}

%files
%license LICENSE
%doc README.md
%{_bindir}/trafix

%changelog
* Mon Apr 22 2024 Masoud Bolhassani <masoud.bolhassani@gmail.com> - 1.0-1
- Initial RPM release
