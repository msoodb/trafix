<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- Copyright (C) 2025 Masoud Bolhassani -->

# Trafix Release Workflow

This document describes the release process for Trafix: test the code, bump the
version, create the Git tag, and build Fedora RPM packages.

For short per-release summaries, see [`RELEASE_NOTES.md`](./RELEASE_NOTES.md).

## Requirements

Install the required Fedora build tools and runtime dependencies:

```sh
sudo dnf install gcc make git rpm-build rpmdevtools rpmlint ncurses-devel lm_sensors iproute iw procps-ng
```

Create the RPM build tree once per machine:

```sh
rpmdev-setuptree
```

## Before Release

Start from a clean working tree:

```sh
git status --short
```

Build and run the test suite:

```sh
make clean
make
make test
```

Run the packaging linter:

```sh
rpmlint trafix.spec
```

If your local `gcc` command is wrapped by `ccache` and the cache is not
writable, use the real compiler:

```sh
make clean
make CC=/usr/bin/gcc
make test CC=/usr/bin/gcc
```

Commit all functional changes before bumping the version:

```sh
git add <changed-files>
git commit -m "Fix: describe the release change"
```

## Bump Version

Use the Makefile target and choose `patch`, `minor`, or `major` when prompted:

```sh
make bump
```

For a non-interactive bump, call the script directly:

```sh
./scripts/bump-version.sh patch
```

The bump script updates:

- `VERSION`
- `Version:` in `trafix.spec`
- `%changelog` in `trafix.spec`

It then commits the version bump.

## Tag And Build RPMs

Create and push the release tag:

```sh
make tag
```

The tag name is built from `VERSION` as `v<version>`. The RPM spec uses this tag
archive as its source.

Build the source and binary RPM packages:

```sh
make rpm
```

The `rpm` target:

- copies `trafix.spec` to `~/rpmbuild/SPECS/trafix.spec`
- keeps the copied spec `Version:` field in sync with `VERSION`
- downloads the GitHub tag archive with `spectool`
- runs `rpmbuild -ba`
- runs the `%check` section, which executes `make test`

Generated packages are written under:

```sh
~/rpmbuild/RPMS/
~/rpmbuild/SRPMS/
```

## Final Validation

Run `rpmlint` on the generated packages:

```sh
rpmlint ~/rpmbuild/SRPMS/trafix-*.src.rpm ~/rpmbuild/RPMS/*/trafix-*.rpm
```

For a stricter Fedora-style build, use `mock` on the generated SRPM:

```sh
mock -r fedora-rawhide-x86_64 ~/rpmbuild/SRPMS/trafix-*.src.rpm
```

Confirm that the remote tag points to the intended release commit:

```sh
git ls-remote --tags origin v$(grep -v '^#' VERSION | head -n 1)
```

For annotated tags, the `refs/tags/v<version>^{}` line should resolve to the
same commit as the release commit.

## Publish GitHub Release

If `rpmlint` and `mock` are clean, publish the release:

1. Go to GitHub releases.
2. Create a release from tag `v<version>`.
3. Attach the generated RPM artifacts:

```sh
~/rpmbuild/SRPMS/trafix-<version>-*.src.rpm
~/rpmbuild/RPMS/x86_64/trafix-<version>-*.x86_64.rpm
```

The Git tag and RPM source archive should now be aligned.

## Test Installed RPM

Install the generated binary RPM locally:

```sh
sudo dnf install ~/rpmbuild/RPMS/x86_64/trafix-<version>-*.x86_64.rpm
```

Verify package metadata and installed files:

```sh
rpm -qi trafix
rpm -ql trafix
ls -l /etc/trafix/config.cfg
man trafix
```

Run Trafix and verify that the dashboard opens, panels render, and `q` exits:

```sh
trafix
```

Remove the local test install when finished:

```sh
sudo dnf remove trafix
```

## Fedora Review Request

After the GitHub release is published and the generated packages pass `rpmlint`
and `mock`, submit a Fedora package review request in Bugzilla:

```text
https://bugzilla.redhat.com/bugzilla/enter_bug.cgi?product=Fedora&format=fedora-review
```

Use this summary format:

```text
Review Request: trafix - Lightweight Linux terminal dashboard for system and network monitoring
```

Use this description template:

```text
Spec URL: https://raw.githubusercontent.com/msoodb/trafix/v<version>/trafix.spec
SRPM URL: https://github.com/msoodb/trafix/releases/download/v<version>/trafix-<version>-1.fc<fedora>.src.rpm

Description:
Trafix is a lightweight terminal dashboard for Linux. It provides real-time
system, CPU, memory, disk, process, connection, and network activity monitoring
through an ncurses interface.

Fedora Account System Username: <your_fedora_username>

rpmlint:
0 errors, 0 warnings

mock:
Build completed successfully for fedora-rawhide-x86_64.

License:
GPL-3.0-or-later
```

If this is your first Fedora package, add the review request as blocking:

```text
FE-NEEDSPONSOR
```

The FE-NEEDSPONSOR tracker bug is:

```text
177841
```

After filing, wait for reviewer comments and update the spec or package as
requested. If the review sits idle, ask politely for a review swap on Fedora
packaging or development channels.

## Notes

- Source tarball creation is not needed in this repository. GitHub generates
  release archives from Git tags.
- Do not run `make tag` until the version bump commit is correct.
- `make tag` pushes the created tag to `origin`.
- `make rpm` depends on the release tag archive being available from GitHub, so
  run it after `make tag` for a new release.
- The checked-in `trafix.spec` should always contain the current real version,
  not a placeholder.
