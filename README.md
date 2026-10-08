# OpenMoH Arena

**OpenMoH Arena** is a competitive-focused MOHAA engine and client based on [OpenMoHAA](https://github.com/openmoh/openmohaa).

It is being developed for competitive Medal of Honor: Allied Assault play and for integration with [MoHArena](https://moharena.com/).

> OpenMoH Arena is an independent derivative project. It is not an official OpenMoHAA release and is not affiliated with or endorsed by the OpenMoHAA project or Electronic Arts.

## Purpose

OpenMoH Arena focuses on the needs of competitive MOHAA players, match administrators, tournament organizers, spectators, and developers.

Areas of development may include:

* competitive multiplayer improvements
* match administration
* tournament-oriented functionality
* spectator improvements
* competitive quality-of-life features
* improved diagnostics
* networking and stability improvements
* security and integrity improvements
* integration with MoHArena services
* improvements inherited from OpenMoHAA

The intention is to remain reasonably close to OpenMoHAA while introducing functionality specifically useful for organized competitive play.

## Project status

OpenMoH Arena is under active development.

This repository contains **publicly released source code**.

Development may occur privately between public source releases, so this repository may not always reflect the latest development version.

Public releases represent selected source snapshots that are considered suitable for publication.

## Source releases

Versions published in this repository are tagged using semantic-style version numbers such as:

```text
v0.1.0
v0.2.0
v1.0.0
```

The Git tag for a release identifies the corresponding public source version.

## Binaries

Each release on the
[Releases page](https://github.com/fecmtc/openmoh-arena/releases) comes with
Windows builds for x64 (64-bit Windows) and x86 (32-bit Windows).

Each zip holds the game's executables and libraries, the pack of the
[optional modern UI](#optional-modern-ui) (`main/zz_moharena_ui.pk3`),
`COPYING.txt`, `SOURCE-OFFER.txt`, `THIRD-PARTY.txt` and `SHA256SUMS.txt`. The
release notes name the commit the builds were made from.

These are the builds that [MoH Arena Guard](https://moharena.com/anti-cheat)
accepts.
Guard checks the SHA-256 of `openmohaa.exe`, so it does not start a build made
anywhere else, even from the same source.

Builds for Linux and macOS are not published yet.

## Installing on Windows

1. Download the zip for your Windows: x64 for 64-bit Windows, x86 for 32-bit.
2. Extract it into your Medal of Honor: Allied Assault folder, the one that
   holds `main`. The zip has a `main` folder of its own with one file, which
   goes into yours.
3. Start `launch_openmohaa_base.exe` (Allied Assault),
   `launch_openmohaa_spearhead.exe` (Spearhead) or
   `launch_openmohaa_breakthrough.exe` (Breakthrough), or pick one of them in
   MoH Arena Guard.

## Differences from OpenMoHAA

The changes are marked `Added in MoH Arena` in the source, and the
[differences page](docs/markdown/01-intro/04-differences.md#openmoh-arena)
lists what players see. In short:

* Windows builds can load the MoH Arena Guard module, which adds the F7 Guard
  menu and the other MoH Arena features. The module loads only when Guard
  starts the game and the module file has the SHA-256 that Guard gives.
  In any other case the game runs like OpenMoHAA.
* An [optional modern UI](#optional-modern-ui), off by default.
* A first-person spectator view that a server can switch on for everyone who
  follows a player (the serverinfo key `moharena_firstperson`).
* A weapon command waits until the server has taken the one before, so a fast
  turn of the mouse wheel no longer drops the weapon on servers of the
  original game.
* On Allied Assault servers a crouched player is predicted with the right
  height, so the view no longer shakes under a low ceiling.

OpenMoH Arena uses the same network protocol as OpenMoHAA, so it joins the
same servers.

## Optional modern UI

OpenMoH Arena can show the menus and HUD of
[Project: Omaha](https://github.com/JayRewd/omaha) by JayRewd instead of the
original ones.

It is off by default. The player turns it on in MoH Arena Guard with the
"Use the modern UI" box, and Guard then starts the client with
`+set ui_legacy 0`. With it off, the game looks and behaves as it did before.

Rules for work on it:

* the original UI stays the default, and nothing of the modern UI may change it
* what the modern UI may change is the fixed list in
  `code/client/cl_moharena_uipolicy.cpp`, never the XML
* a setting it offers must stay inside MoH Arena's rules
* it changes a setting or a key bind only when the player changes the control
  for it
* its files ship as one pack, `main/zz_moharena_ui.pk3`, built by
  `misc/moharena/make_modern_ui_pk3.py`

Project: Omaha built its UI for Allied Assault. Spearhead and Breakthrough are
not complete in it yet.

See `docs/modern-ui/README.md` for how it is built and what was left out, and
`misc/moharena/THIRD-PARTY.txt` for the credits and licences of the parts by
others.

## Building

OpenMoH Arena builds like OpenMoHAA, with CMake. OpenMoHAA's build
documentation applies:

https://github.com/openmoh/openmohaa

The MoH Arena bridge is off by default. The Windows release builds turn it on
with `-DMOHARENA=ON`. The workflow that makes them,
[moharena-windows.yml](.github/workflows/moharena-windows.yml), lists every
library version and build option.

## Relationship to OpenMoHAA

OpenMoH Arena is derived from OpenMoHAA.

Upstream OpenMoHAA:

https://github.com/openmoh/openmohaa

OpenMoH Arena periodically incorporates relevant upstream fixes and improvements while maintaining its own competitive-focused changes.

Where practical, generic fixes that are useful beyond OpenMoH Arena may be suitable for contribution back to upstream.

## MoHArena

OpenMoH Arena is intended to complement:

https://moharena.com/

MoHArena is the competitive platform and community layer, while OpenMoH Arena is intended to provide the game-engine/client side of that ecosystem.

Conceptually:

```text
MoHArena
Competitive platform, matches, community and services

        +

OpenMoH Arena
Competitive MOHAA engine and client
```

## Compatibility

Compatibility goals include preserving support for existing MOHAA gameplay and content wherever practical.

However, competitive functionality may require behavior that differs from stock MOHAA or upstream OpenMoHAA.

Any important compatibility differences should be documented as the project develops.

## Contributing

The public repository may accept issues, bug reports, technical discussion, and contributions in the future.

Before contributing code, please keep changes focused and avoid unnecessary large-scale refactoring.

Changes that improve OpenMoHAA generally rather than specifically serving OpenMoH Arena may also be appropriate for contribution to the upstream OpenMoHAA project.

More detailed contribution guidelines will be added as the project matures.

## License

OpenMoH Arena is based on OpenMoHAA and includes software distributed under the **GNU General Public License version 2**.

See:

```text
COPYING.txt
```

for the license text included with the project.

Original OpenMoHAA copyright and attribution notices are preserved in accordance with the applicable license requirements.

OpenMoH Arena modifications and additions to the GPL-covered codebase are distributed under the applicable GPL terms.

## Game assets

This repository does **not** provide the original commercial Medal of Honor: Allied Assault game assets.

Users are responsible for obtaining any required original game data through lawful means.

The open-source engine code and the original MOHAA game content are separate.

## Disclaimer

OpenMoH Arena is an independent community project.

It is not affiliated with, sponsored by, or endorsed by Electronic Arts, the original Medal of Honor developers, or the OpenMoHAA maintainers.

Medal of Honor and related names and assets may be trademarks or copyrighted works of their respective owners.

## Links

* MoHArena: https://moharena.com/
* OpenMoHAA upstream: https://github.com/openmoh/openmohaa
* OpenMoH Arena source: https://github.com/fecmtc/openmoh-arena

