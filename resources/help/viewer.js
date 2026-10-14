'use strict';
const pages = window.GUIDE_DOCUMENTS;
const home = 'docs/guides/program/README.md';
const content = document.querySelector('#content');
const main = document.querySelector('#main');
let current = home, history = [], position = -1, fontSize = 16;
const root = new URL('./', location.href);
mermaid.initialize({ startOnLoad: false, securityLevel: 'strict', theme: 'neutral' });
marked.use({ extensions: [
  { name: 'blockMath', level: 'block', start: source => source.indexOf('$$'),
    tokenizer(source) { const match = /^\$\$\s*\n?([\s\S]+?)\n?\$\$(?:\n|$)/.exec(source); if (match) return { type: 'blockMath', raw: match[0], text: match[1] }; },
    renderer(token) { return katex.renderToString(token.text, { displayMode: true, throwOnError: false, trust: false }); } },
  { name: 'inlineMath', level: 'inline', start: source => source.indexOf('$'),
    tokenizer(source) { const match = /^\$([^$\n]+?)\$/.exec(source); if (match) return { type: 'inlineMath', raw: match[0], text: match[1] }; },
    renderer(token) { return katex.renderToString(token.text, { throwOnError: false, trust: false }); } }
] });
function resolve(link) { return new URL(link, new URL(current, root)); }
function pageKey(url) { return decodeURIComponent(url.pathname.slice(root.pathname.length)); }
function show(key, hash = '', record = true) {
  if (!pages[key]) return;
  if (record) {
    if (position >= 0) history[position].scroll = main.scrollTop;
    history = history.slice(0, position + 1);
    history.push({ key, hash, scroll: 0 }); position++;
  }
  current = key;
  content.innerHTML = DOMPurify.sanitize(marked.parse(pages[key].markdown, { gfm: true }), { ADD_ATTR: ['target'] });
  const slugs = new Map();
  content.querySelectorAll('h1,h2,h3,h4,h5,h6').forEach(heading => {
    let slug = heading.textContent.toLowerCase().replace(/[^\p{L}\p{N}_\-\s]/gu, '').replace(/\s/g, '-');
    const count = slugs.get(slug) || 0; slugs.set(slug, count + 1);
    heading.id = slug + (count ? '-' + count : '');
  });
  content.querySelectorAll('img').forEach(image => {
    const url = resolve(image.getAttribute('src'));
    if (url.protocol !== 'file:') {
      const description = document.createElement('span');
      description.textContent = image.alt;
      image.replaceWith(description);
      return;
    }
    image.src = url.href;
    image.addEventListener('click', () => {
      const dialog = document.querySelector('#picture');
      dialog.querySelector('img').src = image.src; dialog.showModal();
    });
  });
  content.querySelectorAll('a').forEach(anchor => {
    const url = resolve(anchor.getAttribute('href'));
    anchor.href = url.href;
    anchor.addEventListener('click', event => {
      const key = pageKey(url);
      if (pages[key]) { event.preventDefault(); show(key, url.hash); }
      else if (url.href.startsWith(root.href) && url.pathname.endsWith('.json')) {
        anchor.download = decodeURIComponent(url.pathname.split('/').pop());
      }
      else if (!['https:', 'http:', 'mailto:'].includes(url.protocol)) event.preventDefault();
      else { event.preventDefault(); window.openExternal?.(url.href); }
    });
  });
  content.querySelectorAll('pre').forEach(pre => {
    const code = pre.querySelector('code');
    if (!code) return;
    if (code.classList.contains('language-mermaid')) {
      const diagram = document.createElement('div'); diagram.className = 'mermaid'; diagram.textContent = code.textContent; pre.replaceWith(diagram); return;
    }
    hljs.highlightElement(code);
    const button = document.createElement('button'); button.className = 'copy'; button.textContent = '복사';
    button.onclick = async () => { try { await navigator.clipboard.writeText(code.textContent); button.textContent = '복사됨'; } catch { button.textContent = 'Ctrl+C로 복사'; } };
    pre.prepend(button);
  });
  // Mermaid renders asynchronously; only nodes belonging to this document are used.
  const diagrams = content.querySelectorAll('.mermaid');
  if (diagrams.length) mermaid.init(undefined, diagrams);
  document.querySelector('#title').textContent = pages[key].title;
  document.title = pages[key].title + ' — PLC Simulator';
  document.querySelector('#back').disabled = position <= 0;
  document.querySelector('#forward').disabled = position >= history.length - 1;
  main.scrollTop = record ? 0 : history[position].scroll;
  if (hash) document.getElementById(decodeURIComponent(hash.slice(1)))?.scrollIntoView();
  chapters(document.querySelector('#search').value);
}
function chapters(query = '') {
  const nav = document.querySelector('#chapters'); nav.replaceChildren();
  const needle = query.trim().toLowerCase();
  const keys = Object.keys(pages).sort((a,b) => (a === home ? -1 : b === home ? 1 : a.localeCompare(b)));
  keys.forEach(key => {
    if (!needle && !key.startsWith('docs/guides/program/')) return;
    if (needle && !(pages[key].title + pages[key].markdown).toLowerCase().includes(needle)) return;
    const button = document.createElement('button'); button.textContent = pages[key].title.replace(/^\d+\.\s*/, '');
    button.className = key === current ? 'active' : ''; button.onclick = () => show(key); nav.append(button);
  });
}
document.querySelector('#back').onclick = () => { history[position].scroll = main.scrollTop; position--; show(history[position].key, '', false); };
document.querySelector('#forward').onclick = () => { history[position].scroll = main.scrollTop; position++; show(history[position].key, '', false); };
document.querySelector('#home').onclick = () => show(home);
document.querySelector('#menu').onclick = () => document.body.classList.toggle('no-sidebar');
document.querySelector('#search').oninput = event => chapters(event.target.value);
document.querySelector('#close-picture').onclick = () => document.querySelector('#picture').close();
document.querySelector('#smaller').onclick = () => { fontSize = Math.max(12, fontSize - 1); content.style.fontSize = fontSize + 'px'; };
document.querySelector('#larger').onclick = () => { fontSize = Math.min(28, fontSize + 1); content.style.fontSize = fontSize + 'px'; };
show(home);
