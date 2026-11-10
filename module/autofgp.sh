#!/system/bin/sh

if [ "$USER" != "root" -a "$(whoami 2>/dev/null)" != "root" ]; then
  echo "autofgp: need root permissions"; exit 1;
fi;
case "$HOME" in
  *termux*) echo "autofgp: need su root environment"; exit 1;;
esac;

until [ -z "$1" ]; do
  case "$1" in
    -h|--help|help) echo "sh autofgp.sh"; exit 0;;
    *) break;;
  esac;
done;

echo "Pixel (2016) fgp.prop generator script \
  \n  adapted from autopif4 by osm0sis @ xda-developers";

case "$0" in
  *.sh) DIR="$0";;
  *) DIR="$(lsof -p $$ 2>/dev/null | grep -o '/.*autofgp.sh$')";;
esac;
DIR=$(dirname "$(readlink -f "$DIR")");

item() { echo "\n- $@"; }
die() { echo "\nError: $@!"; exit 1; }
die_bb() { die "$@, install busybox"; }
warn() { echo "\nWarning: $@!"; }

find_busybox() {
  [ -n "$BUSYBOX" ] && return 0;
  local path;
  for path in /data/adb/modules/busybox-ndk/system/*/busybox /data/adb/magisk/busybox /data/adb/ksu/bin/busybox /data/adb/ap/bin/busybox; do
    if [ -f "$path" ]; then
      BUSYBOX="$path";
      return 0;
    fi;
  done;
  return 1;
}

if ! which wget >/dev/null || grep -q "wget-curl" $(which wget); then
  if ! find_busybox; then
    die_bb "wget not found";
  elif $BUSYBOX ping -c1 -s2 android.com 2>&1 | grep -q "bad address"; then
    die_bb "wget broken";
  else
    wget() { $BUSYBOX wget "$@"; }
  fi;
fi;

if ! echo "A\nB" | grep -m1 -A1 "A" | grep -q "B"; then
  if ! find_busybox; then
    die_bb "grep broken";
  else
    grep() { $BUSYBOX grep "$@"; }
  fi;
fi;

if [ "$DIR" = /data/adb/modules/unlimitedphotos -o "$DIR" = /data/adb/modules_update/unlimitedphotos ]; then
  DIR=$DIR/autofgp;
  mkdir -p $DIR;
fi;
cd "$DIR";

item "Selecting random Pixel (2016) device ...";
if [ "$((RANDOM % 2))" = 0 ]; then
  MODEL="Pixel";
  DEVICE="sailfish";
else
  MODEL="Pixel XL";
  DEVICE="marlin";
fi;
echo "$MODEL ($DEVICE)";

item "Crawling Android Flash Tool for $DEVICE build list ...";
wget -q -T 10 -O PIXEL_FLASH_HTML --no-check-certificate "https://flash.android.com/" 2>&1 || exit 1;
wget -q -T 10 -O PIXEL_STATION_JSON --header "Referer: https://flash.android.com" --no-check-certificate "https://content-flashstation-pa.googleapis.com/v1/builds?product=$DEVICE&key=$(grep -o '<body data-client-config=.*' PIXEL_FLASH_HTML | cut -d\; -f2 | cut -d\& -f1)" 2>&1 || exit 1;
BUILD_LIST="$(grep -E '"(buildId|releaseCandidateName)"' PIXEL_STATION_JSON | cut -d\" -f4 | sed 'N;s/\n/ /')";
BUILD_COUNT="$(echo "$BUILD_LIST" | grep -c ' ')";
echo "$BUILD_COUNT";
[ "$BUILD_COUNT" -gt 0 ] || die "Failed to extract build list from JSON";

item "Selecting random $DEVICE build ...";
set -- $(echo "$BUILD_LIST" | sed -n "$((RANDOM % $BUILD_COUNT + 1))p");
INCREMENTAL="$1";
ID="$2";
RELEASE="$(grep -A15 "\"buildId\": \"$INCREMENTAL\"" PIXEL_STATION_JSON | grep -m1 '"version"' | cut -d\" -f4)";
SDK="$(grep -A15 "\"buildId\": \"$INCREMENTAL\"" PIXEL_STATION_JSON | grep -m1 '"apiLevel"' | tr -dc '0-9')";
[ -z "$ID" -o -z "$INCREMENTAL" -o -z "$RELEASE" -o -z "$SDK" ] && die "Failed to extract build info from JSON";
case $RELEASE in
  7.1.0) RELEASE=7.1;;
  9.*|10.*) RELEASE="${RELEASE%%.*}";;
esac;
echo "Android $RELEASE ($ID)";

item "Crawling Factory Images for corresponding security patch level ...";
wget -q -T 10 -O PIXEL_FI_HTML --header "Cookie: devsite_wall_acks=nexus-image-tos" --no-check-certificate "https://developers.google.com/android/images" 2>&1 || exit 1;
FI_DATE="$(grep -A1 "<tr id=\"$DEVICE$(echo $ID | tr '[:upper:]' '[:lower:]')\">" PIXEL_FI_HTML | grep -oE '[A-Z][a-z]{2} 20[0-9]{2}' | head -n1)";
case ${FI_DATE% *} in
  Jan) MONTH=01;; Feb) MONTH=02;; Mar) MONTH=03;; Apr) MONTH=04;; May) MONTH=05;; Jun) MONTH=06;;
  Jul) MONTH=07;; Aug) MONTH=08;; Sep) MONTH=09;; Oct) MONTH=10;; Nov) MONTH=11;; Dec) MONTH=12;;
esac;
[ "$MONTH" ] || die "Failed to determine release date from Factory Images";
SECURITY_PATCH="${FI_DATE#* }-$MONTH-05";
# builds whose system build.prop security patch level differs from their Factory Images date
case $ID in
  NOF27D) SECURITY_PATCH=2017-04-01;;
  PPR1.181005.003.A1|PPR2.181005.003.A1) SECURITY_PATCH=2018-11-01;;
  PQ3A.190801.002) SECURITY_PATCH=2019-08-01;;
  QP1A.191005.007.A1|QP1A.191005.007.A3) SECURITY_PATCH=2019-10-06;;
esac;
echo "$SECURITY_PATCH";

item "Adding corresponding first API level ...";
DEVICE_INITIAL_SDK_INT=25;
echo "$DEVICE_INITIAL_SDK_INT";

item "Dumping values to minimal fgp.prop ...";
cat <<EOF | tee fgp.prop;
MANUFACTURER=Google
MODEL=$MODEL
FINGERPRINT=google/$DEVICE/$DEVICE:$RELEASE/$ID/$INCREMENTAL:user/release-keys
PRODUCT=$DEVICE
DEVICE=$DEVICE
HARDWARE=marlin
BOARD=$DEVICE
SDK=$SDK
DISPLAY=$ID
SECURITY_PATCH=$SECURITY_PATCH
DEVICE_INITIAL_SDK_INT=$DEVICE_INITIAL_SDK_INT
EOF

for MIGRATE in migrate.sh ../migrate.sh /data/adb/modules/unlimitedphotos/migrate.sh; do
  [ -f "$MIGRATE" ] && break;
done;
if [ -f "$MIGRATE" ]; then
  for OLDFGP in /data/adb/modules/unlimitedphotos/custom.fgp.prop /data/adb/modules/unlimitedphotos/custom.fgp.json; do
    [ -f "$OLDFGP" ] && break;
  done;
  if [ -f "$OLDFGP" ]; then
    grep -qE '//".*security_patch"|#.*security_patch' $OLDFGP && PATCH_COMMENT=1;
    item "Retaining existing configuration ...";
    # pass existing feature overrides on to migrate.sh
    case $OLDFGP in
      *.json) grep -v '^[[:space:]]*//' $OLDFGP | grep -o '"[^"]*\.[^"]*"[[:space:]]*:[[:space:]]*"[^"]*"' | sed 's/^"\([^"]*\)"[[:space:]]*:[[:space:]]*"\([^"]*\)"$/\1=\2/';;
      *.prop) grep -v '^[[:space:]]*#' $OLDFGP | grep '^[^=]*\.[^=]*=';;
    esac | grep '^[^=]*[A-Z][^=]*=' >> fgp.prop;
  fi;
  item "Converting fgp.prop to custom.fgp.prop with migrate.sh:";
  rm -f fgp.json custom.fgp.json custom.fgp.prop;
  sh $MIGRATE -i fgp.prop;
  grep_config() {
    if [ -f "$2" ]; then
      case $2 in
        *.json) grep -m1 "$1" $2 | cut -d\" -f4;;
        *.prop) grep -m1 "$1=" "$2" | cut -d= -f2 | cut -d\# -f1 | sed 's/[[:space:]]*$//';;
      esac;
    fi;
  }
  ADVSETTINGS="spoofBuild spoofProps spoofProvider spoofSignature spoofFeatures verboseLogs";
  for SETTING in $ADVSETTINGS; do
    eval [ -z \"\$$SETTING\" ] \&\& $SETTING=$(grep_config "$SETTING" $OLDFGP);
    eval TMPVAL=\$$SETTING;
    [ -n "$TMPVAL" ] && sed -i "s;\($SETTING=\).*;\1$TMPVAL;" custom.fgp.prop;
  done;
  [ "$PATCH_COMMENT" ] && sed -i 's;^\*.security_patch;#*.security_patch;' custom.fgp.prop;
  cat custom.fgp.prop;
fi;

if [ "$DIR" = /data/adb/modules/unlimitedphotos/autofgp -o "$DIR" = /data/adb/modules_update/unlimitedphotos/autofgp ]; then
  for OLDFGP in custom.fgp.prop custom.fgp.json; do
    if [ -f "../$OLDFGP" ]; then
      item "Renaming old file to $OLDFGP.bak ...";
      mv -fv ../$OLDFGP ../$OLDFGP.bak;
    fi;
  done;
  item "Installing new prop ...";
  cp -fv custom.fgp.prop ..;
  item "Killing any running Google Photos processes ...";
  am force-stop com.google.android.apps.photos 2>&1 || true;
fi;
