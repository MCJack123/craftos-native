# craftos-native
A ComputerCraft 1.8 emulator written entirely in C. Uses ncurses for rendering, but can be adapted to run with other renderers.

This program is the basis for [CraftOS-PC 2](https://github.com/MCJack123/craftos2) and [craftos-efi](https://github.com/MCJack123/craftos-efi). CraftOS-PC 2 is a (now) far derivative of craftos-native, using SDL, ncurses, or TRoR as renderers and supporting multiple computers, HTTP, and CC: Tweaked, among other things. craftos-efi shares a very similar code base to craftos-native, but uses the EFI Runtime Services as an interface.

## Building
To build, you must have:
* Any C89 or later compiler
* Lua 5.1
* ncurses
* POSIX headers including:
  * `unistd.h`
  * `dirent.h`
  * `glob.h`
  * `sys/stat[vfs].h`
  * `libgen.h`

Running `make` should be enough, but you may need to change `-llua.5.1` in the Makefile if the library is under a different name, such as `-llua` or `-llua51`.

## Running
First, place the ComputerCraft 1.8 ROM and BIOS at the root of your drive. (You can also place them in a folder and use `chroot` to run craftos-native.) Then, running craftos-native is as simple as running `./craftos`.

Do be aware that craftos-native runs unsandboxed, meaning the root of the CraftOS computer is the root of your real computer. This means there's the potential to screw up your OS inside the Lua environment. Check all of your FS code before running it, or use a `chroot` jail to run under a different root.