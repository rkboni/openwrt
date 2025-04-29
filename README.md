# Openwrt with a slow-boat twist
This is exactly the great openwrt with following minor conveniences:

ppp packages no longer turned on by default, since we don't always need them.

Luci default feed now points to slow-boat branch which is identical to master except for removal of built-in ppp.

New ```configs``` directory contains preconfigured dumb AP configs with basic LuCI interface. symlink the config you want
```
ln -s configs/.config.habanero .config
```

Also added is a DFS watchdog service in `package/network/utils/wifi-mon` that monitors syslog for DFS shutdown and triggers radio restart. This is useful where interference is falsely detected as a radar, and prevents the wireless interface from starting. If its really there, then it will keep tripping on DFS detection. This is probably not legal, but its a PITA when power drops and the radios don't start due to some noise (not from radar).

The wifi setup for IPQ4x platforms in configs uses the high throughput CT non-commercial firmware, and support for 80211r.

Seems to be rock solid on these platforms so far. DSA is used on the switch. Works with vlans. Typical setup is a vlan trunk, with multple SSIDs on their own vlan. The age of the silicon (its "only" AC wifi) is a great example of stability comes with time. And its more than enough for domestic expectations.


### Added support for **TP-link Deco M5 (IPQ4019)**

Sysupgrade currently not working. Use factory image via TFTP recovery- see [here](https://github.com/dutchmillbytes/openwrt/commit/c666fd5bbbe483ec2844326110951b76b9c80f25) which is where the patches came from, with one difference that both GbE ports are part of LAN bridge, and there is no wan interface configured (makes no sense for a dumb AP).

The deco was the cheapest IPQ4x platform I could find and I purchased 3x of them used for AUD$80. I get sustained 500Mbps on my phone which is enough, and the 80211r works great.

Elsewhere I use a 8-devices Habanero dev board which gives me same performance but I get 4 GbE access ports for hardwired devices as well as the wifi. I modded the boards to use higher gain PCB antennas. \
\
&nbsp;
![OpenWrt logo](include/logo.png)

OpenWrt Project is a Linux operating system targeting embedded devices. Instead
of trying to create a single, static firmware, OpenWrt provides a fully
writable filesystem with package management. This frees you from the
application selection and configuration provided by the vendor and allows you
to customize the device through the use of packages to suit any application.
For developers, OpenWrt is the framework to build an application without having
to build a complete firmware around it; for users this means the ability for
full customization, to use the device in ways never envisioned.

Sunshine!

## Download

Built firmware images are available for many architectures and come with a
package selection to be used as WiFi home router. To quickly find a factory
image usable to migrate from a vendor stock firmware to OpenWrt, try the
*Firmware Selector*.

* [OpenWrt Firmware Selector](https://firmware-selector.openwrt.org/)

If your device is supported, please follow the **Info** link to see install
instructions or consult the support resources listed below.

## 

An advanced user may require additional or specific package. (Toolchain, SDK, ...) For everything else than simple firmware download, try the wiki download page:

* [OpenWrt Wiki Download](https://openwrt.org/downloads)

## Development

To build your own firmware you need a GNU/Linux, BSD or macOS system (case
sensitive filesystem required). Cygwin is unsupported because of the lack of a
case sensitive file system.

### Requirements

You need the following tools to compile OpenWrt, the package names vary between
distributions. A complete list with distribution specific packages is found in
the [Build System Setup](https://openwrt.org/docs/guide-developer/build-system/install-buildsystem)
documentation.

```
binutils bzip2 diff find flex gawk gcc-6+ getopt grep install libc-dev libz-dev
make4.1+ perl python3.7+ rsync subversion unzip which
```

### Quickstart

1. Run `./scripts/feeds update -a` to obtain all the latest package definitions
   defined in feeds.conf / feeds.conf.default

2. Run `./scripts/feeds install -a` to install symlinks for all obtained
   packages into package/feeds/

3. Run `make menuconfig` to select your preferred configuration for the
   toolchain, target system & firmware packages.

4. Run `make` to build your firmware. This will download all sources, build the
   cross-compile toolchain and then cross-compile the GNU/Linux kernel & all chosen
   applications for your target system.

### Related Repositories

The main repository uses multiple sub-repositories to manage packages of
different categories. All packages are installed via the OpenWrt package
manager called `opkg`. If you're looking to develop the web interface or port
packages to OpenWrt, please find the fitting repository below.

* [LuCI Web Interface](https://github.com/openwrt/luci): Modern and modular
  interface to control the device via a web browser.

* [OpenWrt Packages](https://github.com/openwrt/packages): Community repository
  of ported packages.

* [OpenWrt Routing](https://github.com/openwrt/routing): Packages specifically
  focused on (mesh) routing.

* [OpenWrt Video](https://github.com/openwrt/video): Packages specifically
  focused on display servers and clients (Xorg and Wayland).

## Support Information

For a list of supported devices see the [OpenWrt Hardware Database](https://openwrt.org/supported_devices)

### Documentation

* [Quick Start Guide](https://openwrt.org/docs/guide-quick-start/start)
* [User Guide](https://openwrt.org/docs/guide-user/start)
* [Developer Documentation](https://openwrt.org/docs/guide-developer/start)
* [Technical Reference](https://openwrt.org/docs/techref/start)

### Support Community

* [Forum](https://forum.openwrt.org): For usage, projects, discussions and hardware advise.
* [Support Chat](https://webchat.oftc.net/#openwrt): Channel `#openwrt` on **oftc.net**.

### Developer Community

* [Bug Reports](https://bugs.openwrt.org): Report bugs in OpenWrt
* [Dev Mailing List](https://lists.openwrt.org/mailman/listinfo/openwrt-devel): Send patches
* [Dev Chat](https://webchat.oftc.net/#openwrt-devel): Channel `#openwrt-devel` on **oftc.net**.

## License

OpenWrt is licensed under GPL-2.0
