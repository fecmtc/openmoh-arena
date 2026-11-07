# OpenMoH Arena Reloaded

This is the `mohaa-reloaded` branch of [OpenMoH Arena](https://github.com/fecmtc/openmoh-arena): the OpenMoH Arena client built on [MoHAA-Reloaded](https://github.com/fortemoh/MoHAA-Reloaded) by pstngh, a fork of [OpenMoHAA](https://github.com/openmoh/openmohaa) with a native Vulkan renderer. It is developed for use with [MoHArena](https://moharena.com/).

OpenMoH Arena's `main` branch stays on OpenMoHAA's renderers. This branch is a second build of the same client, released next to it with tags named `vX.Y.Z-reloaded`.

> OpenMoH Arena is an independent derivative project. It is not affiliated with or endorsed by the OpenMoHAA project, MoHAA-Reloaded, Project: Omaha or Electronic Arts.

## What this branch is

* MoHAA-Reloaded's `vulkan` branch, merged with OpenMoH Arena's `main`.
* The Vulkan renderer is linked into the game, so the SHA-256 that MoH Arena Guard checks covers it. OpenMoHAA's GL renderers are not built.
* The MoH Arena Guard module and its F7 pages, drawn through `code/renderervk/tr_moharena.c`.
* The gameplay work of `main`: the first-person spectator view, kill colours, personal mute, the sniper scopes, the terrain collision of grenades, the weapon commands.
* MoHAA-Reloaded's movement, input timing and defaults (`com_maxfps 125`, `cl_maxpackets 125`, `cl_movefps 100`, `cl_movesubgrid 1`, `r_uselod 0`). `cl_nullbinds` is allowed and off by default.
* The original UI only: the optional modern UI of `main` is not built (`ui_legacy` is held to 1).
* The stock image crosshair (`cg_crosshair`), not MoHAA-Reloaded's drawn one.
* No MoHAA-Reloaded launcher: MoH Arena Guard starts the game. `launcher/desktop` is neither built nor shipped.

Not on this branch: `main`'s PNG screenshots, Curve mouse acceleration and grenade charge bar, and Project: Omaha's `cl_cmdfps` (MoHAA-Reloaded has `cl_movefps`).

## Sound

Two sound mixers, picked with the cvar `s_backend`:

| `s_backend` | Mixer |
|---|---|
| `openal` (default) | OpenAL Soft, as in OpenMoHAA and `main`. The library ships next to the game (`OpenAL64.dll`, `libopenal.so.1`). |
| `omaha` | The mixer of [Project: Omaha](https://github.com/JayRewd/omaha) by JayRewd, as MoHAA-Reloaded carries it (`code/client/omaha`). With `s_speaker_type 1` (headphones) it gives 3D sound with the SADIE KU100 set in `main/sound/prom/hrtf`. |

The change takes effect after `snd_restart` or a restart of the game. MoH Arena Guard has an "Omaha sound mixer" box for it. When the Omaha mixer cannot start, OpenAL plays for that session.

## Binaries

Each `vX.Y.Z-reloaded` release on the [Releases page](https://github.com/fecmtc/openmoh-arena/releases) comes with a Windows x64 zip and a Linux x64 tarball. Each holds the game, its libraries, the headphone set, `COPYING.txt`, `SOURCE-OFFER.txt` (with the commit and library versions), `THIRD-PARTY.txt`, `OMAHA-SOUND-LICENSES.txt`, the libraries' licences and `SHA256SUMS.txt`. The Linux tarball has no dedicated server.

These are the builds that [MoH Arena Guard](https://moharena.com/anti-cheat) accepts. Guard finds out by itself whether a game folder holds the `main` build or the Reloaded one. It checks the SHA-256 of the exe, so it does not start a build made anywhere else, even from the same source.

## Building

The workflows that make the release builds list every library version and build option:

| Workflow | Builds |
|---|---|
| [moharena-windows-reloaded.yml](.github/workflows/moharena-windows-reloaded.yml) | Windows x64 (MSVC): SDL3 3.4.18, OpenAL Soft 1.24.3, Vulkan headers v1.4.341 |
| [moharena-linux-reloaded.yml](.github/workflows/moharena-linux-reloaded.yml) | Linux x64 (Ubuntu 22.04, clang, glibc 2.34): the same libraries |

MoHAA-Reloaded's own workflows are kept as they are.

To build locally, the renderer needs the Vulkan headers ([Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers), or the Vulkan SDK) and SDL3. Vulkan itself is loaded when the game runs, through SDL.

```bash
cmake -B build -DMOHARENA=ON -DVulkanHeaders_DIR=<headers>/share/cmake/VulkanHeaders -DSDL3_DIR=<sdl3>/lib/cmake/SDL3
cmake --build build
```

## Credits

* The OpenMoHAA team, for the engine (built on ioquake3 and the F.A.K.K SDK).
* pstngh, for MoHAA-Reloaded and its Vulkan renderer; notuern, for testing and ideas.
* JayRewd, for Project: Omaha and its sound mixer.

See `misc/moharena/THIRD-PARTY-reloaded.txt` and `code/thirdparty/OMAHA-SOUND-LICENSES.txt` for the parts by others and their licences.

## License

OpenMoH Arena is based on OpenMoHAA and includes software distributed under the GNU General Public License version 2 or later. See `COPYING.txt`.

## Game assets

This repository does **not** provide the original commercial Medal of Honor: Allied Assault game assets. Users are responsible for obtaining any required original game data through lawful means.

## Links

* MoHArena: https://moharena.com/
* OpenMoH Arena source: https://github.com/fecmtc/openmoh-arena
* MoHAA-Reloaded: https://github.com/fortemoh/MoHAA-Reloaded
* OpenMoHAA upstream: https://github.com/openmoh/openmohaa
