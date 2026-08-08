# Google Photos Unlimited
A Zygisk module which gives unlimited Google Photos storage.

To use this module you must have one of the following (latest versions):

- [Magisk](https://github.com/topjohnwu/Magisk) with Zygisk enabled
- [KernelSU](https://github.com/tiann/KernelSU) with [Zygisk Next](https://github.com/Dr-TSNG/ZygiskNext) or [ReZygisk](https://github.com/PerformanC/ReZygisk) module installed
- [KernelSU Next](https://github.com/KernelSU-Next/KernelSU-Next) with [Zygisk Next](https://github.com/Dr-TSNG/ZygiskNext) or [ReZygisk](https://github.com/PerformanC/ReZygisk) module installed
- [APatch](https://github.com/bmax121/APatch) with [Zygisk Next](https://github.com/Dr-TSNG/ZygiskNext) or [ReZygisk](https://github.com/PerformanC/ReZygisk) module installed

> [!IMPORTANT]
> **KernelSU Next 3.0.0+ users:** Starting with version 3.0.0, KernelSU Next removed its built-in module mount system (see [KernelSU-Next#1078](https://github.com/KernelSU-Next/KernelSU-Next/issues/1078)). Without a mount system, this module's `/system` files — including the `pixel_2016_exclusive.xml` that grants the unlimited-storage entitlement — are **not applied**, so the spoof will be incomplete (Google Photos may show the spoofed Pixel model but still not grant unlimited storage).
>
> To restore mounting, install a metamount module such as [Magic Mount (magic_mount_rs)](https://github.com/KernelSU-Modules-Repo/magic_mount_rs), then reboot. After that, clear Google Photos data so it re-checks eligibility.
>
> This does not affect Magisk, classic KernelSU (GKI), or APatch, which retain their own mount systems.

## About module

It injects a classes.dex file to modify fields in the android.os.Build class. Also, it creates a hook in the native code to modify system properties. These are spoofed only into Google Photos.

## Verifying unlimited storage

After installing (and rebooting), **back up a new photo**, then open that photo and swipe up (or tap the ⓘ button) to show its details. Working means the details say the photo does not count against your Google Account storage.

> [!IMPORTANT]
> Check a newly backed-up photo, **not** the settings screen. Google Photos does not reliably surface the storage tier under **Profile / Settings → Backup** any more - it is missing entirely in some versions (7.65 and 7.85 among them) and has moved or reappeared between others. Not seeing it there does not mean the module has failed, and this has cost people days of pointless troubleshooting.

If a newly backed-up photo *does* count against your storage, clear Google Photos app data and reboot, then take another photo and check again.

## About 'custom.app_replace_list.txt' file

You can customize the included default [app_replace_list.txt](https://raw.githubusercontent.com/Rev4N1/GPhotos-Unlimited/main/module/app_replace_list.txt) from the module directory (/data/adb/modules/unlimitedphotos) then rename it to custom.app_replace_list.txt to systemlessly replace any additional conflicting custom ROM spoof injection app paths to disable them.

## Troubleshooting

Make sure Google Photos (com.google.android.apps.photos) is NOT on the Magisk DenyList if Enforce DenyList is enabled since this interferes with the module; the module does prevent this using scripts but it only happens once during each reboot.

### Failing to work (on KernelSU/APatch)

- Disable Zygisk Next
- Reboot
- Enable Zygisk Next
- Reboot again

### Read module logs

You can read module logs using one of these commands directly after boot:

`adb shell "logcat | grep 'FGP/'"` or `su -c "logcat | grep 'FGP/'"`

Add a "verboseLogs" entry with a value of "0", "1", "2", "3" or "100" to your custom.fgp.json to enable higher logging levels; "100" will dump all Build fields, and all the system properties that Photos is checking.

## About spoofing Advanced Settings

The advanced spoofing options add granular control over what exactly gets spoofed, allowing one to disable the parts that may conflict with other kinds of spoofing modules. See more in the Details area below.

<details>
<summary><strong>Details</strong></summary>

- Other than for the "verboseLogs" entry (see above), they are all 0 (disabled) or 1 (enabled).

- The "spoofBuild" entry (default 1) controls spoofing the Build Fields from the fingerprint; the "spoofProps" entry (default 0) controls spoofing the System Properties from the fingerprint; the "spoofProvider" entry (default 0) controls spoofing the Keystore Provider, and the "spoofSignature" entry (default 0) controls spoofing the ROM Signature.

</details>
 

## Credits

Module scripts were adapted from those of osm0sis's Play Integrity Fork (PIFork) module, please see the commit history of [osm0sis's Play Integrity Fork](https://github.com/osm0sis/PlayIntegrityFork) for proper attribution.
