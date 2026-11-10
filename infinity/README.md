# Infinity build of Spotify Radio

Bullet4fun's Spotify Radio (`spotify-radio/`, GPL-3.0-or-later) built as an ExtraOptions plugin for
[Infinity Mods](https://github.com/anarqz/InfinityMods), instead of a mod of the Azimuth Mod Loader.
The changes are the commits on branch `infinity` on top of FH Mods Collection 2.3.0 (`5445c69`); all of them
sit behind `INFINITY_EO`, so upstream's own `build.py` still builds the original DLL unchanged.

With `-DINFINITY_EO=1`:

- `infinity/eo_plugin.c` exports `FH2PluginInitialize` (ExtraOptions plugin ABI 1) and calls the mod's start-up
  (`spotify_radio_load` in `spotify_mod.c`, which is `AzimuthModLoad` otherwise) with an `AzimuthModHost`:
  `data_dir` = the mod's folder (the parent of `plugins\`), `mods_dir` = `<Azimuth>\mods\`, `game` = FH2.
  The DLL exports neither `AzimuthModLoad` nor `AzimuthModGames`, so the Azimuth Mod Loader never loads it.
- ExtraOptions starts plugins while the game runs, so `install_jmp_hook` suspends the game's other threads
  while it writes each hook, and moves a thread stopped inside the patched bytes to the same offset of the
  trampoline (`infinity_freeze_threads`).
- Nothing is written outside the mod's own folder: no logo copy into the game's `Mount` folder (the logo
  ships in the mod's ExtraOptions `media` folder as `ui\textures\Anthem\HUD\HUD_RadioIcon_Xbox_Music.swatchbin`)
  and no move of an older `mods\FH2_SpotifyRadio\` install's files.
- It stays off (and says so in its log) when the original `FH_SpotifyRadio.dll` or `FH_LineInRadio.dll` is in
  `<Azimuth>\mods\`, as the originals do for their older `FH2_` DLLs: two hooks on the same station crash the game.

Build: Infinity Mods' `tools/infmods.py build spotifyradio` (Zig 0.15.2, upstream's flags plus the two defines).
