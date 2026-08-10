## Google Photos Unlimited v4

- Fix Pixel Satellite SOS (and other Pixel-exclusive system services) breaking: the module no longer hides `pixel_experience_<year>_exclusive.xml` sysconfig files system-wide on any device/ROM, and removes any such override left behind by an earlier version when updating ([#19](https://github.com/Rev4N1/GPhotosUnlimited/issues/19))
- Switch the default device profile to the original Pixel (non-XL), matching a real `sailfish` factory image build.prop (`PPR1.180610.009`, August 2018).
- Document that unlimited storage has to be verified on a newly backed-up photo, since Google Photos no longer reliably shows the storage tier in its settings ([#18](https://github.com/Rev4N1/GPhotosUnlimited/issues/18))

## Google Photos Unlimited v3

- Fix crashes when switching to "Collections"
- Fix Build field spoofing on Android 17

_[Full changelogs](https://github.com/Rev4N1/GPhotosUnlimited/releases)_
