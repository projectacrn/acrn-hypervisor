# Build containers for Project ACRN

## Introduction

This folder contains a number of Dockerfile that include
all the build tools and dependencies to build the ACRN Project
components, i.e. the `hypervisor`, `devicemodel` and `tools`

The workflow is pretty simple and can be summarized in these few steps:

1. Build the *build containers* based on your preferred OS
1. Clone the Project ACRN repository
1. Start the build container and give it the repository
1. Build the Project ACRN components

The pre-requisite is that you have Docker installed on your machine.
Explaining how to install it on your system is beyond the scope of this
document, please visit https://www.docker.com for detailed instructions.

## Build the *build containers*

Each `Dockerfile` in this repo has an extension that tells what Linux
distribution it is based on. To build a container using any of those,
use this command:
```
$ sudo docker build -t <container-name> -f Dockerfile.<baseos> .
```

As an example, to build a container based on CentOS 7, do:
```
$ sudo docker build -t centos7 -f Dockerfile.centos7 .
```

## Clone Project ACRN

Follow these simple steps to clone the Project ACRN repositories
```
$ mkdir ~/acrn
$ cd ~/acrn
$ git clone https://github.com/projectacrn/acrn-hypervisor
```

## Start the build container

Use this `~/acrn/acrn-hypervisor` folder and pass it on to your build container:
```
$ cd ~/acrn/acrn-hypervisor
$ sudo docker run -ti -v $PWD:/root/acrn <container-name>
```

Using CentOS 7 again as an example, that gives us:
```
$ cd ~/acrn/acrn-hypervisor
$ sudo docker run -ti -v $PWD:/root/acrn centos7
```

**Note:** if you encounter permission issues within the container (as it
happens on a Fedora 27 host), try adding the `:z` parameter to the mount option.
This will unlock the permission restriction (that comes from SElinux). Your
command-line would then be:
```
$ cd ~/acrn/acrn-hypervisor
$ sudo docker run -ti -v $PWD:/root/acrn:z centos7
```

## Build the ACRN components

The steps above place you inside the container and give you access to
the Project ACRN repository you cloned earlier. You can now build any
of the components. Here are a few examples:
```
# make hypervisor PLATFORM=uefi
# make devicemodel
# make tools
```

If you want to build it all, simply do:
```
# make PLATFORM=uefi
```

You can do this for all build combinations.
All the build dependencies and tools are pre-installed in the container as well as a
couple of useful tools (`git` and `vim`) so you can directly edit files to experiment
from within the container.

The tools to build the ACRN documentation is (still) missing from these containers.
