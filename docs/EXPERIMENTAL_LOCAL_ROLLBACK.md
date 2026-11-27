# Portable dirty-page snapshot provider: design draft

rbengine remains dependency-free. The shared networking experiment is
[recomp-net PR 56](https://github.com/RetroPortingToolKit/recomp-net/pull/56);
it does not belong in this library's runtime dependency graph.

The current `src/snap/rbe_snap_ring.c` owns opaque complete blobs. A proposed
portable page provider would own a retained base, ordered dirty post-images,
bounded eviction/rebasing and branch truncation. Engines would supply memory
regions, opaque device state and notifications from every CPU, DMA, GPU, HLE
and mod writer. Missing notifications invalidate restoration; they cannot be
hidden by a narrower digest or a ban on valid RAM/cartridge writes.

Snapshots describe state before tick T. Restore must validate all metadata and
device capacity before committing memory/device changes. Rebasing must preserve
every still-retained tick; failed allocation/capture must preserve the previous
history. A backward load followed by branch truncation must not leave dirty
notifications relative to the abandoned future. Keep a portable fallback if an
OS page-protection provider is later added.

This PR establishes the provider boundary for review. It implements no page
tracker and claims no copy-time or game performance improvement. The existing
blob API and native saves remain intact. The next proof is a real host with
complete writer coverage, save/load/replay/digest agreement and measured
capture/replay cost; only then should other hosts adopt it.
