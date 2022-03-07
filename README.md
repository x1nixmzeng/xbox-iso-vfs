# xbox-iso-vfs
[![Build status](https://ci.appveyor.com/api/projects/status/dp43t000dnga9w3m?svg=true)](https://ci.appveyor.com/project/x1nixmzeng/xbox-iso-vfs)

xbox-iso-vfs is a utility to mount Xbox ISO files as a file system on Windows

Note that the Dokan library is required for the kernel model file system driver. For more information on Dokan and how it works, [click here](https://github.com/dokan-dev/dokany#how-it-works).

Once mounted, the files can be
* Used natively by emulators (such as Cxbx-Reloaded)
* Used to view or extract files using Windows Explorer

<img width="456" alt="Usage example" src="https://user-images.githubusercontent.com/327967/103487886-76800500-4e00-11eb-8b74-81a3e890a1c1.png">


## Usage

    xbox-iso-vfs.exe [/d|/l] <iso_file> <mount_path>
      /d           Display debug Dokan output in console window
      /l           Open Windows Explorer to the mount path
      <iso_file>   Path to the Xbox ISO file to mount
      <mount_path> Driver letter ("M:\") or folder path on NTFS partition
      /h           Show usage
    
    Unmount with CTRL + C in the console or alternatively via "dokanctl /u mount_path".


## Installation

1. Download Dokan v2 - [x64](https://github.com/dokan-dev/dokany/releases/download/v2.0.3.2000/Dokan_x64.msi)/[x86](https://github.com/dokan-dev/dokany/releases/download/v2.0.3.2000/Dokan_x86.msi) or [all releases](https://github.com/dokan-dev/dokany/releases/tag/v2.0.3.2000)
2. Run the Dokan installer
3. Download xbox-iso-vfs - [latest version](https://github.com/x1nixmzeng/xbox-iso-vfs/releases/download/v1.1/Release.zip)


## History

* Mar 2022 - Upgraded Dokan from v1.4 to v2.0 to match new API and improvements
* Jan 2021 - Initial version


## Similar Projects

The idea of an ISO file system in userspace (FUSE) is not new, even for the Xbox community
* [xbfuse](https://github.com/multimediamike/xbfuse) does exactly the same thing on Linux

There are also various other utilities extract ISO files
* [extract-xiso](https://github.com/XboxDev/extract-xiso)
* [xbiso](https://github.com/thrimbor/xbiso)
* [zef-xiso-convert](https://github.com/zefie/zef-xiso-convert)

Others apps (closed source)
* Qwix
* XDVDMulleter


## Thanks

The implementation of `xdvdfs` was taken from [xbiso](https://github.com/thrimbor/xbiso)
