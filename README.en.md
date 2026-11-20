# NDS BASIC

*English · [Deutsch](README.md)*

A small, retro-inspired BASIC interpreter for the Nintendo DS.

* **Top screen:** program listing with line numbers, status line and input line
  (or the output screen while a program is running)
* **Bottom screen:** retro touch keyboard with an LCD status display
* **Title screen:** on start-up a low-poly home computer (keyboard, CRT monitor, floppy drive) swings
  around on the top screen, rendered with the DS 3D hardware. The bottom screen lists all saved programs
  (12 per page, ← → to flip pages): tapping one loads it straight into the editor. "NEW PROGRAM"
  or "BACK TO PROGRAM", or any button, starts BASIC without loading anything.
  `EXIT` (in direct mode or in a program) returns to the title screen; program and data are kept

## Building

Only Docker is required (image `devkitpro/devkitarm`):

```bash
./build.sh          # creates nds-basic.nds
./build.sh clean
```

Interpreter tests on the PC (no DS needed):

```bash
make -C host test
```

## Controls

| Action | Keyboard | Buttons |
|---|---|---|
| Run / stop the program | `RUN` / `STOP` | START |
| Help page with all commands (↑↓ scroll, ←→ page) | – | SELECT |
| Switch between editor and output | `SCRN` | – |
| Select / edit a line in the listing | – | ↑↓ / A |
| Move the cursor, delete | `←` `→` `DEL` | ←→, B |
| Clear the input line | `CLR` | – |
| Shift (tap twice = lock) | `SHFT` | hold L / R |
| Sprite editor | – | X |
| Sound editor | – | Y |

Lines with a number are stored (`10 PRINT "HI"`), a number on its own deletes that line.
Without a number the command runs immediately (`PRINT 2+3`, `RUN`, `NEW`, `LIST 100`).

**Language:** the user interface (title screen, hints, help page) follows the system language of the DS.
If the DS is set to German it appears in German, with any other language in English.
The classic BASIC messages (`READY.`, `?SYNTAX ERROR` …) and the commands are always English.

## Language reference

`PRINT`/`?` `LET` `INPUT` `IF…THEN…ELSE` `GOTO` `GOSUB`/`RETURN` `FOR…TO…STEP`/`NEXT`
`ON…GOTO/GOSUB` `DIM` `READ`/`DATA`/`RESTORE` `REM`/`'` `END` `STOP` `CLS` `PAUSE n` (frames)
`LOCATE row,column` `COLOR 0-15` `RANDOMIZE` – commands `RUN` `LIST` `NEW` `CLR` `EXIT` `SAVE` `LOAD` `DIR` `DELETE` `SPRITE` `SPROFF` `STAMP` `SPRSET` `SPRED` `SOUND` `SNDED`

Functions: `ABS INT SQR SIN COS TAN ATN EXP LOG RND SGN LEN VAL ASC CHR$ STR$
LEFT$ RIGHT$ MID$ INSTR INKEY$ JOY PI`, operators `+ - * / ^ MOD = <> < > <= >= AND OR NOT`.

Keywords must be separated by spaces (`FOR I=1 TO 10`).

`JOY` returns the DS buttons currently held as a bit mask, ideal for games:
1 = up, 2 = down, 4 = left, 8 = right, 16 = A, 32 = B, 64 = X, 128 = Y, 256 = L, 512 = R
(e.g. `IF JOY AND 1 THEN Y=Y-2`). START remains the break button.

## Graphics

Behind the text on the top screen there is a graphics layer of 256×192 pixels
(x 0–255, y 0–191). Colours are 0–15 as with `COLOR`, `-1` erases a pixel.
Colour `c` and fill `f` are optional (default colour via `GCOLOR`).

```
GCLS                          clear graphics
GCOLOR c                      set the default drawing colour
PLOT x,y[,c]                  set a pixel
LINE x1,y1,x2,y2[,c]          line
RECT x1,y1,x2,y2[,c[,f]]      rectangle, f=1 filled
CIRCLE x,y,r[,c[,f]]          circle, f=1 filled
PAINT x,y[,c]                 flood-fill a connected area
POINT(x,y)                    colour of a pixel (-1 = empty)
```

Example:

```
10 GCLS: CLS
20 FOR I=0 TO 15: CIRCLE 128,96,90-I*5,I,1: NEXT
30 LINE 0,191,255,0,2
```

The graphics are visible on the output screen and hidden in the editor.

## Sprites

There are 256 sprite slots (0–255), each 8×8 pixels in the 16 colours or transparent.
They are shown as DS **hardware sprites**: 64 sprite objects (0–63) that sit above text and
graphics and can be moved without touching the background.

