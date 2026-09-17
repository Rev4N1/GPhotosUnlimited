## Google Photos Unlimited v5

- Fix features (e.g. Unblur), only the features listed in `fgp.prop` are overridden and everything else passes through ([#20](https://github.com/Rev4N1/GPhotosUnlimited/issues/20))
- Make Pixel feature spoofing configurable: add a `# Feature Overrides` section to `fgp.prop` so any `hasSystemFeature` query can be set to `true`/`false`
- Avoid future detection of Dobby by switching spoofProps libc hooking method to latest ShadowHook (thanks @pershoot)
- Fix restoring from module versions up to v3: also remove stale `pixel_*_exclusive.xml` overrides and fully clean up the empty `$MODPATH/system` directory
- Drop dead zygisk guards in `customize.sh`/`post-fs-data.sh` (zygisk has been required since rebase)

_[Full changelogs](https://github.com/Rev4N1/GPhotosUnlimited/releases)_
