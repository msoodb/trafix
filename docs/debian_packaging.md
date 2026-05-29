# Trafix Debian and Ubuntu Packaging Notes

Trafix does not ship Debian or Ubuntu packages yet.

## Current Status

- No `debian/` packaging directory is present in this repository.
- No `.deb` artifacts are published.
- `apt install trafix` is not available today.

## Packaging Plan

When Debian/Ubuntu packaging starts, the package should:

- use `debhelper` and standard `dh` packaging helpers
- install the binary under `/usr/bin`
- install the man page under `/usr/share/man/man1`
- install the default config under `/etc/trafix/config.cfg` as a conffile
- keep the shipped README and release notes aligned with the package version
- run the local test suite during package build where the packaging environment allows it

## Dependencies

The package will need the same runtime tools Trafix uses today:

- `iproute2`
- `iw`
- `lm-sensors`
- `procps`

Build dependencies should include:

- `gcc`
- `make`
- `libncurses-dev`
- `debhelper`

## Next Step

Create Debian packaging metadata only when the repository has a reproducible
source package flow and the install/uninstall paths are aligned with the RPM
spec and Makefile.
