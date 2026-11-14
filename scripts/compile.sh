#!/bin/sh
set -eu
# The caller supplies a NEW, empty output directory and an immutable source tree.
source_dir=$1
output_dir=$2
build_dir=$(mktemp -d /tmp/pro-x.XXXXXX)
cp -a "$source_dir/." "$build_dir/"
cd "$build_dir"
chmod +x contrib/pspgz.py Installer/create_module_include.py PXE/Launcher/create_module_include.py
# GCC 9 retired -mno-crt0. These links already specify -nostartfiles.
mkdir /tmp/pro-x-bin
cat > /tmp/pro-x-bin/psp-gcc <<'EOF'
#!/bin/sh
for argument do
    if [ "$argument" != '-mno-crt0' ]; then
        set -- "$@" "$argument"
    fi
    shift
done
exec /usr/local/pspdev/bin/psp-gcc "$@"
EOF
chmod +x /tmp/pro-x-bin/psp-gcc
export PATH="/tmp/pro-x-bin:$PATH"
make CONFIG_661=1 build_lib
if [ -d Updater ]; then
    sh /work/scripts/compile-updater.sh "$build_dir/Updater" "$(basename "$output_dir")" library-only
fi
# Same dependency order as upstream, deliberately excluding IPL/permanent tools.
for component in CrossFW Rebootex_bin Rebootex Recovery Vshctrl usbdevice SystemControl ISODrivers/Galaxy ISODrivers/Inferno Stargate Satelite Popcorn Installer PXE/SystemControlPXE PXE/RebootexPXE; do
    if [ "$component" = Vshctrl ]; then
        # Upstream rebuilds shared Common objects after Recovery.
        rm -f ./Common/*.o
    fi
    make -C "$component" CONFIG_661=1
done
cp PXE/RebootexPXE/rebootex.h PXE/Launcher/rebootex.h
make -C PXE/Launcher CONFIG_661=1
make -C FastRecovery CONFIG_661=1
if [ -d Updater ]; then
    # The installer shares storage/JSON/SHA-256 sources, not its private bridge.
    # Recompile the client objects with its own flags and build identity.
    rm -f Updater/*.o
    sh /work/scripts/compile-updater.sh "$build_dir/Updater" "$(basename "$output_dir")"
    mkdir -p "$output_dir/PSP/GAME/PROXUPDATE"
    cp Updater/EBOOT.PBP Updater/cacert.pem "$output_dir/PSP/GAME/PROXUPDATE/"
fi
# Separate opt-in identification tool, never part of the installer payload.
if [ -d tools/wpa2diag ]; then
    # Common/wpa2_patch.o previously used kernel SystemControl flags.
    rm -f Common/wpa2_patch.o
    make -C tools/wpa2diag CONFIG_661=1
    mkdir -p "$output_dir/diagnostics/PSP/GAME/WPA2DIAG"
    cp tools/wpa2diag/EBOOT.PBP "$output_dir/diagnostics/PSP/GAME/WPA2DIAG/"
fi
mkdir -p "$output_dir/PSP/GAME/PROUPDATE" "$output_dir/PSP/GAME/FastRecovery"
cp PXE/Launcher/EBOOT.PBP "$output_dir/PSP/GAME/PROUPDATE/"
cp FastRecovery/EBOOT.PBP "$output_dir/PSP/GAME/FastRecovery/"
# Keep build evidence separate from the installation payload.
mkdir "$output_dir/modules"
for module in Rebootex/Rebootex.prx Recovery/recovery.prx Vshctrl/vshctrl.prx usbdevice/usbdevice.prx SystemControl/systemctrl.prx ISODrivers/Galaxy/galaxy.prx ISODrivers/Inferno/inferno.prx Stargate/stargate.prx Satelite/satelite.prx Popcorn/popcorn.prx; do
    cp "$module" "$output_dir/modules/"
done
cp PXE/Launcher/PARAM.SFO "$output_dir/installer.PARAM.SFO"
cp FastRecovery/PARAM.SFO "$output_dir/recovery.PARAM.SFO"
cp -a "$build_dir" "$output_dir/build-tree"
