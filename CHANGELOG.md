## Google Photos Unlimited v2
- Fix spoofSignature crashes when built with Android Gradle Plugin 8.9.0+
- Improve ROM injector overlay APK package name parsing for more overlay xml disabling support (e.g. PixelOS 15)
- Fix various PIF module detections (thanks @simonpunk)
- Improve Dobby detection fix (thanks @5ec1cff)
- Add prop config format support and make it default
- Rename example.app_replace.list to app_replace_list.txt to be more intuitive
- Add support for Magisk's file/directory whiteout implementation

## Google Photos Unlimited v1
- Full rebase module into PIFork
- Now you can make a custom spoofing
- Fix retaining disabled ROM apps through module updates in some scenarios
- Copy any supported custom files to updated module
- Warn if potentially conflicting modules are installed
- Remove Google Photos from Magisk denyList if needed
- Fix compatiblility with lastest hOS version

_[Previous changelogs](https://github.com/Rev4N1/GPhotosUnlimited/releases)_
