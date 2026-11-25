#!/usr/bin/env bash
#
# Builds the LoopDeck AUv3 MIDI processor for iOS and packages it as an .ipa.
#
# Must run on macOS with Xcode installed. GitHub Actions runs this exact script
# (see .github/workflows/build-auv3-ipa.yml), so a local run and a CI run
# produce the same IPA.
#
#   ./scripts/build_ipa.sh                 # unsigned IPA (sideload tools sign it)
#   IOS_TEAM_ID=ABCDE12345 \
#   IOS_SIGNING_IDENTITY="Apple Development: You (ABCDE12345)" \
#   IOS_PROVISIONING_PROFILE_SPECIFIER="LoopDeck Dev" \
#   ./scripts/build_ipa.sh                 # signed IPA
#
# Environment:
#   IOS_DEPLOYMENT_TARGET   minimum iOS version              (default 17.0)
#   IOS_TEAM_ID             Apple team identifier            (enables signing)
#   IOS_SIGNING_IDENTITY    codesign identity name
#   IOS_PROVISIONING_PROFILE_SPECIFIER  profile name installed in the keychain
#   IOS_PROVISIONING_PROFILE_PATH       .mobileprovision file to install first
#   IOS_ALLOW_PROVISIONING_UPDATES      1 to let Xcode fetch/repair profiles
#   IOS_EXPORT_METHOD       development | ad-hoc | app-store-connect
#                           when set, the built archive is exported with
#                           xcodebuild -exportArchive instead of zipped
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build-ios"
DIST_DIR="${ROOT}/dist"
IPA_PATH="${1:-${DIST_DIR}/LoopDeck.ipa}"
DEPLOYMENT_TARGET="${IOS_DEPLOYMENT_TARGET:-17.0}"
APP_NAME="LoopDeck"
APPEX_NAME="LoopDeck.appex"

