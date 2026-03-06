const vscode = require('vscode');

function formatGsText(text, options) {
  const lines = text.split(/\r?\n/);
  const tabSize = Number.isInteger(options?.tabSize) ? options.tabSize : 4;
  const indentUnit = options?.insertSpaces === false ? '\t' : ' '.repeat(Math.max(1, tabSize));

  let indentLevel = 0;
  let inBlockComment = false;
  const out = [];

  const stripForBraceScan = (line, state) => {
    let s = '';
    let i = 0;
    let inString = false;
    let escaped = false;
    let inBlock = state;

    while (i < line.length) {
      const ch = line[i];
      const next = i + 1 < line.length ? line[i + 1] : '';

      if (inBlock) {
        if (ch === '*' && next === '/') {
          inBlock = false;
          i += 2;
          continue;
        }
        i += 1;
        continue;
      }

      if (!inString) {
        if (ch === '#') {
          break;
        }
        if (ch === '/' && next === '/') {
          break;
        }
        if (ch === '/' && next === '*') {
          inBlock = true;
          i += 2;
          continue;
        }
        if (ch === '"') {
          inString = true;
          escaped = false;
          i += 1;
          continue;
        }
        s += ch;
        i += 1;
        continue;
      }

      if (escaped) {
        escaped = false;
        i += 1;
        continue;
      }
      if (ch === '\\') {
        escaped = true;
        i += 1;
        continue;
      }
      if (ch === '"') {
        inString = false;
        i += 1;
        continue;
      }
      i += 1;
    }

    return { text: s, inBlockComment: inBlock };
  };

  const countChar = (s, ch) => {
    let count = 0;
    for (let i = 0; i < s.length; i++) {
      if (s[i] === ch) {
        count += 1;
      }
    }
    return count;
  };

  for (const rawLine of lines) {
    const trimmed = rawLine.trim();
    if (trimmed.length === 0) {
      out.push('');
      continue;
    }

    const leadingClosersMatch = trimmed.match(/^\}+/);
    const leadingClosers = leadingClosersMatch ? leadingClosersMatch[0].length : 0;
    const currentIndent = Math.max(0, indentLevel - leadingClosers);
    out.push(indentUnit.repeat(currentIndent) + trimmed);

    const scan = stripForBraceScan(trimmed, inBlockComment);
    inBlockComment = scan.inBlockComment;
    const opens = countChar(scan.text, '{');
    const closes = countChar(scan.text, '}');
    indentLevel = Math.max(0, indentLevel + opens - closes);
  }

  return out.join('\n');
}

function activate(context) {
  const ellipsisDecoration = vscode.window.createTextEditorDecorationType({
    light: {
      color: '#B54708',
      fontWeight: '400',
    },
    dark: {
      color: '#FFD166',
      fontWeight: '400',
    },
  });

  const applyEllipsisDecorations = (editor) => {
    if (!editor || editor.document.languageId !== 'gs') {
      return;
    }

    const text = editor.document.getText();
    const ranges = [];
    const regex = /\.\.\./g;
    let match = regex.exec(text);
    while (match) {
      const start = editor.document.positionAt(match.index);
      const end = editor.document.positionAt(match.index + 3);
      ranges.push(new vscode.Range(start, end));
      match = regex.exec(text);
    }

    editor.setDecorations(ellipsisDecoration, ranges);
  };

  const refreshActiveEditorEllipsis = () => {
    applyEllipsisDecorations(vscode.window.activeTextEditor);
  };

  refreshActiveEditorEllipsis();

  context.subscriptions.push(
    ellipsisDecoration,
    vscode.window.onDidChangeActiveTextEditor((editor) => {
      applyEllipsisDecorations(editor);
    }),
    vscode.workspace.onDidChangeTextDocument((event) => {
      const activeEditor = vscode.window.activeTextEditor;
      if (!activeEditor || event.document !== activeEditor.document) {
        return;
      }
      applyEllipsisDecorations(activeEditor);
    })
  );

  const disposable = vscode.commands.registerCommand('gs.addLineComment', async () => {
    const editor = vscode.window.activeTextEditor;
    if (!editor) {
      return;
    }

    await editor.edit((editBuilder) => {
      for (const selection of editor.selections) {
        const startLine = selection.start.line;
        const endLine = selection.end.line;

        for (let line = startLine; line <= endLine; line++) {
          const lineText = editor.document.lineAt(line).text;
          const firstNonWs = lineText.search(/\S|$/);
          const insertPos = new vscode.Position(line, firstNonWs === -1 ? 0 : firstNonWs);
          editBuilder.insert(insertPos, '# ');
        }
      }
    });
  });

  const formatter = vscode.languages.registerDocumentFormattingEditProvider('gs', {
    provideDocumentFormattingEdits(document, options) {
      const original = document.getText();
      const formatted = formatGsText(original, options);
      if (formatted === original) {
        return [];
      }

      const fullRange = new vscode.Range(
        document.positionAt(0),
        document.positionAt(original.length)
      );

      return [vscode.TextEdit.replace(fullRange, formatted)];
    },
  });

  context.subscriptions.push(
    disposable,
    formatter,
    vscode.window.onDidChangeVisibleTextEditors((editors) => {
      for (const editor of editors) {
        applyEllipsisDecorations(editor);
      }
    })
  );
}

function deactivate() {}

module.exports = {
  activate,
  deactivate,
};
