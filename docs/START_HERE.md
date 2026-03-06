# GameScript Documentation Portal

This site is the entry point for GameScript docs and is designed for static hosting (for example, GitHub Pages).

## What You Get

- A single `docs/index.html` page that renders Markdown files in-browser.
- A left navigation panel generated from `docs/docs-manifest.json`.
- Hash routing (`#doc=...`) so links are shareable.
- Safe HTML rendering using `marked` plus `DOMPurify`.

## Quick Start

1. Open `docs/index.html` from a static web server.
2. Pick any document from the left sidebar.
3. Use the search box to filter docs quickly.

## Local Preview

Some browsers block `fetch` when opening via `file://`.

### One-click on Windows

Use:

```text
docs\start-docs-server.bat
```

Behavior:

- If a server is already listening on port `5500`, it only opens the homepage.
- If no server is running, it starts `python -m http.server 5500` in `docs/` and then opens the homepage.

Run a static server in the `docs` directory:

```powershell
cd docs
python -m http.server 5500
```

Then open:

```text
http://localhost:5500/index.html
```

## Next Documents

- [Docs Overview](DOCS_OVERVIEW.md)
- [Website Deployment](WEBSITE_DEPLOYMENT.md)
- [Parameter Unpack Spec](features/PARAMETER_UNPACK_SPEC.md)
