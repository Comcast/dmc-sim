dmc-sim
=======

ns-3 bindings to support simulation evaluations of 
Distributed Monotonic Clocks (DMC)

For more information on Distributed Monotonic Clocks, you may want to watch
[this talk](https://www.youtube.com/watch?v=YqNGbvFHoKM) from StrangeLoop 2015.

Note: There is a lot of undocumented installation hoo-hah to get ns-3 to
compile on the Ubuntu Vagrant box, mostly involves creating the VM with
enough memory and then bashing along following the instructions here:

https://www.nsnam.org/docs/release/3.22/tutorial/singlehtml/index.html

The files here are meant to be symlinked into the ns-3.22 source tree so that
builds can be done on the Vagrant box but editing done locally.

```
udp-gossip.{cc,h} -> src/applications/model
dmc-data.h -> src/applications/model
luby-mis.{cc,h} -> src/applications/model
simulated-clock.{cc,h} -> src/applications/model
udp-gossip-helper.{cc,h} -> src/applications/helper
```

Note also these files have to be added to the 'wscript' file in
src/applications under the ns-3.22 source tree.