```
SPRITE o,n,x,y[,s]   show or move sprite object o with image n at x,y, s = size 1-4
SPROFF [o]           hide object o, all objects without an argument
SPRHIT(a,b)          -1 if the visible objects a and b overlap, otherwise 0
STAMP n,x,y[,s]      stamp image n into the graphics layer (s = 1-8), e.g. for backgrounds
SPRSET n,x,y,c       set a single pixel (x,y 0-7) of an image, c=-1 = transparent
SPRED [n]            open the sprite editor (also key X in the editor)
```

Sprites are only visible on the output screen and may be partly off screen. `RUN` and `NEW`
hide all objects. When an image changes (editor or `SPRSET`), every object using it is updated.

Example:

```
10 CLS
20 FOR X=0 TO 240 STEP 2
30 SPRITE 0,0,X,80,2: SPRITE 1,1,240-X,80,2
40 IF SPRHIT(0,1) THEN SOUND 0
50 PAUSE 1: NEXT
```

**Sprite editor** (key X or `SPRED n`): at the bottom an 8×8 grid to paint with the stylus,
a colour palette (checkerboard = transparent), a preview at 4×/2×/1× and buttons to change the slot,
copy, paste, clear and OK. The top screen shows an overview of all 256 slots; the current one
has a yellow frame. The d-pad changes the slot (←→ ±1, ↑↓ ±16), B/X/OK closes the editor.

`SAVE` writes all used sprites into the `.BAS` file (`#SPR n` + 64 characters,
`.` = transparent, `0`–`F` = colour). `LOAD` replaces the sprites if the file contains any.

## Sounds

There are 10 sound slots (0–9). Each slot is a tune of up to 64 notes with
a sound (square wave 12.5 %, 25 %, 50 % or noise) and a tempo.

```
SOUND n        play slot n in the background, the program keeps running
SOUND n,1      play slot n and wait until it has finished
SOUND -1       stop playback
SNDED [n]      open the sound editor (also key Y in the editor)
```

**Sound editor** (key Y or `SNDED n`): at the bottom a piano keyboard spanning two octaves. Tapping a
key plays the note and appends it to the tune with the selected length (1/16 to 1/1).
More buttons: rest (`REST`), delete the last note (`DEL`), clear the slot (`CLR`),
sound, tempo, octave, change slot, `PLAY`/`STOP` and `OK`. The top screen shows the notes of the
slot (e.g. `C#4  8` = eighth note) and an overview of all slots; during playback the
current note is highlighted. D-pad: ←→ slot, ↑↓ octave; B/Y/OK closes the editor.

`SAVE` stores the used slots as well (`#SND n sound tempo note:length …`, note as
MIDI number, 0 = rest, length in 16ths); `LOAD` restores them.

## Examples

The [`examples/`](examples/) folder contains ready-made programs. To try them, copy the `.BAS` files
to the `/nds-basic` folder on the SD card; they then appear in the list on the title screen
(or use `LOAD "PONG"`).

| File | Contents | Controls |
|---|---|---|
| [PONG.BAS](examples/PONG.BAS) | Pong against the computer, first to 7 points. Hardware sprites for paddles and ball, sounds for hits | d-pad ↑↓ or W/S |
| [SNAKE.BAS](examples/SNAKE.BAS) | Snake with apples, grows longer and faster with every apple. Body drawn with `STAMP` into the graphics, head and apple as sprites | d-pad or W/A/S/D |

Both files include their sprites (`#SPR`) and sounds (`#SND`).

## Saving

Programs are saved as text files on the SD card (flashcart via DLDI or DSi SD) in the folder
`/nds-basic`:

```
SAVE "GAME"       saves as /nds-basic/GAME.BAS (SAVE without a name = last name)
LOAD "GAME"       loads the program (replaces the current one)
DIR               lists all .BAS files
DELETE "GAME"     deletes the file
```

File names: up to 8 characters from A–Z, 0–9, `_`, `-`. The files are plain text files
(`10 PRINT "HI"` per line) and can be edited on the PC as well. Without an SD card the interpreter
reports `NO SD CARD` at start-up and the commands return `?NO STORAGE ERROR`.

In the melonDS emulator: enable SD emulation under *Config → Emu settings → DLDI*
(optionally with "Sync SD to folder").

## Website

The landing page lives in [`site/`](site/), in English (`index.html`, `docs.html`) and German
(`index.de.html`, `docs.de.html`) with a language switch in the header. The workflow
[`.github/workflows/pages.yml`](.github/workflows/pages.yml) builds the ROM in the devkitPro container
on every push to `main`, assembles the site with `nds-basic.nds`, this README and the German one
(as documentation pages) and the examples, and publishes it via GitHub Pages.

One-time repository setting: *Settings → Pages → Build and deployment → Source: GitHub Actions*.

To view it locally:

```bash
./build.sh && sh site/build-site.sh && python3 -m http.server -d _site 8000
```
