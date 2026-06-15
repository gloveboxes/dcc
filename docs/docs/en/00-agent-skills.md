# Agent skills

The dcc repo ships a project-scoped **agent skill** in
`.github/skills/c89-cpm-z80`. A skill is a folder containing a `SKILL.md` (plus
optional `references/`) that packages domain knowledge — here, how to write,
build, test, and debug C89 code for dcc/CP/M/Z80, along with the runtime library
inventory and hard-won pitfalls.

An agent that supports skills reads `SKILL.md` on demand when your task matches
the skill's description, so it gets dcc-specific guidance without you pasting it
into every prompt. This is the fastest way to get an AI coding assistant to
produce correct dcc code, so it's worth setting up before you start a project.

## Invoking the skill in VS Code

With GitHub Copilot in VS Code (agent mode), the skill is picked up
automatically when you open the dcc repo — the agent loads it when your request
falls within the skill's scope (anything mentioning dcc, CP/M, Z80, ntvcm,
DCCRTL, and so on). You don't have to do anything special; you can also nudge it
explicitly, for example:

> use the c89-cpm-z80 skill to build and test foo.c

## Using the skill from the GitHub Copilot CLI

The GitHub Copilot CLI discovers skills the same way: project skills from the
repo you launch it in, plus any personal skills in your home-directory roots
(see below). From the repo root, start a session:

```sh
copilot
```

Then, at the prompt, ask something within the skill's scope (for example,
"build and run sieve.c for CP/M with dcc"). The CLI reads the matching
`SKILL.md` on demand, exactly like VS Code.

## Making the skill available system-wide

The copy in the dcc repo only applies while you're working inside that repo. The
main reason to deploy it system-wide is to build CP/M apps in a **separate,
independent project**: with the skill in a personal root, the agent brings
dcc-specific knowledge into that other workspace, and as long as the
`dcc` / `dccpeep` / `dccrtlstrip` binaries and `DCCRTL.MAC` are on your `PATH`
(see [Setting up the toolchain](00-setup-toolchain.md)), you can compile and run
from there without copying the toolchain into every project.

To use it from **every** workspace on your machine, copy the skill folder into a
personal skills root in your home directory (`~/.agents/skills/`,
`~/.copilot/skills/`, or `~/.claude/skills/` — pick one and stay consistent):

=== "macOS / Linux"

    ```sh
    mkdir -p ~/.agents/skills
    cp -R .github/skills/c89-cpm-z80 ~/.agents/skills/
    ```

=== "Windows (PowerShell)"

    ```powershell
    New-Item -ItemType Directory -Force "$env:USERPROFILE\.agents\skills" | Out-Null
    Copy-Item -Recurse ".github\skills\c89-cpm-z80" "$env:USERPROFILE\.agents\skills\"
    ```

The repo copy and the personal copy are independent files, so re-copy after
editing either one to keep them in sync.
