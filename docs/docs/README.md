# dcc MkDocs site

This folder contains the MkDocs configuration and English documentation source
for the dcc documentation site.

## Layout

- `mkdocs.yml` — MkDocs configuration.
- `requirements.txt` — Python packages needed to build the site.
- `en/` — English documentation pages.
- `hooks/copy_assets.py` — build hook that copies shared repo assets into the
  docs source tree before MkDocs validates links.

The benchmark chart is maintained once at the repo root as `images/table.jpg`.
During a docs build, `hooks/copy_assets.py` copies it to
`en/images/table.jpg`, which is ignored by git.

## Install dependencies

From this directory:

```sh
python3 -m pip install -r requirements.txt
```

## Build the site

From this directory:

```sh
mkdocs build --strict
```

The generated site is written to `../site` (`docs/site` from the repo root),
which is ignored by git.

## Serve locally

From this directory:

```sh
mkdocs serve
```

MkDocs prints the local URL, usually `http://127.0.0.1:8000/`.

## Deploy

The GitHub Actions workflow in `.github/workflows/docs.yml` builds this MkDocs
site from the `main` branch and publishes it with `mkdocs gh-deploy`.

## Updating runtime size tables

The runtime size tables on the *Runtime function sizes* appendix page are
**generated at build time** by the `hooks/runtime_sizes.py` MkDocs hook, which
analyses `DCCRTL.MAC` directly. There is nothing to regenerate by hand: edit the
runtime and rebuild the docs, and the tables update themselves. To preview:

```sh
mkdocs build --strict
```
