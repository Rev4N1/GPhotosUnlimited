# Copy any disabled app files to updated module
if [ -d /data/adb/modules/unlimitedphotos/system ]; then
    ui_print "- Restoring disabled configuration"
    cp -afL /data/adb/modules/unlimitedphotos/system $MODPATH
fi

# Copy any supported custom files to updated module
for FILE in custom.app_replace_list.txt custom.fgp.json custom.fgp.prop uninstall.sh; do
    if [ -f "/data/adb/modules/unlimitedphotos/$FILE" ]; then
        ui_print "- Restoring $FILE"
        cp -af /data/adb/modules/unlimitedphotos/$FILE $MODPATH/$FILE
    fi
done

# Warn if potentially conflicting modules are installed
for MODULE in Pixelify PixelifyPhotos unlimitedgpics; do
    if [ -d /data/adb/modules/$MODULE ]; then
        ui_print "! $MODULE module may cause issues with this module"
    fi
done

# Run common tasks for installation and boot-time
. $MODPATH/common_setup.sh

# Migrate custom.fgp.json to latest defaults if needed
if [ -f "$MODPATH/custom.fgp.json" ]; then
    chmod 755 $MODPATH/migrate.sh
    sh $MODPATH/migrate.sh --install $MODPATH/custom.fgp.json
    ui_print " "
fi

# Generate a random custom.fgp.prop if none was restored, falling back to the default fgp.prop
if [ ! -f "$MODPATH/custom.fgp.prop" -a ! -f "$MODPATH/custom.fgp.json" ]; then
    ui_print "- Generating random fingerprint with autofgp.sh"
    if sh $MODPATH/autofgp.sh >/dev/null 2>&1 && [ -f "$MODPATH/custom.fgp.prop" ]; then
        ui_print "  $(grep -m1 '^FINGERPRINT=' $MODPATH/custom.fgp.prop | cut -d= -f2)"
    else
        ui_print "! Failed to generate, using default fgp.prop (run Action button to retry)"
    fi
fi
