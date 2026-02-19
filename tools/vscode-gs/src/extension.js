const vscode = require('vscode');

function activate(context) {
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

  context.subscriptions.push(disposable);
}

function deactivate() {}

module.exports = {
  activate,
  deactivate,
};
