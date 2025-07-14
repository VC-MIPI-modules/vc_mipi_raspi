# Building the Debian package

Running dpkg-buildpackage by script creates a Debian package

If different Linux headers are used, 
the path can be specified by the environmental variable ```KERNEL_HEADERS```
I.e. if the Linux kernel is built from sources, 
the path is set by ```KERNEL_HEADERS=/path/to/linux-gitrepo```

## Requirements
```shell
sudo apt-get install debhelper-compat  dkms dh-dkms debhelper  linux-headers-generic 
```
Init submodule 
```shell
git submodule update --init --recursive
```
## Script

```shell
bash createDebianPackage.sh
```