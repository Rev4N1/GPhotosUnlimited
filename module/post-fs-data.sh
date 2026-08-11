MODPATH="${0%/*}"

# Remove Google Photos from Magisk DenyList when set to Enforce in normal mode
if magisk --denylist status; then
    magisk --denylist rm com.google.android.apps.photos
fi
# Run common tasks for installation and boot-time
. $MODPATH/common_setup.sh