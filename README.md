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

This repository is currently intended primarily for **source-code releases**.

Official compiled OpenMoH Arena binaries may be distributed separately in the future.

The absence of binaries from this repository does not mean the project cannot be built from source.

## Building

OpenMoH Arena inherits much of its build system from OpenMoHAA.

The project uses CMake and supports multiple platforms.

Build instructions will be documented here as the OpenMoH Arena build and release process is finalized.

Until then, OpenMoHAA's upstream build system and documentation may be useful references:

https://github.com/openmoh/openmohaa

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

