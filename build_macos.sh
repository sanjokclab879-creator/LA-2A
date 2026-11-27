#!/bin/bash
set -e

cmake -B build_mac -DCMAKE_BUILD_TYPE=Release
cmake --build build_mac --config Release -j$(sysctl -n hw.ncpu)

mkdir -p pkg_root/Library/Audio/Plug-Ins/VST3
mkdir -p pkg_root/Library/Audio/Plug-Ins/Components

cp -R "build_mac/LA2A_Clone_artefacts/Release/VST3/LA2A Clone.vst3" "pkg_root/Library/Audio/Plug-Ins/VST3/"
cp -R "build_mac/LA2A_Clone_artefacts/Release/AU/LA2A Clone.component" "pkg_root/Library/Audio/Plug-Ins/Components/"

# РџРѕРґРїРёСЃСЊ ad-hoc, С‡С‚РѕР±С‹ macOS Gatekeeper РЅРµ СЂСѓРіР°Р»СЃСЏ РЅР° РїРѕРІСЂРµР¶РґРµРЅРЅС‹Р№ РїР°РєРµС‚
codesign --force --deep --sign - "pkg_root/Library/Audio/Plug-Ins/VST3/LA2A Clone.vst3"
codesign --force --deep --sign - "pkg_root/Library/Audio/Plug-Ins/Components/LA2A Clone.component"

pkgbuild --root pkg_root \
         --identifier com.sanjoklab.la2aclone.pkg \
         --version 1.0.0 \
         --install-location / \
         "LA2A_Clone_v1.0.0_macOS.pkg"

echo "Р“РћРўРћР’Рћ! РЈСЃС‚Р°РЅРѕРІС‰РёРє СЃРѕР·РґР°РЅ: LA2A_Clone_v1.0.0_macOS.pkg"