# zip runs from a temporary directory, so the IPA path must be absolute.
case "${IPA_PATH}" in
    /*) ;;
    *) IPA_PATH="$(pwd)/${IPA_PATH}" ;;
esac

DIST_DIR="$(dirname "${IPA_PATH}")"

log()  { printf '\033[1;36m==>\033[0m %s\n' "$*"; }
fail() { printf '\033[1;31merror:\033[0m %s\n' "$*" >&2; exit 1; }

# ------------------------------------------------------------------------------
# Preconditions
# ------------------------------------------------------------------------------
[[ "$(uname -s)" == "Darwin" ]] || fail "iOS builds need macOS with Xcode (this is $(uname -s))"
command -v xcodebuild >/dev/null 2>&1 || fail "xcodebuild not found; install Xcode"
command -v cmake >/dev/null 2>&1 || fail "cmake not found; brew install cmake"
command -v python3 >/dev/null 2>&1 || fail "python3 not found"

log "Xcode: $(xcodebuild -version | tr '\n' ' ')"
log "Deployment target: iOS ${DEPLOYMENT_TARGET}"

# ------------------------------------------------------------------------------
# Optional signing setup
# ------------------------------------------------------------------------------
SIGN_ARGS=()

if [[ -n "${IOS_PROVISIONING_PROFILE_PATH:-}" ]]; then
    [[ -f "${IOS_PROVISIONING_PROFILE_PATH}" ]] || fail "profile not found: ${IOS_PROVISIONING_PROFILE_PATH}"
    PROFILE_DEST="${HOME}/Library/MobileDevice/Provisioning Profiles"
    mkdir -p "${PROFILE_DEST}"
    cp "${IOS_PROVISIONING_PROFILE_PATH}" "${PROFILE_DEST}/loopdeck.mobileprovision"
    log "Installed provisioning profile"
fi

if [[ -n "${IOS_TEAM_ID:-}" ]]; then
    [[ -n "${IOS_SIGNING_IDENTITY:-}" ]] || fail "IOS_TEAM_ID is set but IOS_SIGNING_IDENTITY is not"
    SIGN_ARGS+=(
        "DEVELOPMENT_TEAM=${IOS_TEAM_ID}"
        "CODE_SIGN_STYLE=Manual"
        "CODE_SIGN_IDENTITY=${IOS_SIGNING_IDENTITY}"
        "CODE_SIGNING_REQUIRED=YES"
        "CODE_SIGNING_ALLOWED=YES"
    )

    if [[ -n "${IOS_PROVISIONING_PROFILE_SPECIFIER:-}" ]]; then
        SIGN_ARGS+=("PROVISIONING_PROFILE_SPECIFIER=${IOS_PROVISIONING_PROFILE_SPECIFIER}")
    fi

    if [[ "${IOS_ALLOW_PROVISIONING_UPDATES:-0}" == "1" ]]; then
        SIGN_ARGS+=("-allowProvisioningUpdates")
    fi

    log "Signing enabled for team ${IOS_TEAM_ID}"
else
    SIGN_ARGS+=("CODE_SIGNING_ALLOWED=NO" "CODE_SIGNING_REQUIRED=NO" "CODE_SIGN_IDENTITY=")
    log "Signing disabled: producing an unsigned IPA for sideloading"
fi

# ------------------------------------------------------------------------------
# Configure
# ------------------------------------------------------------------------------
log "Configuring the Xcode project"
cmake -S "${ROOT}" -B "${BUILD_DIR}" -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_SYSROOT=iphoneos \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="${DEPLOYMENT_TARGET}" \
    -DLOOPDECK_BUILD_TESTS=OFF \
    -DCMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH=NO

# ------------------------------------------------------------------------------
# Build
# ------------------------------------------------------------------------------
log "Building ${APP_NAME} (Release, arm64)"
set +u
xcodebuild \
    -project "${BUILD_DIR}/${APP_NAME}.xcodeproj" \
    -target "${APP_NAME}_Standalone" \
    -configuration Release \
    -sdk iphoneos \
    -arch arm64 \
    ONLY_ACTIVE_ARCH=NO \
    "${SIGN_ARGS[@]}" \
    build
set -u

# ------------------------------------------------------------------------------
# Locate and validate the product
# ------------------------------------------------------------------------------
APP_PATH="$(find "${BUILD_DIR}" -type d -name "${APP_NAME}.app" -not -path "*/${APP_NAME}.app/*" | head -n 1)"

[[ -n "${APP_PATH}" ]] || fail "could not find ${APP_NAME}.app in ${BUILD_DIR}"

log "App bundle: ${APP_PATH}"

APPEX_PATH="${APP_PATH}/PlugIns/${APPEX_NAME}"
[[ -d "${APPEX_PATH}" ]] || fail "the AUv3 extension is missing: ${APPEX_PATH}"

PLIST_BUDDY=/usr/libexec/PlistBuddy
APPEX_PLIST="${APPEX_PATH}/Info.plist"
APP_PLIST="${APP_PATH}/Info.plist"

extension_point="$("${PLIST_BUDDY}" -c "Print :NSExtension:NSExtensionPointIdentifier" "${APPEX_PLIST}" 2>/dev/null || true)"
[[ "${extension_point}" == "com.apple.AudioUnit-UI" ]] || fail "unexpected NSExtensionPointIdentifier: '${extension_point}'"

au_type="$("${PLIST_BUDDY}" -c "Print :NSExtension:NSExtensionAttributes:AudioComponents:0:type" "${APPEX_PLIST}" 2>/dev/null || true)"
au_subtype="$("${PLIST_BUDDY}" -c "Print :NSExtension:NSExtensionAttributes:AudioComponents:0:subtype" "${APPEX_PLIST}" 2>/dev/null || true)"
au_manufacturer="$("${PLIST_BUDDY}" -c "Print :NSExtension:NSExtensionAttributes:AudioComponents:0:manufacturer" "${APPEX_PLIST}" 2>/dev/null || true)"
bundle_id="$("${PLIST_BUDDY}" -c "Print :CFBundleIdentifier" "${APP_PLIST}" 2>/dev/null || true)"
appex_id="$("${PLIST_BUDDY}" -c "Print :CFBundleIdentifier" "${APPEX_PLIST}" 2>/dev/null || true)"

[[ "${au_type}" == "aumi" ]] || fail "AU component type is '${au_type}', expected 'aumi' (MIDI processor)"

log "Architecture: $(lipo -archs "${APP_PATH}/${APP_NAME}" 2>/dev/null || echo unknown)"
log "App bundle id:     ${bundle_id}"
log "AUv3 bundle id:    ${appex_id}"
log "AU component:      type=${au_type} subtype=${au_subtype} manufacturer=${au_manufacturer}"

# ------------------------------------------------------------------------------
# Package
# ------------------------------------------------------------------------------
mkdir -p "${DIST_DIR}"
rm -f "${IPA_PATH}"

if [[ -n "${IOS_EXPORT_METHOD:-}" ]]; then
    log "Archiving for export (${IOS_EXPORT_METHOD})"

    ARCHIVE_PATH="${BUILD_DIR}/${APP_NAME}.xcarchive"
    rm -rf "${ARCHIVE_PATH}"

    xcodebuild \
        -project "${BUILD_DIR}/${APP_NAME}.xcodeproj" \
        -target "${APP_NAME}_Standalone" \
        -configuration Release \
        -sdk iphoneos \
        -arch arm64 \
        ONLY_ACTIVE_ARCH=NO \
        "${SIGN_ARGS[@]}" \
        -archivePath "${ARCHIVE_PATH}" \
        archive

    EXPORT_PLIST="${BUILD_DIR}/ExportOptions.plist"

    {
        echo '<?xml version="1.0" encoding="UTF-8"?>'
        echo '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">'
        echo '<plist version="1.0">'
        echo '<dict>'
        echo "    <key>method</key><string>${IOS_EXPORT_METHOD}</string>"
        echo "    <key>teamID</key><string>${IOS_TEAM_ID:-}</string>"
        echo '    <key>signingStyle</key><string>manual</string>'
        echo '    <key>stripSwiftSymbols</key><true/>'
        echo '    <key>compileBitcode</key><false/>'
        echo '    <key>uploadSymbols</key><true/>'
        echo '</dict>'
        echo '</plist>'
    } > "${EXPORT_PLIST}"

    rm -rf "${DIST_DIR}/export"
    xcodebuild -exportArchive \
        -archivePath "${ARCHIVE_PATH}" \
        -exportPath "${DIST_DIR}/export" \
        -exportOptionsPlist "${EXPORT_PLIST}"

    EXPORTED_IPA="$(find "${DIST_DIR}/export" -maxdepth 1 -name '*.ipa' | head -n 1)"
    [[ -n "${EXPORTED_IPA}" ]] || fail "xcodebuild -exportArchive produced no IPA"
    mv "${EXPORTED_IPA}" "${IPA_PATH}"
else
    log "Packaging ${IPA_PATH}"

    STAGE="$(mktemp -d)"
    trap 'rm -rf "$STAGE"' EXIT
    mkdir -p "${STAGE}/Payload"
    cp -R "${APP_PATH}" "${STAGE}/Payload/"

    # -y keeps the framework symlinks intact, which iOS requires.
    ( cd "${STAGE}" && zip -qry "${IPA_PATH}" Payload )
fi

[[ -s "${IPA_PATH}" ]] || fail "IPA was not created"

log "Done: ${IPA_PATH} ($(du -h "${IPA_PATH}" | cut -f1))"
log "Install with AltStore, Sideloadly or Xcode. In AUM / Koala / Drambo look for:"
log "  MIDI processors -> Miiro Audio: LoopDeck"
