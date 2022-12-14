![header image](ply_header.svg "header image")
## Overview
Plymouth is an application that runs very early in the boot process (even before the root filesystem is mounted!) that provides a graphical boot animation while the boot process happens in the background.

It is designed to work on systems with DRM modesetting drivers. The idea is that early on in the boot process the native mode for the computer is set, plymouth uses that mode, and that mode stays throughout the entire boot process up to and after X starts. Ideally, the goal is to get rid of all flicker during startup.

For systems that don't have DRM mode settings drivers, plymouth falls back to text mode (it can also use a legacy `/dev/fb` interface).

In either text or graphics mode, the boot messages are completely occluded.  After the root file system is mounted read-write, the messages are dumped to `/var/log/boot.log`. Also, the user can see the messages at any time during boot up by hitting the escape key.

## Installation
Plymouth isn't really designed to be built from source by end users. For it to work correctly, it needs integration with the distribution. Because it starts so early, it needs to be packed into the distribution's initial ram disk, and the distribution needs to poke plymouth to tell it how boot is progressing.

### Binary Files
plymouth ships with two binaries:
- `/sbin/plymouthd` and
- `/bin/plymouth`

The first one, `plymouthd`, does all the heavy lifting. It logs the session and shows the splash screen. The second one, `/bin/plymouth`, is the control interface to `plymouthd`.

It supports things like plymouth `show-splash`, or plymouth `ask-for-password`, which trigger the associated action in `plymouthd`.

Plymouth supports various "splash" themes which are analogous to screensavers, but happen at boot time. There are several sample themes shipped with plymouth, but most distributions that use plymouth ship something customized for their distribution.

## Current Efforts
Plymouth isn't done yet. It's still under active development, but is used in several popular distros already, including Fedora, Mandriva, Ubuntu and others.  See the distributions page for more information.

## Code of Conduct
As with other projects hosted on freedesktop.org, Plymouth follows its Code of Conduct, based on the Contributor Covenant[^1]. Please conduct yourself in a respectful and civilized manner when using the above mailing lists, bug trackers, etc:

### References
[^1]: [freedesktop.org Contributor Covenant Code of Conduct](https://www.freedesktop.org/wiki/CodeOfConduct)
