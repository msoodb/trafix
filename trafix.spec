Name:           trafix
Version:        1.0
Release:        1%{?dist}
Summary:        A simple C monitoring tool for Linux

License:        MIT
URL:            https://github.com/msoodb/trafix
Source0:        trafix-1.0.tar.gz

BuildRequires:  gcc
BuildRequires:  ncurses-devel
BuildRequires:  libpcap-devel

%description
Trafix is a lightweight network monitoring tool for Linux that provides real-time insights into active connections and bandwidth usage. Track top talkers, set alerts, and gain quick visibility into your network performance through an intuitive command-line interface. This package is built for Fedora.

%prep
%autosetup

%build
make

%install
mkdir -p %{buildroot}/usr/bin
cp trafix %{buildroot}/usr/bin/

%files
/usr/bin/trafix

%changelog
* Mon Apr 22 2024 Masoud Bolhassani <masoud.bolhassani@gmail.com> - 1.0-1
- Initial RPM release
