# Replace/hide conflicting Pixel Configurations files to disable them
directory="/system/product/etc/sysconfig"
for APP in $directory/pixel_experience_*; do
	# Extract the year from the APP name
	year="${APP#*pixel_experience_}"
	year="${year:0:4}"

    # Check if the year is higher than 2019
	if [ "$year" -gt 2019 ]; then
        case $APP in
            /system/*) ;;
            *) PREFIX=/system;;
        esac
        HIDEPATH=$MODPATH$PREFIX$APP
        mkdir -p $(dirname $HIDEPATH)
        if [ "$KSU" = "true" -o "$APATCH" = "true" ]; then
            mknod $HIDEPATH c 0 0
        else
            touch $HIDEPATH
        fi
    fi
done


# Replace/hide conflicting custom ROM injection app folders/files to disable them
LIST=$MODPATH/app_replace_list.txt
[ -f "$MODPATH/custom.app_replace_list.txt" ] && LIST=$MODPATH/custom.app_replace_list.txt
for APP in $(grep -v '^#' $LIST); do
    if [ -e "$APP" ]; then
        case $APP in
            /system/*) ;;
            *) PREFIX=/system;;
        esac
        HIDEPATH=$MODPATH$PREFIX$APP
        if [ -d "$APP" ]; then
            mkdir -p $HIDEPATH
            if [ "$KSU" = "true" -o "$APATCH" = "true" ]; then
                setfattr -n trusted.overlay.opaque -v y $HIDEPATH
            else
                touch $HIDEPATH/.replace
            fi
        else
            mkdir -p $(dirname $HIDEPATH)
            if [ "$KSU" = "true" -o "$APATCH" = "true" ]; then
                mknod $HIDEPATH c 0 0
            else
                touch $HIDEPATH
            fi
        fi
        case $APP in
            */overlay/*)
                CFG=$(echo $APP | grep -oE '.*/overlay')/config/config.xml
                if [ -f "$CFG" ]; then
                    if [ -d "$APP" ]; then
                        APK=$(readlink -f $APP/*.apk);
                    elif [[ "$APP" = *".apk" ]]; then
                        APK=$(readlink -f $APP);
                    fi
                    if [ -s "$APK" ]; then
                        PKGNAME=$(unzip -p $APK AndroidManifest.xml | tr -d '\0' | grep -oE 'android.*overlay' | strings | tr -d '\n' | sed -e 's/^android//' -e 's/application//' -e 's;*http.*res/android;;' -e 's/manifest//' -e 's/overlay$//' | grep -oE '[[:alnum:].-_].*overlay' | cut -d\  -f2)
                        if [ "$PKGNAME" ] && grep -q "overlay package=\"$PKGNAME" $CFG; then
                            HIDECFG=$MODPATH$PREFIX$CFG
                            if [ ! -f "$HIDECFG" ]; then
                                mkdir -p $(dirname $HIDECFG)
                                cp -af $CFG $HIDECFG
                            fi
                            sed -i 's;<overlay \(package="'"$PKGNAME"'".*\) />;<!-- overlay \1 -->;' $HIDECFG
                        fi
                    fi
                fi
            ;;
        esac
        if [[ -d "$APP" || "$APP" = *".apk" ]]; then
            ui_print "! $(basename $APP .apk) ROM app disabled, please uninstall any user app versions/updates after next reboot"
            [ "$HIDECFG" ] && ui_print "!  + $PKGNAME entry commented out in copied overlay config"
        fi
    fi
done