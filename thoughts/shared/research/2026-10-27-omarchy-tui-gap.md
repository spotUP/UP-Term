---
date: 2026-10-27
topic: What Omarchy does that Amarchy does not - startup logo, font sizes, menu actions with TUI pages; gap list
status: research
---

# Omarchy vs Amarchy gap (logo, font sizes, richer TUI)

Source: shallow clone of basecamp/omarchy master (scratchpad `omarchy/`, read-only, no script run). Paths below are repo paths.
Important correction to older notes: the Omarchy menu is no longer a `bin/omarchy-menu` script plus walker. `bin/omarchy-menu` is a 52-line wrapper
that sends IPC to the Quickshell plugin `omarchy.menu` (`shell/plugins/menu/Menu.qml`, `MenuModel.js`); the menu tree is DATA in
`default/omarchy/omarchy-menu.jsonc` (462 lines, dotted ids are the tree, `action` = shell command, user overlay `~/.config/omarchy/extensions/omarchy-menu.jsonc`; schema in `docs/menu.md`).
Section 3 was built from that file (ids, labels, actions parsed with a script; counts below are from the parsed ids, 377 entries incl. dynamic providers not expanded).

## 1 Startup logo

One ASCII logo, one PNG logo, used in five places:

| Where | What happens | Files |
|---|---|---|
| Boot / disk unlock / shutdown | Plymouth theme: centred `logo.png` sprite, fake-then-real progress bar (fake to 70 percent over 15 s, `fake_progress_limit` / `fake_progress_duration`), password entry with lock and bullet images for LUKS | `default/plymouth/omarchy.plymouth`, `omarchy.script`, `logo.png`, `progress_bar.png`, `progress_box.png`, `entry.png`, `lock.png`, `bullet.png`, `logos/oma.png`; switched by `bin/omarchy-plymouth-set`, `-switcher`, `-set-by-theme`, `-preview`, `-reset`; menu Appearance > Unlock |
| Before every install/update/setup job | `omarchy-show-logo` clears the terminal and prints `logo.txt` in green (ANSI 32), then the job runs, then `omarchy-show-done` | `bin/omarchy-show-logo`, `logo.txt` (11 rows of block characters), `bin/omarchy-launch-floating-terminal-with-presentation` (wraps every job: logo, command, done) |
| Screensaver (idle or System > Screensaver) | Fullscreen terminal on every monitor running `ttfx` (terminaltexteffects) on `~/.config/omarchy/branding/screensaver.txt` (a copy of `logo.txt`), `--random-effect`, 120 fps, any key exits | `bin/omarchy-screensaver` (line 40), `bin/omarchy-launch-screensaver`, `bin/omarchy-toggle-screensaver`; branding: `bin/omarchy-branding-screensaver image|text|reset`, `bin/omarchy-transcode-ascii` (PNG/SVG to ASCII) |
| About (menu root "About") | Floating terminal, `fastfetch` with the logo from `~/.config/omarchy/branding/about.txt` plus modules (hardware, software, age/uptime/update); window auto-fitted to the logo; a timed "sheen" glint animation sweeps across the logo | `bin/omarchy-launch-about`, `bin/omarchy-branding-about`, `bin/omarchy-branding-about-animation` |
| Branding menu | User can replace the screensaver and About logo from an image or text | `appearance.branding.screensaver`, `appearance.branding.about` in `default/omarchy/omarchy-menu.jsonc` |

Other: `logo.svg`, `icon.png`, `icon.txt` at repo root; `bin/omarchy-ascii`.

## 2 Font sizes

The shell has ONE font family and a size scale derived from a base (`shell/Commons/Style.qml`, lines 281-340). Default base 12 px; every token is `round(base * multiplier)`; a theme can pin a token via `[font]` in `shell.toml`; `fontBaseSize` also scales spacing and bar dimensions.

| Token | Multiplier | px at base 12 |
|---|---|---|
| caption | 0.833 | 10 |
| bodySmall | 0.917 | 11 |
| body | 1.0 | 12 |
| subtitle | 1.083 | 13 |
| title | 1.167 | 14 |
| heading | 1.333 | 16 |
| display | 2.0 | 24 |
| displayLarge | 2.333 | 28 |
| icon / iconLarge | title / 1.5 | 14 / 18 |

