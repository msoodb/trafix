Name:           trafix
Version:        __VERSION__
Release:        1%{?dist}
Summary:        A simple monitoring tool for Linux

License:        GPL-3.0-or-later
URL:            https://github.com/msoodb/%{name}
Source0:        %{url}/archive/refs/tags/%{name}-%{version}.tar.gz

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
make CFLAGS="%{optflags} -fPIE" LDFLAGS="-pie"

%install
make install DESTDIR=%{buildroot}
strip %{buildroot}%{_bindir}/trafix
install -Dm644 man/trafix.1 %{buildroot}%{_mandir}/man1/trafix.1

%check
# No test suite upstream; basic functionality tested manually.

%files
%license LICENSE
%doc README.md
%{_bindir}/trafix
%{_mandir}/man1/trafix.1*

%changelog
* Fri Apr 25 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com> - 1.0.3-1
- Bump version to 1.0.3

* Fri Apr 25 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com> - 1.0.2-1
- Bump version to 1.0.2

* Fri Apr 25 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com> - 1.0.1-1
- Bump version to 1.0.1

* Fri Apr 25 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com> - 1.0.0-1
- Initial RPM release
