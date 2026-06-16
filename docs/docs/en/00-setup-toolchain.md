# Setting up the toolchain

To build and run CP/M apps with dcc you need two things: the dcc compiler tools
(`dcc`, `dccpeep`, `dccrtlstrip`, plus `DCCRTL.MAC`, `m80.com`, and `l80.com`)
and the [ntvcm](https://github.com/davidly/ntvcm) CP/M 2.2 emulator to run the
results. Both are self-contained projects that build on Windows, Linux, and
macOS.

You build these tools once. After that they live wherever you cloned them, and
you build your own CP/M apps from separate project directories — nothing has to
be copied into the dcc repo.

## Clone the repositories

```bash
# Clone the dcc compiler
git clone https://github.com/davidly/dcc.git
cd dcc

# Clone the ntvcm emulator (in a parallel directory, or wherever you prefer)
cd ..
git clone https://github.com/davidly/ntvcm.git
```

## Build dcc

The build scripts are in the dcc root directory. Each produces `dcc`,
`dccpeep`, and `dccrtlstrip`.

=== "macOS"

    ```bash
    chmod +x mmacos.sh
    ./mmacos.sh
    ```

    Requires the clang compiler from the Xcode Command Line Tools
    (`xcode-select --install`).

=== "Linux"

    ```bash
    chmod +x m.sh
    ./m.sh
    ```

    Requires gcc (`sudo apt install build-essential` on Debian/Ubuntu, or the
    equivalent for your distribution).

=== "Windows"

    ```batch
    m.bat
    ```

    Requires Visual Studio with the C++ build tools installed.

## Build ntvcm

ntvcm is a C++ project. Build it from its own directory.

=== "macOS"

    ```bash
    cd ntvcm
    chmod +x mmac.sh
    ./mmac.sh
    ```

    Produces the `ntvcm` executable. Requires the clang/g++ compiler from the
    Xcode Command Line Tools (`xcode-select --install`).

=== "Linux"

    ```bash
    cd ntvcm
    chmod +x m.sh
    ./m.sh
    ```

    Requires g++ (`sudo apt install build-essential` on Debian/Ubuntu, or the
    equivalent for your distribution).

=== "Windows"

    Check the ntvcm repository for Windows build instructions (typically via
    Visual Studio or a batch script).

## Set up your environment

The build scripts (`ma.sh`, `ma.bat`, `runall.sh`, `runall.bat`) resolve each
tool the same way: they use an environment variable if you set one, otherwise
they look for the tool on your `PATH`. The relevant tools are `dcc`, `dccpeep`,
`dccrtlstrip`, `ntvcm`, and the `m80` / `l80` assembler/linker.

The simplest setup — especially when building apps in a project *outside* the
dcc repo — is to add the directories containing the built `dcc` and `ntvcm`
binaries to your `PATH`. The dcc directory also provides `dccpeep`,
`dccrtlstrip`, `m80.com`, `l80.com`, and `DCCRTL.MAC`, so no per-tool variables
are needed.

=== "macOS / Linux"

    Add this to your shell profile (`~/.zshrc`, `~/.bash_profile`, or
    `~/.bashrc`), or run it before invoking the scripts:

    ```bash
    # Add the directories that contain the built dcc and ntvcm binaries to PATH.
    export PATH="$PATH:/path/to/dcc:/path/to/ntvcm"
    ```

    Replace `/path/to/dcc` and `/path/to/ntvcm` with the actual directories
    (e.g. `~/GitHub/dcc` and `~/GitHub/ntvcm`). With this on your `PATH`, the
    scripts find `dcc`, `dccpeep`, `dccrtlstrip`, and `ntvcm` automatically.

    To pin specific binaries instead (for example, when juggling multiple dcc
    builds), set the env vars to explicit paths and only put ntvcm on `PATH`:

    ```bash
    export PATH="$PATH:/path/to/ntvcm"
    export DCC=/path/to/dcc/dcc
    export DCCPEEP=/path/to/dcc/dccpeep
    export DCCRTLSTRIP=/path/to/dcc/dccrtlstrip
    ```

=== "Windows"

    Both dcc and ntvcm compile to native Windows executables. Add their
    directories to `PATH` (via System Properties → Environment Variables for a
    permanent setting, or temporarily in your shell):

    **PowerShell:**

    ```powershell
    $env:PATH += ";C:\path\to\dcc;C:\path\to\ntvcm"
    ```

    **CMD:**

    ```batch
    set PATH=%PATH%;C:\path\to\dcc;C:\path\to\ntvcm
    ```

    Putting the dcc directory on `PATH` means `dcc`, `dccpeep`, and
    `dccrtlstrip` are found automatically; the `DCC` / `DCCPEEP` /
    `DCCRTLSTRIP` variables are only needed to pin specific binaries.

## Verify the setup

With the tools on your `PATH`, build and run a small program from **any**
directory to confirm everything is wired up. Create a `hello.c`, then build it
with a copy of `ma.sh` (or `ma.bat` on Windows):

```sh
sh ./ma.sh hello      # compiles hello.c -> HELLO.COM in the current directory
ntvcm HELLO.COM       # runs it under the emulator
```

The dcc repo's own `tests/` programs are handy samples to copy into a scratch
project, but you do not need to work inside the dcc repo — the tools build CP/M
apps from wherever your sources live. Once that works, move on to
[Building and linking](02-build-and-link.md) for the day-to-day workflow.
