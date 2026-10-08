# The optional modern UI

OpenMoH Arena can show the menus and HUD of
[Project: Omaha](https://github.com/JayRewd/omaha) by JayRewd instead of the
original ones. It is off unless the player turns it on. With it off, the game
looks and behaves as it did before this was added.

Project: Omaha built its UI for Allied Assault and says its support for
Spearhead and Breakthrough is not complete yet.

## Turning it on

- Players: tick "Use the modern UI" in the Game Options of the MoH Arena
  launcher.
- Developers: start the client with `+set ui_legacy 0`.

`ui_legacy` is read once when the client starts. Its default is `1`, the
original UI. Only the command line can set it: a cfg file and the console
cannot. Every value other than `0` means the original UI.

The files of the modern UI are in `main/zz_moharena_ui.pk3`, which comes with
the release. In the original UI nothing reads them, and no cvar or command of
the modern UI exists.

Without those files the modern UI cannot show its main menu. So when the
client is started with `ui_legacy 0` and `ui/modern/main.xml` is not found, it
says so in the console and uses the original UI. The launcher checks for the
pack as well, and starts the original UI when it is missing.

## How it is built

| Part | Where |
|------|-------|
| Menus and HUDs as XML, with their images | `assets/main/ui/modern/` |
| Reading the XML into widgets and laying them out | `code/uidesign/` |
| Drawing | `code/uirender/` |
| The client code that connects them to the game | `code/client/cl_uirender.cpp` and the files beside it |

The pack is built from `assets/main` with:

```bash
python3 misc/moharena/make_modern_ui_pk3.py --output build/zz_moharena_ui.pk3
```

The same files always give the same pack, byte for byte.

The test programs of the modern UI are built only when asked for:

```bash
cmake -B build-tests -DBUILD_TESTING=ON -DMOHARENA_UI_TESTS=ON .
```

One of them, `test_uir_design`, ends with failed checks: 37 when the modern UI
was added, in these twelve tests. The same checks fail in Project: Omaha's own
sources, so a failed check in any other test is a new one.

`TestAmmoPanelBottomRowLayoutMaxClip32`, `TestEdgeClipShapePaint`,
`TestOpacityInheritancePaint`, `TestClassicHudLayoutGeometry`,
`TestCyclicSelect`, `TestSettingsCvarBindsResolve`,
`TestInheritedDropShadowPaint`, `TestButtonNestedShapeChild`,
`TestLeafImageItemFieldBind`, `TestShapeCvarBackedProps`,
`TestIntrinsicShapePropScale`, `TestMarqueeLabelPaint`.

## Limits in OpenMoH Arena

In Project: Omaha the XML decides what a control changes. Here the XML only
places and styles the controls. What they may do is a fixed list in
`code/client/cl_moharena_uipolicy.cpp`:

- **Settings.** A control can change only the settings in that list, and only
  to values MoH Arena's rules allow. For example texture detail (`r_picmip`)
  stops at 2. A setting whose value the rules fix has no control. The field
  of view has no control either: it is set in the F7 menu of MoH Arena Guard.
  The client still keeps `cg_fov` between 65 and 80.58 and never past a
  horizontal 98 on a wide screen.
- **Keys.** A key can be bound only to the commands in the list.
- **Commands.** A button can run only the commands the shipped menus use, such
  as joining a team, picking a weapon or voting.
- **Servers.** A server address may contain only address characters.
- **Files.** Layouts are read only from `.xml` files under `ui/modern/`. The
  menus and HUDs of the pack keep their names: a file with another path cannot
  take the name of one of them.

A loose file or another pack with a file of the same path replaces the pack's
file, as with any other game file. The lists still apply to whatever that file
says, so it can change how the modern UI looks but not what it may do.

The menus write a setting only when the player changes its control. When a
setting has a value that is not among the options of its row, the setting
keeps its value and the row shows the default option of its list until the
player changes it. The same goes for keys: opening the key screen changes no
bind.

These parts of Project: Omaha are not in OpenMoH Arena:

- its crosshair and its sniper scope: the game's own are drawn, and the
  custom crosshair of the F7 menu works as before;
- remote player prediction;
- its changes to the sound code;
- its changes to default settings, to the config file name and to the first
  start of the game;
- the steps that rewrote settings and binds on their own: the frame rate
  limit, the mouse input mode and the model detail when their values were not
  among the menu's options, binds on uppercase letter keys, and the older
  scoreboard binds;
- the menu actions the shipped menus do not use: `restart-video`,
  `reset-cvar`, `navigate`, `back`, `close`, `legacy-pushmenu`, `menu-open`
  and `menu-close`;
- the wait for CONTINUE on every loading screen: a multiplayer map starts by
  itself, and only the single-player game waits, where the loading screen is
  the briefing.

The console is always there with the modern UI, since its menus have no row
for it. `ui_console` is not changed, so the original UI keeps its own choice.

The scoreboard is not the one of Project: Omaha. Both team tables have the
same column order. The player's number is shown in every game, also on Allied
Assault servers, whose own scoreboard has no number column. The player's own
row is marked. In team games a dead player has a skull, dim text and a DEAD
tag; the game sends this in team games only, and the original scoreboard
shows the same thing with a text colour.

The HUD has a fourth style besides the three of Project: Omaha. "Arena" is put
together from them:

- from "Competitive": the compass, the kill feed and the top bar, with a
  square for every player, the round scores and the clock;
- from "Classic": the health bar, and the team and weapon menus;
- from "Modern": the weapon panel.

It also shows the vote prompt, the name and health of the player under the
crosshair and the voice message menu. Five settings size its parts. Their rows
are on the settings page while "Arena" is the chosen style:

| Row | Setting | Range | Default |
|-----|---------|-------|---------|
| Compass size | `ui_om_hud_compass_scale` | 0.5 to 2 | 1 |
| Top bar size | `ui_om_hud_topbar_scale` | 0.5 to 2 | 1 |
| Kill feed text size | `ui_om_hud_killfeed_size` | 10 to 32 | 16 |
| Messages text size | `ui_om_hud_messages_size` | 10 to 32 | 16 |
| Chat text size | `ui_om_hud_chat_size` | 10 to 32 | 16 |

The three text sizes are in pixels on a 1920 x 1080 screen, and follow the
resolution and "UI Scale" like the rest of the modern UI. The squares of the
top bar get smaller when a team has more than 12 players, so the bar keeps its
width.

`ui_compass_scale` of the original game sizes the original compass only. No
style of the modern UI reads it.

In the "Classic" style the voice message menu was a box with no size of its
own, so it was not drawn. It is a picture now, which takes the size of the
picture.

These come with the modern UI and are off or unchanged until the player sets
them:

- hit markers (`cg_hitmarker`, off by default), shown when the server reports
  a hit;
- a first-person view while spectating (off by default, with a key to switch);
- zoom sensitivity, where the default is the original behaviour.

## More to read

| Document | About |
|----------|-------|
| [designformat.md](designformat.md) | The XML format |
| [ui-rendering-pipeline.md](ui-rendering-pipeline.md) | How a frame is drawn |
| [`code/uirender/LIFECYCLE.md`](../../code/uirender/LIFECYCLE.md) | Start, connect and `vid_restart` |

These three are Project: Omaha's own texts, kept as written. They still
describe parts that are not in OpenMoH Arena: the crosshair and scope
previews, the menu actions listed above, and a control writing any setting a
menu file names. Where they differ from this page, this page and the code are
right.

The credits and licences of the parts by others are in
[`misc/moharena/THIRD-PARTY.txt`](../../misc/moharena/THIRD-PARTY.txt).
