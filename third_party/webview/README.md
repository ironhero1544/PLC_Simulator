# Embedded guide reader dependencies

- `webview.h`: webview 0.12.0, tag commit
  `3ab4b5d722438fc8a13e6ca830c5e2372d19a01d`, MIT.
  https://github.com/webview/webview/tree/0.12.0
- `WebView2.h`: Microsoft.Web.WebView2 NuGet SDK 1.0.4191.47.
  Distribution terms are in `WebView2-LICENSE.txt`.
  https://www.nuget.org/packages/Microsoft.Web.WebView2/1.0.4191.47

The header uses webview's built-in dynamic WebView2 loader, so no MSVC import
library or loader DLL is required. This also keeps the Windows MinGW build
compatible. The user's system needs the Microsoft Edge WebView2 Runtime.

The offline Markdown renderer's third-party scripts, styles, fonts and licenses
are bundled in `resources/help/vendor` (marked 16.4.2, DOMPurify 3.3.1,
highlight.js 11.11.1, KaTeX 0.16.22, Mermaid 9.4.3).
