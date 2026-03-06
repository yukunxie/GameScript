# Website Deployment

This guide explains how to deploy the `docs` folder as a static documentation site.

## Files Used by the Site

- `index.html`: Main portal shell.
- `assets/site.css`: Site style.
- `assets/site.js`: Markdown rendering and navigation.
- `docs-manifest.json`: Sidebar document registry.

## Deployment Option 1: GitHub Pages (Branch Root)

Use this when repository root is published directly.

1. Commit `docs/` updates.
2. In GitHub repository settings, open Pages.
3. Set source to branch `main` (or your branch) and folder `/docs`.
4. Save and wait for publishing.
5. Open the generated Pages URL.

## Deployment Option 2: Any Static Host

Works on Azure Static Web Apps, Netlify, Vercel static mode, Nginx, and similar hosts.

1. Publish the whole `docs/` directory as static assets.
2. Ensure `index.html` is served at site root.
3. Keep all files in relative paths exactly as committed.

## Routing Notes

- The portal uses hash routing such as `#doc=BINDING_API.md`.
- No server rewrite rules are required for this route style.

## Add a New Markdown Page

1. Create a new file under `docs/`.
2. Add an entry in `docs/docs-manifest.json`.
3. Commit and redeploy.

Example manifest entry:

```json
{
  "title": "New Feature",
  "path": "features/NEW_FEATURE.md"
}
```

## Troubleshooting

### Blank page or load error

- Confirm browser console has no `404` for Markdown files.
- Check that file paths in `docs-manifest.json` are correct.

### Works online but not with double-click open

- `file://` may block `fetch` for local files.
- Use a local static server for preview.
