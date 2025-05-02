# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2025 Masoud Bolhassani

Name:           trafix
Version:        __VERSION__
Release:        1%{?dist}
Summary:        A simple monitoring tool for Linux

License:        GPL-3.0-or-later
URL:            https://github.com/msoodb/%{name}
Source0:        %{url}/releases/download/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  make
BuildRequires:  ncurses-devel
BuildRequires:  libpcap-devel
BuildRequires:  lm_sensors

Requires:       lm_sensors

%global debug_package %{nil}

%description
Trafix is a lightweight command-line tool that provides real-time insights into
system connections, CPU, and network activity.

%prep
%autosetup

%build
%set_build_flags
%make_build

%install
%make_install PREFIX=%{_prefix}
install -Dm644 man/trafix.1 %{buildroot}%{_mandir}/man1/trafix.1
install -Dm644 config/config.cfg %{buildroot}/etc/trafix/config.cfg

%check
# No test suite upstream; basic functionality tested manually.

%files
%license LICENSE
%doc README.md
%{_bindir}/trafix
%{_mandir}/man1/trafix.1*
%config(noreplace) /etc/trafix/config.cfg

%changelog
* Fri May 02 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com> - 1.0.4-1
- Bump version to 1.0.4

* Fri May 02 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com> - 1.0.3-1
- Bump version to 1.0.3

* Fri May 02 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com> - 1.0.2-1
- Bump version to 1.0.2

* Wed Apr 30 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com> - 1.0.1-1
- Bump version to 1.0.1

* Fri Apr 25 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com> - 1.0.0-1
- Initial RPM release
