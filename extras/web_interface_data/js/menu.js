(function () {
    if (window.MiOpenMenuReady) return;
    window.MiOpenMenuReady = true;

    function setMenuOpen(open) {
        var toggle = document.getElementById("menu-toggle");
        document.body.classList.toggle("menu-open", open);
        if (toggle) toggle.setAttribute("aria-expanded", open ? "true" : "false");
    }

    window.showPage = function (page) {
        var pages = {
            devices: document.getElementById("devices-page"),
            twow: document.getElementById("twow-page"),
            help: document.getElementById("help-page"),
            settings: document.getElementById("settings-page")
        };
        Object.keys(pages).forEach(function (key) {
            if (!pages[key]) return;
            pages[key].style.display = key === page ? "grid" : "none";
        });
        setMenuOpen(false);
        return false;
    };

    function init() {
        var toggle = document.getElementById("menu-toggle");
        var close = document.getElementById("menu-close");
        var backdrop = document.getElementById("menu-backdrop");
        var menu = document.getElementById("main-menu");
        if (!toggle || !menu) return;

        toggle.addEventListener("click", function () {
            setMenuOpen(!document.body.classList.contains("menu-open"));
        });
        if (close) close.addEventListener("click", function () { setMenuOpen(false); });
        if (backdrop) backdrop.addEventListener("click", function () { setMenuOpen(false); });
        menu.addEventListener("click", function (event) {
            var item = event.target.closest ? event.target.closest(".items") : null;
            if (item && item.tagName !== "SELECT") setMenuOpen(false);
        });
        document.addEventListener("keydown", function (event) {
            if (event.key === "Escape") setMenuOpen(false);
        });
        window.addEventListener("resize", function () {
            if (window.innerWidth > 700) setMenuOpen(false);
        });
    }

    if (document.readyState === "loading") {
        document.addEventListener("DOMContentLoaded", init);
    } else {
        init();
    }
}());
