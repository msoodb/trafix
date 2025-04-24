Name:           trafix
Version:        __VERSION__
Release:        1%{?dist}
Summary:        A simple network monitoring tool for Linux

License:        GPL-3.0-or-later
URL:            https://github.com/msoodb/trafix
Source0:        https://github.com/msoodb/%{name}/archive/refs/tags/v%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  ncurses-devel
BuildRequires:  libpcap-devel
BuildRequires:  lm_sensors

%global have_libpcap %(pkg-config --exists libpcap && echo yes || echo no)
%global have_ncurses %(pkg-config --exists ncurses && echo yes || echo no)

%if %{have_libpcap} == yes
Requires:       libpcap
%endif

%if %{have_ncurses} == yes
Requires:       ncurses-libs
%endif

Requires:       lm_sensors

%global debug_package %{nil}

%description
Trafix is a lightweight tool that provides real-time insights into connections, CPU, and network through a command-line interface.

%prep
%autosetup

%check
# No test suite is available for this package. The package is manually tested.

%build
make

%install
make install DESTDIR=%{buildroot}

# Install man page
install -D -m 644 man/trafix.1 %{buildroot}%{_mandir}/man1/trafix.1

%files
%license LICENSE
%doc README.md
%{_bindir}/trafix
%{_mandir}/man1/trafix.1

%changelog
* Thu Apr 24 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com> - 1.0.2-1
- Bump version to 1.0.2

* Thu Apr 24 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com> - 1.0.1-1
- Bump version to 1.0.1

* Mon Apr 22 2024 Masoud Bolhassani <masoud.bolhassani@gmail.com> - 1.0-1
- Initial RPM release
