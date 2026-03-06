/* global marked, DOMPurify */

const navElement = document.getElementById("doc-nav");
const docMetaElement = document.getElementById("doc-meta");
const docContentElement = document.getElementById("doc-content");
const searchInput = document.getElementById("search-input");

const fallbackManifest = {
  sections: [
    {
      title: "Start",
      docs: [
        { title: "Start Here", path: "START_HERE.md" },
        { title: "Docs Overview", path: "DOCS_OVERVIEW.md" },
        { title: "Website Deployment", path: "WEBSITE_DEPLOYMENT.md" }
      ]
    }
  ]
};

let manifest = fallbackManifest;
let activePath = "START_HERE.md";

marked.setOptions({
  gfm: true,
  breaks: false,
  mangle: false,
  headerIds: true
});

function getPathFromHash() {
  const hash = window.location.hash || "";
  const hashMatch = hash.match(/doc=([^&]+)/);
  if (!hashMatch) {
    return activePath;
  }
  return decodeURIComponent(hashMatch[1]);
}

function setHashForDoc(path) {
  const encoded = encodeURIComponent(path);
  window.location.hash = `doc=${encoded}`;
}

function flattenDocs() {
  return manifest.sections.flatMap((section) => section.docs);
}

function buildMarkdownLinkRenderer(currentPath) {
  const renderer = new marked.Renderer();
  renderer.link = ({ href = "", title = "", tokens }) => {
    const text = marked.Parser.parseInline(tokens);
    const isExternal = /^https?:\/\//i.test(href);
    const isMarkdownFile = href.toLowerCase().endsWith(".md");
    const sameSiteLink = !isExternal && isMarkdownFile;

    if (sameSiteLink) {
      const resolvedHref = new URL(href, `https://docs.local/${currentPath}`).pathname.replace(/^\//, "");
      return `<a href="#doc=${encodeURIComponent(resolvedHref)}">${text}</a>`;
    }

    const titleAttr = title ? ` title="${title.replace(/"/g, "&quot;")}"` : "";
    const targetAttrs = isExternal ? " target=\"_blank\" rel=\"noopener noreferrer\"" : "";
    return `<a href="${href}"${titleAttr}${targetAttrs}>${text}</a>`;
  };
  return renderer;
}

async function loadManifest() {
  try {
    const response = await fetch("./docs-manifest.json", { cache: "no-store" });
    if (response.ok) {
      manifest = await response.json();
    }
  } catch (_) {
    manifest = fallbackManifest;
  }
}

function renderNav(filterText = "") {
  const filter = filterText.trim().toLowerCase();
  navElement.innerHTML = "";

  manifest.sections.forEach((section) => {
    const docs = section.docs.filter((item) => {
      if (!filter) {
        return true;
      }
      return item.title.toLowerCase().includes(filter) || item.path.toLowerCase().includes(filter);
    });

    if (docs.length === 0) {
      return;
    }

    const sectionEl = document.createElement("section");
    sectionEl.className = "doc-nav-section";

    const titleEl = document.createElement("p");
    titleEl.className = "doc-nav-title";
    titleEl.textContent = section.title;
    sectionEl.appendChild(titleEl);

    docs.forEach((doc) => {
      const link = document.createElement("a");
      link.className = "doc-link";
      if (doc.path === activePath) {
        link.classList.add("active");
      }
      link.href = `#doc=${encodeURIComponent(doc.path)}`;
      link.textContent = doc.title;
      sectionEl.appendChild(link);
    });

    navElement.appendChild(sectionEl);
  });
}

async function loadDocument(path) {
  activePath = path;
  renderNav(searchInput.value);
  docMetaElement.textContent = `Loading: docs/${path}`;

  try {
    const response = await fetch(`./${path}`, { cache: "no-store" });
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }

    const markdown = await response.text();
    const renderer = buildMarkdownLinkRenderer(path);
    const html = marked.parse(markdown, { renderer });
    const safeHtml = DOMPurify.sanitize(html, {
      USE_PROFILES: { html: true }
    });

    docContentElement.innerHTML = safeHtml;
    docMetaElement.textContent = `docs/${path}`;
    updateActiveState(path);
  } catch (error) {
    docMetaElement.textContent = `Unable to load docs/${path}`;
    docContentElement.innerHTML = [
      "<h1>Document Load Failed</h1>",
      `<p>Could not fetch <code>docs/${path}</code>.</p>`,
      "<p>If you opened this page using <code>file://</code>, browser security may block <code>fetch</code>.</p>",
      "<p>Run a local static server (for example: <code>python -m http.server</code> in the <code>docs</code> folder) or deploy to GitHub Pages.</p>",
      `<pre>${String(error.message || error)}</pre>`
    ].join("");
  }
}

function updateActiveState(path) {
  const allLinks = navElement.querySelectorAll(".doc-link");
  allLinks.forEach((node) => {
    const href = node.getAttribute("href") || "";
    node.classList.toggle("active", href === `#doc=${encodeURIComponent(path)}`);
  });
}

function resolveInitialDoc() {
  const allDocs = flattenDocs();
  const hashPath = getPathFromHash();
  const found = allDocs.find((doc) => doc.path === hashPath);
  if (found) {
    return found.path;
  }

  const first = allDocs[0];
  return first ? first.path : "START_HERE.md";
}

async function init() {
  await loadManifest();
  activePath = resolveInitialDoc();
  renderNav();

  if (!window.location.hash) {
    setHashForDoc(activePath);
  } else {
    await loadDocument(activePath);
  }

  searchInput.addEventListener("input", () => {
    renderNav(searchInput.value);
  });

  window.addEventListener("hashchange", async () => {
    const requestedPath = getPathFromHash();
    await loadDocument(requestedPath);
  });

  if (window.location.hash) {
    await loadDocument(getPathFromHash());
  }
}

init();
