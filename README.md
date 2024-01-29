# reMarkable Keyboard

The rmkb project produces a small binary for the reMarkable 2, which when run over SSH, simulates keystrokes on the device.

_N.B. This project is somewhere between a proof-of-concept and a joke.  It may be useful to investigate how the reMarkable works with a keyboard, but it is not, and will never be, a daily driver for using a keyboard with your reMarkable._

## Installation

Installation and use of rmkb require an SSH connection to your reMarkable device.  This [reMarkable guide page](https://remarkable.guide/guide/access/ssh.html) can get you started.

To download rmkb, SSH into your device and run
```
wget https://github.com/rschroll/rmkb/releases/download/v0.1/rmkb
```
And that's it!

<details>
<summary>A note about security</summary>

Downloading and running binaries from random people on the internet is not a great idea, security-wise.  For openness, the binary is built on GitHub Actions.  You can checkout the [workflow](https://github.com/rschroll/rmkb/blob/main/.github/workflows/build.yml) and examine the [build logs](https://github.com/rschroll/rmkb/actions).  A `sha256sum` of the binary is computed in the build process.  Use this to verify that the binary you downloaded was the same as was built in the GitHub Action.  On either your reMarkable or your computer, run
```
sha256sum rkmb
```
The output should be the same as in the GitHub Actions log _for the version that you have downloaded._
</details>

## Usage

SSH into your reMarkable device and run
```
./rmkb
```
All the keystrokes you type will be forwarded on to appear as if they were typed into a keyboard attached to the reMarkable itself.  (However, see the [caveats section](#caveats) below.)

### Command mode

In order to issue commands to rmkb itself, you must enter command mode with `Ctrl-q`.  This will cause a `?` prompt to appear.  Type `q` to quit rmkb, or `h` to list other commands.  After a valid command, rmkb exits command mode and returns to echoing keystrokes.

### Caveats

rmkb works by trying to reconstruct the keys that you've pressed based on the characters that get sent to it over SSH.  This is a surprisingly difficult problem, and there is not a one-to-one mapping between keystrokes and characters.  While letters, numbers, and symbols should mostly work, modifier keys are a bit more of a crap-shoot.

1. rmkb has been developed and tested only on US QWERTY keyboards.  I _think_ the keyboard on your computer shouldn't matter, as long as you're typing ASCII characters.  If you enter any other characters, rmkb will likely fail.  I _don't think_ the language you've chosen for your on-screen keyboard matters, but I haven't exhaustively checked this.

2. Pressing a modifier key (Shift, Ctrl, Alt) does not cause a character to be sent to rmkb, so it is unaware when these are pressed in isolation.

3. Some keystrokes will be captured by your operating system, window manager, terminal emulator, or SSH client.  rmkb will never see them, and therefore cannot forward them to the reMarkable.

4. Sometimes, the same character will be produced with and without a modifier.  (On my system, `Ctrl-1` produces the `1` character.)  Thus, rmkb won't always be able to detect which modifiers have been pressed.

5. There are [many different standards](https://hsm.stackexchange.com/questions/6363/when-did-grace-murray-hopper-say-the-wonderful-thing-about-standards-is-that-th) for encoding characters in terminal applications.  I've been testing with terminals that identify themselves as xterm.  If rmkb starts complaining about unrecognized escape characters, please [open an issue](https://github.com/rschroll/rmkb/issues/new).

To help you enter more complex key chords, the command mode lets you toggle a modifier key on for the next keystroke, or lock it on for all following keystrokes.  Open command mode, with `Ctrl-q`, and press `a` to make the next keystroke have the Alt modifier included.  The `A` in the prompt should become bold signifying this.  Alternatively, press `A` to lock the Alt modifier on.  The `A` in the prompt should now be underlined. The `c` and `s` keys behave similarly for Ctrl and Shift.

For some reason, the on-screen keyboard pops up when you are entering text with rmkb.

## Development

To build rmkb for the reMarkable, you need the [reMarkable toolchain](https://remarkable.guide/devel/toolchains.html).  I've been using the [official 4.0.117 release for the reMarkable 2](https://storage.googleapis.com/remarkable-codex-toolchain/remarkable-platform-image-4.0.117-rm2-public-x86_64-toolchain.sh).  The toolchain is intended for Linux machines, but I know people have gotten it to run in Docker.

This file is a self-extracting shell script, which unpacks, by default to `/opt/codex`.  You may need to run the script as root.

You will need to `source` the environment-setup file that got installed with the toolchain to get everything set up for cross-compilation.  Then, simply running `make` should build rmkb.

To help with development, the `make deploy` target attempts to copy the binary over to the reMarkable device.  It assumes that it can be reached at `remarkable.local`.  If that's not    the case, set the `rmdevice` environmental variable with the right host name or IP address.

## Other approaches

As this system of detecting keystrokes became more involved, I considered a couple of other mechanisms.  I ultimately decided against them, but I record them here, in case someone else is considering them.

### Separate app

Instead of relying on the program in the SSH client detecting the keystrokes, a GUI program could run on your computer.   This would find it much easier to record modifier keys and chords.  This information could be sent over a socket to the reMarkable.

However, this would require two separate programs working in sync.  Different OSes would likely require different toolkits to be able to capture the keystrokes reliably.  This seemed like too big of a lift for this silly project, but it's probably the way to go if you want to solve this for real.

### Capturing `/dev/input/event0`

On Linux machines, all keystrokes can be read from `/dev/input/event0` (or `1`, or `2`,...).  (In fact, writing to such a file is basically how rmkb works.)  We could read from your computer and write them on the reMarkable.  This could probably be accomplished by piping the event file into SSH, and the data would already be in the right format to serve as keystrokes on the reMarkable.

This approach would be Linux-only.  It would also work _too_ well&mdash;all of your keystrokes on your computer would be transmitted to the device, not just those typed when a particular window has focus.  But these keystrokes would also be driving some action on your computer.  That could turn into a confusing mess pretty easily.

### Using X forwarding

I thought this was rather clever: Rather than having a GUI app run on your computer to capture and forward keystrokes, have the GUI app run on the remarkable, but connect to the X server on your computer for display and for input.  X forwarding over SSH is well-established, and all of the finer operating systems come with an X server.

The reMarkable has neither an X server nor the X libraries.  But I stumbled across [libxcb](https://xcb.freedesktop.org/) and found that I could compile a small program with static linking, at least for x86.  I might have to compile libxcb myself for ARM, but it didn't seem too bad.

However, I had misunderstood the nature of X forwarding.  I had thought the remote machine (client in X-speak) needed only a program and SSH took care of the rest.  But actually, the client machine needs a few X utilities to set up things like authorization.  And, as previous mentioned, the reMarkable has none of these.

### Running a web server on the reMarkable

With HTML and JavaScript, you can actually capture quite a lot of keypress information.  I've seen other projects run a small webserver on the reMarkable, and I can imagine this serving a little page to capture keystrokes, which then simulated on the device.

This does require a few moving pieces, but I think it may be the most feasible of all of these alternative approaches.  I haven't investigated it in too much depth, though.

## Copyright

Copyright 2024 Robert Schroll

rmkb is released under the MIT license. See LICENSE for details.