Use sites (file:line):
- Menu (`shell/plugins/menu/Menu.qml`): header text `Style.font.heading` (1413, 1426); row icon `iconLarge` (1522); row title `heading` (1564); row description / search subtitle `bodySmall` (1578); other text `body` (1598); large glyph `displayLarge` + `title` (1682, 1693; likely the empty state, not verified). So the menu mixes 16 / 11 / 12 / 18 / 28 px.
- Notifications (`shell/plugins/notifications/Toast.qml`): title `title` 14 (400), body `body` 12 (321, 427), meta `bodySmall` 11 (454), big glyph `displayLarge` 28 (334).
- Bar (`shell/plugins/bar/Bar.qml:1887`): `body` 12; bar height scales with base.
- Terminals: `config/alacritty/alacritty.toml:13` `size = 9`; `config/ghostty/config:7` `font-size = 9`; kitty default commented (`config/kitty/kitty.conf:9`, `# font_size 12`). Terminal font family changed by `bin/omarchy-font-set` (writes alacritty, kitty, ghostty, fontconfig alias); Appearance > Font is a provider list (`bin/omarchy-font-list`, `-current`, `-set`).
- Walker is gone from this version (no `default/walker`, no `config/walker`); its role is the Quickshell menu.

## 3 Menu tree and actions

Root entries (`default/omarchy/omarchy-menu.jsonc`): Apps, Learn, Appearance, Trigger, Setup, Install, Remove, Update, About, System. Each child is a drawn row; Search is by label / last id segment / description. TUI column: "float" = job runs in `omarchy-launch-floating-terminal-with-presentation` (logo, then output scrolls live in a terminal, then Done screen; Ctrl-C exit 130 skips Done). "term" = `xdg-terminal-exec --app-id=org.omarchy.terminal`. "shell" = Quickshell plugin panel, no terminal.

