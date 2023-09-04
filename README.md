# Solar powered wireless sensor network research project

A research project into simulating energy consumption of wireless sensor nodes using the [Castalia](https://omnetpp.org/download-items/Castalia.html) simulator.

See my related paper [Allen J, Forshaw M, Thomas N: Towards an accurate and scalable energy harvesting wireless sensor network simulator model framework](https://eprints.ncl.ac.uk/file_store/production/237281/41F9437B-6E32-4A69-9203-DDC5333594AF.pdf) for motivation for this work.

## Network stack components

The following network stack components were written for this project to assess their suitability for use in solar powered wireless networks:

- MAC layer: [BoX-MAC 2](http://sing.stanford.edu/pubs/sing-08-00.pdf) and `RICER` (Receiver Initiated CyclEd Receivers)
- Routing layer: `Collection Tree Protocol` and `Min-Max Battery Cost Routing`


## Setup

GreenCastalia (v1.0) is built on Castalia (v3.3) which is built on Omnet++ (v4.6), so all these are installed in reverse order. 

The files in `src` and `Simulations` folders are customised Castalia modules and simulation parameter files used for this project.


### Install VirtualBox and Vagrant on the Ubuntu host

Do not rely on Ubuntu repos - they are very out of date. Install from VirtualBox / Hashicorp website

- Install (latest) virtualbox - tested with version 5.1.6
- Install (latest) virtualbox extension pack, match version with installed virtualbox version
- Install (latest) Vagrant - tested with version 1.8.5


### Checkout this project

### Download Omnet++

Manually download [Omnet++ v4.6 `omnetpp-4.6-src.tgz`](https://omnetpp.org/component/jdownloads/category/32-release-older-versions?Itemid=-1) to the vagrant-setup folder.

> Version compatibility: Castalia is based on OMNeT++. The latest version (Castalia 3.3) works with OMNeT versions 4.3 to 4.6 . Note that the **latest versions of OMNeT (5.x) are not compatible with Castalia**. Castalia has been tested with Ubuntu 14 and 16 and OS X 10.9. Note that Castalia is designed as a command line tool. Even though some people have adapted it to work with the OMNeT graphical IDE, it is not recommended (nor supported) to be used with it.

### Download GreenCastalia

Manually download [GreenCastalia `GreenCastalia-v0.1d.tar.bz2`](http://senseslab.di.uniroma1.it/greencastaliav01d) to the vagrant-setup folder.


### (Optional) Download Akaroa ###

This requires a license.

### (Optional) Modify VM specs ###

Edit the following in the Vagrantfile as appropriate for your host to allocate RAM and CPU cores to the guest VM:

```
  # VM specs
  vb.memory = 3072
  vb.cpus = 3
```

### Provision the VM ###

From the root project directory:

`vagrant up`

Look for any errors in the output. See the following files for how this provisioning is done and what happens:

* `Vagrantfile`
* `vagrant-setup/Bootstrap.sh`

Once the VM has been provisioned, **need to do a reboot** to complete installation of some packages and the VirtualBox guest additions. Do this by:

```
vagrant halt
vagrant up
```

### Login ###

Everything should now be ready to go. Log in to the guest VM with `vagrant ssh`.

## Development details ##

### Dev files ###

Are in `src` (contains Castalia modules consisting of Omnet NED, .cc and .h files) and `Simulations` (contains parameter .ini files).

### Building ###
The script `update-green-castalia.sh` is used to copy dev files into the Vagrant VM and build Castalia. It should be run from inside the VM.

`update-green-castalia.sh` if only NED and omnetpp.ini files have been modified (just copies files, no build)

`update-green-castalia.sh -b` if any of the .cc, .h or .msg files have been modified (Build)

`update-green-castalia.sh -r` if there are any new or deleted .cc, .h or .msg files (runs makemake and rebuilds)

### Running ###

`cd` to the appropriate simulation folder

Run the Castalia script (which is in `~/Castalia/Castalia/bin` - this folder should already have been added to PATH by Vagrant setup bootstrap)

`Castalia -c General -o results.txt -d` (run with the `General` configuration, output to results.txt, `-d` outputs further debug info



### Debugging ###

#### Build in debug mode ####

First, make sure omnet++ was built in debug mode. In the vagrant-setup/bootstrap.sh used to create the VM find the line for building Omnet with make. Ensure MODE is not set to release. If MODE is not specified, this is okay - defaults to building both debug and release

Note the -j argument - this specifies number of CPU cores to enable for simulations

`make -j3`

or

`make -j3 MODE=debug`

Then ensure Castalia is built in debug mode by editing makemake and ensuring -M arg is set to debug:

`OPTS=" -f -r --deep -o CastaliaBin -u Cmdenv -P $ROOT -M debug"`

Then rebuild Castalia (`update-green-castalia.sh -r`)

> Don't forget to set omnet and makemake back to release after debugging! (-M release)

#### GDB ####
Normally `~/Castalia/Castalia/bin/Castalia` is run, which is a Python script which does some clever interleaving of configuration sections (see Castalia docs), then calls on `~/Castalia/Castalia/CastaliaBin` which is the comiled Omnet executable

To debug we need to directly run CastaliaBin with gdb (this means we can only use a single configuration without the Castalia script's interleaving)

`gdb ~/Castalia/Castalia/CastaliaBin`

Set a breakpoint at function foo:

`(gdb) b foo`

Or at a line number 6 of file bar.cc:

`(gdb) b bar.cc:6`

Or given a specific condition:

`(gdb) break file1.c:6 if i >= ARRAYSIZE`

Run

`(gdb) r`

See GDB docs for more info

#### Remote debug using GUI on host with gdbgui ####

Use [gdbgui](https://github.com/cs01/gdbgui) interface on host machine to debug Castalia on guest VM

gdbgui should already have been installed on the guest VM by Vagrant. On host install pip3 and gdbgui: 

`sudo apt-get install python3-pip`

`pip3 install gdbgui --upgrade`

> On Ubuntu 16.04 this installs gdbgui in ~/.local/bin which is not included in your PATH. If so you need to add this to your path.

Then on the guest VM, `cd` to your simulation folder (containing the omnetpp.ini) and start gdbgui in remote mode:

`gdbgui -r`

By default gdbgui listens on port 5000. You will see in the Vagrantfile that port forwarding has been done to port 2345 (port 5000 was conflicting with another program on my host machine):

`config.vm.network "forwarded_port", guest: 5000, host: 2345`

This means that we can access port 5000 of the guest (which is where gdbgui is being served on) from the host's localhost port 2345. So we go to `http://127.0.0.1:2345` on our host browser

In gdbgui, load the CastaliaBin binary (relative to your simulation folder), for example:

`../../CastaliaBin`

The simulation will run in the debugger.

### Useful commands ###

Basic Castalia run:

`Castalia -c General -o results.txt`

Use configurations which vary a parameter, and repeat 3 times with different random number streams:

`Castalia -c varyDutyCycle,varyBeacon,varyTxPower -r 3`

View stats available:

`CastaliaResults -i results.txt`

View stats with LinkEstimator in the name

`CastaliaResults -i results.txt -s LinkEstimator`

Split results by node

`CastaliaResults -i results.txt -s LinkEstimator -n`

Use Valgrind (from the simulation directory) - we need to call the executable directly (Castalia is a link to this executable):

`valgrind ../../out/gcc-debug/CastaliaBin`

Use Valgrind to performance profile:

`valgrind --tool=callgrind ../../out/gcc-release/CastaliaBin`

This will output a cachegrind file in the current directory. Then use kcachegrind to parse/view the output (copy the file to host machine as this uses a GUI)

`kcachegrind`

~~### Building standalone executables ###~~

Doesn't work
~~https://stackoverflow.com/questions/35157723/running-simulation-in-omnet-with-standalone-file~~

### Using multple cores ###

`opp_runall -j3 ~/Castalia/Castalia/out/gcc-release/CastaliaBin -u Cmdenv -c General -r 1,2,3`

### Build Omnet++ / Green-Castalia statically for windows ###

Download / unzip Win32 version of Omnet 4.7

Modify config.user:

`LDFLAGS="NO_TCL=yes"`

`WITH_PARSIM=no`

`SHARED_LIBS=no` (to build static libraries - eg useful for CondorHT deployment?)

`NO_TCL=true`

Run contained mingwenv.exe - this runs an instance of MSYS (unix-like cmd shell on windows with build tools e.g. make etc). Configure and build omnetpp:

```
./configure
make
```

Checkout Castalia and merge in GreenCastalia into a folder to use for building the simulation:

```
git clone https://github.com/boulis/Castalia.git
cd Castalia/Castalia
```

Extract GreenCastalia files into Castalia/Castalia. Remove these files:

rm src/node/resourceManager/ResourceManager.h
rm src/node/resourceManager/ResourceManager.cc

(If using Boost) Add C++ Boost - download Boost zip for windows, extract, modify makemake file to include the Boost directory, e.g:

`EXTOPTS="-I /C/path/to/your/boost_1_66_0"`

Fix issue with M_PI on windows (e.g. in WindTurbine.h / .cc):

```
/* For compilation on windows:
	MS C++ does not include M_PI constant define, because it is not standard C/C++.
	WindTurbine.cc uses M_PI, so compile will fail on windows. Therefore add this
	hacky check to define M_PI if it doesn't exist:
 */
#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif
```

If you are building statically, need to modify the makemake file to include a few missing libs for the linker. Not entirely sure why this is needed. See the file `omnetpp\tools\win32\mingw32\lib\cmake\libxml2\libxml2-config.cmake` and also https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=396416 for some hints which led to this solution. Bascially the linker does not know where to get reference for certain gzip, winsock and character encoding functions called by libxml2.

Add the following to makemake:

`EXTOPTS="-lz -liconv -lws2_32"`

Without these, you may get undefined reference compile errors such as:

```
C:/Users/you/omnetpp/tools/win32/mingw32/bin/../lib/gcc/i686-w64-mingw32/4.9.2/../../../../lib\libxml2.a(xmlIO.o):(.text+0x22d): undefined reference to `deflateEnd'
C:/Users/you/omnetpp/tools/win32/mingw32/bin/../lib/gcc/i686-w64-mingw32/4.9.2/../../../../lib\libxml2.a(nanohttp.o):(.text+0x808): undefined reference to `WspiapiGetAddrInfo@16'
C:/Users/you/omnetpp/tools/win32/mingw32/bin/../lib/gcc/i686-w64-mingw32/4.9.2/../../../../lib\libxml2.a(encoding.o):(.text+0x11e6): undefined reference to `libiconv'
```


Generate the Makefile

```
./makemake
```

Modify the generated Makefile for a static build (if you want to deploy as a completely self-contained executable, e.g. for Condor) by adding `-static` flag at this point:

```
@echo Creating executable: $@
$(Q)$(CXX) $(LDFLAGS) -o $O/$(TARGET) -static $(OBJS) $(EXTRA_OBJS) $(AS_NEEDED_OFF) $(WHOLE_ARCHIVE_ON) $(LIBS) $(WHOLE_ARCHIVE_OFF) $(OMNETPP_LIBS)
```

Build

`make`

### Run on Windows as standalone exe ###

#### Dealing with NED files ####

Still need to have .ned files accessible, which are loaded dynamically at runtime.

Need to copy all .ned files from src folder, e.g:

```
cp --parents `find -name \*.ned*` /vagrant/ned
```


