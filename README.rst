Lines-98 (C + SDL2 + Meson)
===========================

A full Lines-98 style game implemented in pure C (no C++) with SDL2-based
rendering and audio.

Features
--------

- 9x9 board with classic Lines-98 gameplay rules
- BFS pathfinding for valid ball movement
- Line clearing in 4 directions (horizontal, vertical, 2 diagonals)
- Preview of upcoming balls
- SDL audio feedback (procedural tones)
- Turn animation pipeline (move -> clear dust -> spawn growth)
- Unit tests for core logic and animation/controller modules

Dependencies
------------

SDL2 is configured via Meson wrap in ``subprojects/sdl2.wrap`` to keep builds
portable across Linux distributions.

Build
-----

Build commands::

   meson setup build --wrap-mode=forcefallback
   meson compile -C build

Run
---

Run command::

   ./build/lines98

Controls
--------

- Left mouse button: select a ball / move to an empty cell
- ``R``: restart game

Tests
-----

Run all tests::

   meson test -C build --print-errorlogs

Run logic tests only (without building the SDL app):

::

   meson setup build-tests -Dbuild_game=false
   meson test -C build-tests --print-errorlogs

Memory checks
-------------

ASan/UBSan:

::

   meson setup build-asan -Db_sanitize=address,undefined -Dbuild_game=false
   meson test -C build-asan --print-errorlogs

LeakSanitizer (for environments where ``ptrace`` is available):

::

   meson setup build-lsan -Db_sanitize=address,undefined -Dbuild_game=false -Denable_lsan=true
   meson test -C build-lsan --print-errorlogs

Note: in some sandboxed CI environments LeakSanitizer can be unavailable due
to kernel restrictions.
