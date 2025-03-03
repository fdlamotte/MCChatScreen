# MCChatScreen

A version of the companion radio for Heltec_v3 with screen enabled, which prints messages coming in.

## To compile the firmware

The firmware are build using PlatformIO on VSCode.

This repository only comes with firmware files, the MeshCore files come from the [main MeshCore repository](https://github.com/ripplebiz/MeshCore) managed by Scott Powell and should be provided apart. 

To setup the build environment, you should clone both repositories at the same level and add them to a VSCode workspace, new build targets should then be available.

 <pre>$ cd &lt;meshcode_workspace_dir&gt;
$ git clone https://github.com/ripplebiz/MeshCore
$ git clone https://github.com/fdlamotte/MCChatScreen</pre>

Then in PlatformIO, add both dirs to your workspace.

Once the environment is setup, you can customize the firmware by adding your code and writing your own build targets in `platformio.ini`.