| Entry | Action | TUI | Output | Input |
|---|---|---|---|---|
| Apps | `omarchy-shell omarchy.dock apps` (provider: installed apps) | shell | none | type to search |
| Learn: Keybindings | `omarchy-menu-keybindings` | shell list | list | search |
| Learn: Omarchy, Hyprland, Arch, Neovim, Bash | open web app window on manual / wiki / cheat sheet URLs | no | n/a | none |
| Learn: Tmux, Herdr keybindings | `omarchy-menu-tmux-keybindings`, `-herdr-keybindings` | shell list | list | search |
| Learn: Community | open Discord | no | n/a | none |
| Appearance: Theme | `omarchy.image-picker` source themes (preview thumbnails) | shell picker | live theme change | pick |
| Appearance: Background | `omarchy-theme-bg-switcher` then `omarchy-theme-bg-set` | picker | wallpaper change | pick |
| Appearance: Font | provider list of fonts, `omarchy-font-set` | list | font change | pick |
| Appearance: Menu Bar (hide/show, position, background opaque/glassy/clear, indicators: battery %, clock formats x6) | `omarchy-bar ...`, `omarchy-toggle-bar` | no | instant | pick |
| Appearance: Windows (gaps, corners, opacity, 1-window ratio) | `omarchy-hyprland-window-*-toggle` | no | instant | pick |
| Appearance: Branding (Screensaver, About) | `omarchy-branding-screensaver/about image/text/reset` | file picker or editor | preview launches | file select |
| Appearance: Dock (autohide, position), Workspace Layout (Tiling, Scrolling, Floating), Animations on/off | `omarchy-dock`, `omarchy-hyprland-workspace-layout-default`, `omarchy-toggle-animations` | no | instant | pick |
| Appearance: Unlock | `omarchy-plymouth-switcher`, then float job to rebuild initramfs | float | sudo prompt + build log | pick, sudo password |
| Trigger: Toggle (Stay Awake, Notifications, Nightlight, Screensaver, Crash Capture) | `omarchy-toggle-*` | no | notification | none |
| Trigger: Emoji | `omarchy-menu-emoji` | shell picker | inserts text | search |
| Trigger: Clipboard History | `omarchy.clipboard` panel | shell | list | search |
| Trigger: Reminder (set / show / clear) | `omarchy-reminder -i` etc. | prompt | none | text prompt |
| Trigger: Capture (Screenshot, Screenrecord, Text OCR, QR, Color picker) | `omarchy-capture-*`, `hyprpicker -a` | region select | file + clipboard + notification | drag region |
| Trigger: Transcode | `omarchy-transcode` | gum-style | ffmpeg output | file select |
| Trigger: Share (Clipboard, File, Folder, Receive) | `omarchy-menu-share`, `localsend` | file picker | notification | file select |
| Trigger: Hardware (laptop display, mirroring, hybrid GPU, touchpad, haptics, touchscreen) | toggles; hybrid GPU runs in float job | float for GPU | instant | none |
| Trigger: Speed Test (network, disk) | `omarchy.speedtest`, `omarchy.disk-speedtest` panels | shell | live graph / result | none |
| Setup: Monitors, Keybindings, Input, Config (Hyprland, Hyprsunset, XCompose) | open the config file in the editor | editor | n/a | edit file |
| Setup: Typing (keyboard layouts, input methods), Language | `omarchy-setup-keyboard`, `-input`, language provider | list | applies | pick |
| Setup: Network (DNS, QR) | DNS sub-menu; `omarchy.wifiqr` panel | shell | QR for wifi | pick |
| Setup: Defaults (Agent, Browser, Dictation, Terminal, Editor) | default-app setters | no | instant | pick |
| Setup: Plugins (enable, disable, add, clone, remove) | `omarchy-menu-plugin`, `omarchy-plugin-add` (float) | list / float | log | pick, URL |
| Setup: Security (Fingerprint, Fido2, SSHD, SSH Agent, Sudoless Docker, Passwordless Sudo) | `omarchy-setup-security-*` | float | step output | sudo, gum prompts, touch device |
| Setup: Config Git | `omarchy-setup-git` | float | output | gum input (name, email) |
| Setup: Direct Boot, Reset Computer | `omarchy-setup-direct-boot`, `omarchy-system-factory-reset` | float | output | confirm |
| Install: Package | `omarchy-pkg-install` in term | fzf (multi-select, preview `pacman -Sii`, alt-p toggles preview) | pacman output | type to filter, Tab multi-select |
| Install: AUR | `omarchy-pkg-aur-install` | fzf in term | yay output | same |
| Install: Web App | `omarchy-webapp-install` | float, gum | writes desktop entry | gum input name, URL, icon |
| Install: TUI | `omarchy-tui-install` | float, gum | writes desktop entry | gum input name, command, `gum choose` float/tile, icon |
| Install: Windows | `omarchy-windows-vm install` | float | VM setup log | prompts |
| Install: Preinstalls | `omarchy-install-preinstalls` | float | log | none |
| Install: Style (Theme from git URL, Background, Font) | `omarchy-theme-install`, `omarchy-theme-bg-install` | float / picker | log | URL, file |
| Install: Service (1Password, Dropbox, Spotify, Signal, Slack, Tailscale, NordVPN, ONCE, Bitwarden, Microsoft, Chromium Account) | `omarchy-install-service-*` or `omarchy-install-and-launch` | float | pacman / yay output | sudo; some login flows |
| Install: Editor (VSCode, Cursor, Zed, Sublime, Helix, Vim, Emacs), Browser (Chrome, Edge, Brave, Brave Origin, Firefox, Zen), Terminal (Alacritty, Foot, Ghostty, Kitty), AI (ChatGPT, Claude, Grok Bot, Hermes, LM Studio, Ollama, OpenClaw, Perplexity, T3 Code), Gaming (Steam, RetroArch, Minecraft, GeForce NOW, Xbox Cloud, Xbox Controllers, Battle.net, Lutris, Heroic, RetroArch Game Launcher) | `omarchy-install-*` | float | package-manager output | sudo |
| Install: Development (Rails, Docker DB, JavaScript, Go, PHP, Python, Elixir, Zig, Rust, Java, .NET, OCaml, Clojure, Scala) | `omarchy-install-dev-env <lang>`, `omarchy-install-docker-dbs` | float | mise / pacman output | JavaScript, PHP, Elixir have sub-menus; Docker DB picks a database |
| Remove: Package, TUI, Web App, Theme, Preinstalls, Windows, AI, Dictation, Services, Development, Gaming, Browser, Security | `omarchy-pkg-remove` (fzf in term), `omarchy-tui-remove`, `omarchy-webapp-remove`, `omarchy-theme-remove`, `omarchy-remove-*` | fzf / gum choose / float | removal output | multi-select |
| Update: Omarchy | `omarchy-update` (float): free-space check, `omarchy-snapshot create`, package update, migrations, boot verification, reboot offer | float | full log, coloured warnings | confirm reboot |
| Update: Channel, Kernel (provider), Config, Extra Themes (`omarchy-theme-update`), Process (restart services) | `omarchy-update-*`, `omarchy-restart-*` | float / no | log | pick |
| About | `omarchy-launch-about` | term (fastfetch + glint animation) | system info | any key |
| System: Screensaver, Lock, Suspend, Hibernate, Logout, Reboot, Shutdown | `omarchy-launch-screensaver force`, `omarchy-system-lock`, `systemctl`, `omarchy-system` | no | none | none (Suspend and Hibernate hidden by `when` guards) |

