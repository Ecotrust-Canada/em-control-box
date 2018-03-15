em-control-box
==============

Electronic monitoring control box for commercial fisheries.

# How To Contribute

On a development system (you must choose this in grub, when booting, rather than a production image), the project source code is saved in /opt/em. To access a terminal, type `ctrl-alt-f1` and log in. First get a fresh version of the code in your dev environment:

```
cd /opt/em
git pull origin master
```

Then, make your changes. To revert a change to the original, just use `git checkout <file>`. When you're done, review them with:

```
git status
git diff
```

When you're happy with them, add your changes and commit them.

```
git add <file1>
git add <file2>
...
git commit -m "Add a description of what you did, here"
```

And then share them with the world!

```
git push origin master
```
# To Run Your Custom Build

To Build locally, just

```
em stop
cd /opt/em/src
make
em start
```

You're now running your own build of the EM software.

# How Build a New Image

If you are not adding or removing any files, and instead are just making fixes to existing files, it is as simple as:
`em buildem <release> <kernel version>`

The script assumes you have the kernel sources for <kernel version> in /usr/src/linux-<kernel version>
 
The script also requires two kernel configurations to be present in /opt/em/src, those are
`config-<release>-stage1` and
`config-<release>-stage2`
 
So for the most recent build, which I did with
```
em buildem 2.2.1 3.11.4
```
 
I had to have
/usr/src/linux-3.11.4
/opt/em/src/config-2.2.1-stage1
/opt/em/src/config-2.2.1-stage2
 
The config-2.2.1 files are just symlinks to the 2.2.0 versions as nothing has changed in the kernel conf.
 
If you ARE adding files, then be sure to add the exact path to the new file(s) in the same form as already present in
`/opt/em/src/files.lst`
 
This build system isn’t the most flexible yet:
- there’s no way to resume from some failure
- it assumes the presence of stuff without checking, or falling back
- I should really have a kernel conf per kernel rather than per release
- etc.
 
It will improve =)

The process it takes is as follows:
 
* mozplugger-update
* builds a clean em-rec
* builds Arduino hex images
* creates a skeleton root in /usr/src/em-<release>
* copies files from /opt/em/src/files.lst over to skeleton root
* copies node apps (cleaned up and stripped) over to skeleton root, using my “stripnodeapp” em command (you can look at the source to figure out what it does)
* strips all executables
* strips all libraries
* strips other archives
* runs ldconfig on new skeleton root
* builds linux kernel, modules
* copies modules directory over to new root
* creates initramfs CPIO archive out of new root
* rebuilds kernel again while injecting CPIO archive into binary image
* copies the finished product into /opt/em/images/
 
How to use images
After building, or after putting an already-built image (such as the one off dropbox) into /opt/em/images, you can do
`em upgrade /opt/em/images/em-2.2.1`
 
And that will mount /boot, copy the image there, append to the grub.cfg file, and unmount /boot


# How to Create An Image With Booatloader

For brand new boxes, the OS disk must first be imaged with a bootloader version of the EM image. To build this, simply clone the entire EM dev box disk to a file. First clean it up.

```
em stop
em clean
em resetelog
```

zero out empty blocks for compression.
```
mount /dev/sda1 /boot
dd if=/dev/zero of=/boot/zero
dd if=/dev/zero of=/zero
dd if=/dev/zero of=/var/em/zero
rm /boot/zero
rm /var/em/zero
rm /zero
```

Then boot into ubuntu, and image the disk (supposing it's sda here)
```
dd if=/dev/sda bs=1M | gzip -c > em-boot-<ver>.gz
```
