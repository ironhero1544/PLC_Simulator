'use strict';
const pages = window.GUIDE_DOCUMENTS;
const requestedLanguage = new URLSearchParams(location.search).get('lang');
const language = ['en', 'ja'].includes(requestedLanguage) ? requestedLanguage : 'ko';
const suffix = language === 'ko' ? '' : '.' + language;
const home = 'docs/guides/program/README' + suffix + '.md';
const labels = {
  ko: { menu: '목차 접기', back: '이전 문서', forward: '다음 문서', home: '목차',
    smaller: '글자 축소', larger: '글자 확대', search: '가이드 검색',
    placeholder: '배선, 타이머, HMI…', hint: 'Ctrl+F: 문서 안에서 찾기',
    close: '닫기', picture: '확대한 가이드 이미지', copy: '복사', copied: '복사됨',
    copyFallback: 'Ctrl+C로 복사' },
  en: { menu: 'Toggle contents', back: 'Previous document', forward: 'Next document',
    home: 'Contents', smaller: 'Smaller text', larger: 'Larger text', search: 'Search guides',
    placeholder: 'Wiring, timers, HMI…', hint: 'Ctrl+F: Find in this document',
    close: 'Close', picture: 'Enlarged guide image', copy: 'Copy', copied: 'Copied',
    copyFallback: 'Copy with Ctrl+C' },
  ja: { menu: '目次の表示切り替え', back: '前の文書', forward: '次の文書',
    home: '目次', smaller: '文字を縮小', larger: '文字を拡大', search: 'ガイドを検索',
    placeholder: '配線、タイマー、HMI…', hint: 'Ctrl+F：文書内を検索',
    close: '閉じる', picture: '拡大したガイド画像', copy: 'コピー', copied: 'コピーしました',
    copyFallback: 'Ctrl+Cでコピー' }
}[language];
document.documentElement.lang = language;
['menu', 'back', 'forward', 'smaller', 'larger'].forEach(id => {
  document.getElementById(id).title = labels[id];
});
document.getElementById('home').textContent = labels.home;
document.querySelector('label[for="search"]').textContent = labels.search;
document.getElementById('search').placeholder = labels.placeholder;
document.querySelector('.hint').textContent = labels.hint;
document.getElementById('close-picture').textContent = labels.close;
document.querySelector('#picture img').alt = labels.picture;
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
    if (!anchor.hasAttribute('href')) return;
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
    const button = document.createElement('button'); button.className = 'copy'; button.textContent = labels.copy;
    button.onclick = async () => { try { await navigator.clipboard.writeText(code.textContent); button.textContent = labels.copied; } catch { button.textContent = labels.copyFallback; } };
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
    if (pages[key].language !== language) return;
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
