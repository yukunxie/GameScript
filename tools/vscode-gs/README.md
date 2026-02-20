# GS Script Tools

VS Code extension for `GameScript` (`.gs`) files.

## Features

- Syntax highlighting for GameScript keywords, numbers, strings, operators, and comments.
- Module syntax keyword highlighting for `import`, `from`, and `as`.
- Comment support via language configuration:
  - Line comments: `#`
  - Block comments: `/* ... */`
- Command: `GS: Add Line Comment` (`gs.addLineComment`) to add `# ` to selected lines.

### Module syntax examples

```gs
import module_math;
import module_math as mm;
from module_math import add as plus;
from module_math import * as ns;
```

## Local usage

1. Open `tools/vscode-gs` in VS Code.
2. Press `F5` to launch an Extension Development Host.
3. Open any `.gs` file and verify highlighting.
4. Run command palette: `GS: Add Line Comment`.

## Package (optional)

If you want a `.vsix` package:

```bash
npm install -g @vscode/vsce
cd tools/vscode-gs
vsce package
```

Then install with:

```bash
code --install-extension gs-script-tools-0.0.1.vsix
```