Conditional rows: `when` / `checked` / `disabled` shell guards (docs/menu.md) show On/Off pairs only for the active state.
Pattern worth copying: every long job is the same wrapper (logo, command, Done), so output, errors and "press any key" are uniform.

## 4 Gap list against Amarchy

Amarchy today (checked: `dist/keymap` [menu] rows, `amarchy/*.c`, roadmap): groups Apps, Settings (Amarchy prefs, menu font, edit menu), Style (theme, font, background), Learn, System (Reboot, screen switch), Install (upkg browse, upgrade, update), Trigger (clipboard history), Window (25 rows), DOpus (10). Drawn menu has one font (the screen font via `amarchy/menu_font.c`, plan `2026-10-23-amarchy-menu-aspect.md`).
Planned = roadmap `thoughts/shared/plans/2026-10-26-amarchy-roadmap.md` (R1.x P1 theme, R2.x P2 bar, R3.x P3 tiling, R4.x P4 capture/notify/power) or the AMARCHY-UHC section of `thoughts/spot/todos.md` (line 2788).

| Omarchy feature | Status | Amiga equivalent for the missing ones |
|---|---|---|
| Menu with groups, search, Apps launcher | already in Amarchy | |
| Theme / Font / Background rows | already in Amarchy (style.*); desktop-wide theme planned R1.2-R1.13 | |
| Clipboard history, Learn > Keybindings | already in Amarchy | |
| Window snapping, tiling, layouts | already (window.* rows) / planned R3.1-R3.12, auto re-tile R3.7b | |
| Status bar (clock, RAM, CPU, network, workspaces) | planned R2.1-R2.16 | |
| Screenshot / capture, clipboard image | planned R4.1-R4.7 (no OCR, QR, colour picker) | OCR and QR: none on stock OS 3.2; colour picker: ReadPixel under pointer, small, not planned |
| Notifications | planned R4.8-R4.11 | |
| Power: shutdown, reboot, flush | planned R4.12-R4.14 (Safe to switch off); Reboot row exists | Suspend, Hibernate, Logout: none on Amiga; Lock: out of scope per roadmap line 260 |
| Package install / remove / update (Install, Remove, Update groups) | planned: AMARCHY-UHC (merged upkg + UHC Packages group: Search, Browse, Installed, Upgrades, Refresh) | Remove group = Installed rows with remove action; Update Omarchy = Upgrades row |
| Output of a job shown inside the UI, not a terminal | planned in AMARCHY-UHC (owner: "show the output inside amarchy not in a term"); the Amarchy-side gap is stated there (act_run output unread, no live log view, loop waits only on IDCMP and CTRL_C) | The same log pane serves every job below |
| Prompt rows for questions (gum input / choose) | planned for upkg add-ons only (AMARCHY-UHC, "drawn as input rows") | General input row: string, choose-one, confirm, reused by Setup and Install forms |
| Startup logo: Plymouth boot logo + progress | missing; Amiga has no pre-Workbench splash from Amarchy | Logo shown when Amarchy starts (a small themed window or full screen for about 2 s from Startup-Sequence or the commodity start), optional boot logo ILBM via a Startup-Sequence line before LoadWB |
| Logo before every job (omarchy-show-logo / Done screen) | missing | Header band (logo rows) at the top of the job log pane, plus a Done/Failed footer with the exit code |
| Screensaver with logo effects (ttfx) | missing; `thoughts/shared/research/2026-10-22_amarchy-v2-native-workbench.md` row 28 notes a blanker commodity idea, no roadmap item | Blanker commodity: input.device event counter then a screen with the ASCII logo and a random text effect; needs a timer source (R2.3 provides it) |
| About screen with logo and system info (fastfetch) | missing | About row: logo, Kickstart/WB version, CPU, chip/fast RAM, UP-Term and Amarchy versions, uptime; data from AvailMem, SysBase, exec version |
| Branding menu (replace logo) | missing | Logo file in `UP-Term:Amarchy/` that the user can replace |
| Several font sizes in the menu (title 16, row 16, description 11, small 10) | missing; one font and one size per menu | Title bar and group header in a larger font, rows in the screen font, description line in a smaller one; needs menu_font.c to open 2 or 3 fonts per screen (JetBrainsMono 16 / TopazPro 16 / topaz 8 pairs already exist in `render/fontpair.c`); theme-pinned sizes would follow GUI-themes plan |
| Font size scale as one base + multipliers, theme can pin tokens | missing | `font-base` plus ratios in the theme file (R1.5 parses `font`; extend to size tokens) |
| Description subtitle on a row | missing | Second text line (or status line at the menu bottom) per row from keymap `|` field |
| Toggle rows that show the current state (On/Off pairs with `when`) | missing as a general feature | Row `checked:` condition (ENV: variable or file exists) shown with a tick glyph |
| Setup: security, git, default apps, keyboard, language | missing | Locale/Input prefs launch (Prefs programs), Roadshow/AmiSSL setup rows; most are Linux-only |
| Setup: edit config files in editor | already (settings.amarchy.edit = nvim ENV:up-term/keymap) | |
| Install: dev environments, browsers, editors, AI, gaming catalogues | covered by the package group (upkg add-ons: up-python, up-vim, up-git ...) | Group the rows by upkg Section: field (UPKG-AMARCHY-API) instead of hand-written lists |
| Install Web App / TUI launcher wizard | missing | "Add launcher" form: name, command, window style, icon -> writes an Amarchy app entry / .info |
| Speed test, disk test | missing | Disk speed row (read/write a RAM/DH file, time via timer.device); network via Roadshow |
| Reminders | missing | Timer-driven reminder list using R2.3 ticks plus notify R4.9 |
| Emoji picker, Transcode, Share (LocalSend), Hardware toggles, Hybrid GPU | missing, Linux-specific | Skip; Share could be an FTP/ftpd toggle (not planned) |
| Update Omarchy with snapshot, migrations, boot check | missing | Update = upkg upgrade plus a pre-update backup of changed files (upkg may already keep backups: not checked) |
| Keybinding viewer | already (learn.keys viewer) | |

## 5 Open questions for the owner

1. Boot logo: only when Amarchy starts (safe, one window), or also a pre-Workbench splash edited into Startup-Sequence (riskier, touches the owner's boot)?
2. Screensaver: wanted at all? If yes it becomes a new roadmap phase after P2 (needs the timer tick); text-effect style (ttfx-like) or just the logo with a colour cycle?
3. Several font sizes: three tiers (header, row, description) from one base like Omarchy's `Style.font` scale, or only a bigger header? Should a theme be able to pin sizes?
4. One shared job log pane (header logo, live output, Done/Failed footer) as a roadmap item before AMARCHY-UHC, so packages, Setup forms and power reuse it? Recommended: yes, it is the common piece.
5. General input rows (string, choose, confirm) in the drawn menu: build once now for Setup/Install forms, or only for upkg questions?
6. Which Omarchy Setup/Trigger groups do you want at all (Setup defaults, Reminders, Speed tests, Add-launcher wizard)? Unverified: how Amarchy would persist toggle state ("checked" rows).
7. Not verified here: the exact meaning of the Menu.qml large-glyph font uses (lines 1682, 1693) and the contents of the Omarchy provider sub-menus (JavaScript, PHP, Elixir, DNS, Docker DB) - the parsed list shows the parents only.
