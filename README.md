# xbox-iso-vfs

xbox-iso-vfs is a utility to mount Xbox ISO files on Windows.

The Dokan library is required for the kernel model file system driver. For more information on Dokan and how it works, see: https://github.com/dokan-dev/dokany#how-it-works

## Usage

    xbox-iso-vfs is a utility to mount Xbox ISO files on Windows
    Written by x1nixmzeng
    
    xbox-iso-vfs.exe [/d|/l] <iso_file> <mount_path>
      /d           Display debug Dokan output in console window
      <iso_file>   Path to the Xbox ISO file to mount
      <mount_path> Driver letter ("M:\") or folder path on NTFS partition
      /h           Show usage
    
    Unmount with CTRL + C in the console or alternatively via "dokanctl /u mount_path".


## What you can do with it

* Launch ISO files directly from emulators (such as Cxbx-Reloaded)
* Visually browse/copy files natively using Windows


## Installation

1. Download Dokan v1.4 from Github - [x64 installer][1]
2. Run the Dokan installer
3. Download the xbox-iso-vfs binary


## Similar Projects

The idea of an ISO file system in userspace (FUSE) is not new, even for the Xbox community
* [xbfuse][2] does exactly the same thing on Linux

There are also various other utilities extract ISO files
* https://github.com/XboxDev/extract-xiso
* https://github.com/zefie/zef-xiso-convert

Others apps (closed source)
* Qwix
* XDVDMulleter


## Credits

The implementation of `xdvdfs` was taken from https://github.com/thrimbor/xbiso


[1]:https://github.com/dokan-dev/dokany/releases/download/v1.4.0.1000/Dokan_x64.msi
[2]:https://github.com/multimediamike/xbfuse
