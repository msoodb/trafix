# Trafix Release Checklist

Use this list before tagging and publishing a release.

## Version And Metadata

- [ ] Update `VERSION`.
- [ ] Update the man page version and date in `man/trafix.1`.
- [ ] Update the RPM spec `Version:` field in `trafix.spec`.
- [ ] Update the RPM spec `%changelog` in `trafix.spec`.

## Build Verification

- [ ] Run `make clean`.
- [ ] Run `make`.
- [ ] Run `make test`.
- [ ] Run `make asan`.
- [ ] Confirm all build and test commands finish without warnings or errors.

## Documentation Review

- [ ] Review `README.md` for accuracy.
- [ ] Review `man/trafix.1` for accuracy.
- [ ] Review `docs/` for accuracy.
- [ ] Review `RELEASE.md` for release-process accuracy.

## Packaging Review

- [ ] Confirm `trafix.spec` matches the shipped files.
- [ ] Confirm `make install` and `make uninstall` still behave as expected.
- [ ] Confirm the package metadata matches the release notes and version.

## Final Release Steps

- [ ] Create the release commit.
- [ ] Tag the release commit.
- [ ] Push the tag.
- [ ] Publish the release artifacts.
