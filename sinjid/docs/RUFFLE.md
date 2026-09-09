# Running the original for reference

The original is a Flash file, and it can be driven headlessly. This is for
checking fidelity questions the bytecode cannot answer -- how something looks,
or what an interface does. No asset or text from it is copied into this remake.

## What it takes

* Ruffle 0.6 (`@ruffle-rs/ruffle` from npm) hosting the SWF from a local server.
* Playwright with the preinstalled Chromium and `--use-gl=swiftshader`.

## Two things that block you

**The intro never hands off to the main menu.** Clicking Play on the opening
splash runs the intro and returns to that splash; the real menu (Start Game /
Load Game / Options / View Intro) is a later root frame that is never reached.
Ruffle logs no error -- only missing device fonts and an unhandled `showmenu`
FSCommand -- so this is a control-flow dead end, not a crash.

The way through is to rebuild the SWF with one extra `DoAction` appended to the
splash frame holding `gotoAndPlay(<menu frame>)`. Appending rather than
overwriting matters: that frame defines functions the game uses later, so they
still need to run. Keep the patched copy out of the repository.

**Clicks register as hover but do not press.** Move the pointer to the target
from a short distance away, pause, then press with a dwell between down and up;
click a neutral spot first so the player has focus. Some controls are movie
clips rather than buttons -- on the class screen the portrait is clickable and
the card around it is not -- so probe candidate points and compare screenshot
hashes to find the live one.
