#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
SDK="${ANDROID_SDK_ROOT:-${ANDROID_HOME:?Android SDK required}}"
TOOLS="$SDK/build-tools/35.0.0"
JAR="$SDK/platforms/android-35/android.jar"
OUT="$PWD/out"
mkdir -p "$OUT/classes" "$OUT/dex" "$OUT/test"
javac -source 8 -target 8 -d "$OUT/test" src/pl/ardurower/garetthr/HrPacket.java test/pl/ardurower/garetthr/HrPacketTest.java
java -cp "$OUT/test" pl.ardurower.garetthr.HrPacketTest
javac --release 8 -classpath "$JAR" -d "$OUT/classes" $(find src -name '*.java')
"$TOOLS/d8" --min-api 26 --lib "$JAR" --output "$OUT/dex" $(find "$OUT/classes" -name '*.class')
"$TOOLS/aapt" package -f -M AndroidManifest.xml -I "$JAR" -F "$OUT/unsigned.apk"
(cd "$OUT/dex" && zip -q "$OUT/unsigned.apk" classes.dex)
"$TOOLS/zipalign" -f 4 "$OUT/unsigned.apk" "$OUT/aligned.apk"
KEY="${HR_KEYSTORE:-$OUT/test.keystore}"
PASS="${HR_KEYSTORE_PASSWORD:-android}"
if [ ! -f "$KEY" ]; then
  keytool -genkeypair -keystore "$KEY" -storepass "$PASS" -keypass "$PASS" -alias ardurower -keyalg RSA -keysize 2048 -validity 10000 -dname 'CN=ArduRower HR Test'
fi
APK="$OUT/ArduRower-HR-Garett-0.1.1.apk"
"$TOOLS/apksigner" sign --ks "$KEY" --ks-key-alias ardurower --ks-pass "pass:$PASS" --key-pass "pass:$PASS" --min-sdk-version 26 --v1-signing-enabled true --v2-signing-enabled false --v3-signing-enabled false --v4-signing-enabled false --out "$APK" "$OUT/aligned.apk"
"$TOOLS/apksigner" verify --verbose --print-certs --min-sdk-version 26 "$APK" | tee "$OUT/signature.txt"
grep -q 'Verified using v1 scheme (JAR signing): true' "$OUT/signature.txt"
"$TOOLS/aapt" dump badging "$APK" | tee "$OUT/badging.txt"
test "$(unzip -Z1 "$APK" | grep -Ec '^classes[0-9]*\.dex$')" = 1
! unzip -Z1 "$APK" | grep -q '^lib/'
(cd "$OUT" && sha256sum "$(basename "$APK")" > SHA256SUMS.txt)
