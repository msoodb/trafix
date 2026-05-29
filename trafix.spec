# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2025 Masoud Bolhassani

Name:           trafix
Version:        0.1.1
Release:        1%{?dist}
Summary:        A simple monitoring tool for Linux

License:        GPL-3.0-or-later
URL:            https://github.com/msoodb/%{name}
%global gittag v%{version}
Source0:        %{url}/archive/%{gittag}/%{name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  make
BuildRequires:  ncurses-devel

Requires:       iproute
Requires:       iw
Requires:       lm_sensors
Requires:       procps-ng

%description
Trafix is a lightweight terminal dashboard that provides real-time insights into
Linux system, CPU, memory, disk, process, connection, and network activity.

%prep
%autosetup -n %{name}-%{version}

%build
%make_build

%check
%make_build test

%install
%make_install PREFIX=%{_prefix}
install -Dpm644 man/trafix.1 %{buildroot}%{_mandir}/man1/trafix.1
install -Dpm644 config/config.cfg %{buildroot}%{_sysconfdir}/trafix/config.cfg

%files
%license LICENSE
%doc README.md
%{_bindir}/trafix
%{_mandir}/man1/trafix.1*
%dir %{_sysconfdir}/trafix
%config(noreplace) %{_sysconfdir}/trafix/config.cfg

%changelog
* Fri May 29 2026 Masoud Bolhassani <masoud.bolhassani@gmail.com> - 0.1.1-1
- Bump version to 0.1.1

* Wed May 27 2026 Masoud Bolhassani <masoud.bolhassani@gmail.com> - 0.1.0-1
- Initial 0.1.0 RPM release
