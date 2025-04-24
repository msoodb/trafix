## Trafix Release Workflow

This document describes how to make a new release of the trafix project, including versioning, tagging, and RPM packaging for Fedora.
Ensure you have the following installed:
- rpm-build
- rpmdevtools
- gcc, make
- git
- libpcap-devel, ncurses-devel, lm_sensors
- A proper ~/rpmbuild structure:
```sh
	rpmdev-setuptree
```

## Workflow
1. Make Code Changes: Make your changes in the source code.
```sh
	nano src/main.c
	git add src/main.c
	git commit -m "Fix: correct packet stats rendering"
```
2. Bump the Version: Update the VERSION file and changelog automatically.
```sh
	make bump patch/minor/major
```
3. Create a Git Tag: Tag the release using the version from the VERSION file.
```sh
	make tag
```
4. Generate the Source Tarball: Create a .tar.gz of the source code from the Git tag.
```sh
	make tarball
```
5. Copy the Spec File: Copy trafix.spec into the correct RPM build directory.
```sh
	make copy-spec
```
6. Build the RPM: Build both the binary and source RPM packages.
```sh
	make rpm
```


# Workflow - Magic
1. Make Code Changes: Make your changes in the source code.
```sh
	nano src/main.c
	git add src/main.c
	git commit -m "Fix: correct packet stats rendering"
```
2. Bump the Version: Update the VERSION file and changelog automatically.
```sh
	make bump
```
3. One-Command Full Release: Use this to run tag, tarball, and rpm build all in one go.
```sh
	make release
```
