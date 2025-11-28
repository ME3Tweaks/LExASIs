SSRUnlocker (LE1)
=================

With Legendary Edition release in 2021, ME1 got a nice visual upgrade in the form
of realtime screen-space reflections in select places (such as Eden Prime starting
area, or Citadel embassies). However, intentionally or not, reflection probes placed
by BioWare seem to render only at half the expected frame rate, which leads to
very visibly choppy reflections.

This ASI hooks a function which determines for each reflection probe whether
it is time to re-render the scene, and bypasses it to always return true.
This makes the screen-space reflections re-render each frame.

Note that this ASI *will* increase your GPU utilization, the higher your
FPS target the bigger the impact. Originally the mod was called "GPUCrusher",
but thankfully it doesn't seem to have crushed any GPUs so far.
