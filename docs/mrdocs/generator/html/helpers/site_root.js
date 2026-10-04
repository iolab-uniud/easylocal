// The documentation site's root, relative to a page: the API reference is
// served under api/, so the root is the parent of relativize "/index.html".
function site_root(api_index) {
    return api_index.replace(/index\.html$/, "") + "../";
}
