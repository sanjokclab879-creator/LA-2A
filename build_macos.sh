#!/bin/bash
set -e

cmake -B build_mac -DCMAKE_BUILD_TYPE=Release
cmake --build build_mac --config Release -j$(sysctl -n hw.ncpu)

mkdir -p pkg_root/Library/Audio/Plug-Ins/VST3
mkdir -p pkg_root/Library/Audio/Plug-Ins/Components

cp -R "build_mac/LA2A_Clone_artefacts/Release/VST3/LA2A Clone.vst3" "pkg_root/Library/Audio/Plug-Ins/VST3/"
cp -R "build_mac/LA2A_Clone_artefacts/Release/AU/LA2A Clone.component" "pkg_root/Library/Audio/Plug-Ins/Components/"

# Р СџР С•Р Т‘Р С—Р С‘РЎРѓРЎРЉ ad-hoc, РЎвЂЎРЎвЂљР С•Р В±РЎвЂ№ macOS Gatekeeper Р Р…Р Вµ РЎР‚РЎС“Р С–Р В°Р В»РЎРѓРЎРЏ Р Р…Р В° Р С—Р С•Р Р†РЎР‚Р ВµР В¶Р Т‘Р ВµР Р…Р Р…РЎвЂ№Р в„– Р С—Р В°Р С”Р ВµРЎвЂљ
codesign --force --deep --sign - "pkg_root/Library/Audio/Plug-Ins/VST3/LA2A Clone.vst3"
codesign --force --deep --sign - "pkg_root/Library/Audio/Plug-Ins/Components/LA2A Clone.component"

pkgbuild --root pkg_root \
         --identifier com.sanjoklab.la2aclone.pkg \
         --version 1.0.0 \
         --install-location / \
         "LA2A_Clone_v1.0.0_macOS.pkg"

echo "Р вЂњР С›Р СћР С›Р вЂ™Р С›! Р Р€РЎРѓРЎвЂљР В°Р Р…Р С•Р Р†РЎвЂ°Р С‘Р С” РЎРѓР С•Р В·Р Т‘Р В°Р Р…: LA2A_Clone_v1.0.0_macOS.pkg"
