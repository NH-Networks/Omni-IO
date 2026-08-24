(function () {
    function pageElement(name) {
        return document.getElementById(name + "-page");
    }

    function setPageDisplay(name, activePage) {
        const element = pageElement(name);
        if (!element) {
            return;
        }
        element.style.display = activePage === name ? "grid" : "none";
    }

    function closeMobileMenu() {
        const nav = document.querySelector("header nav");
        if (nav) {
            nav.classList.remove("open");
        }
        document.body.classList.remove("menu-open");
    }

    window.showPage = function (page) {
        setPageDisplay("devices", page);
        setPageDisplay("twow", page);
        setPageDisplay("settings", page);
        setPageDisplay("help", page);
        closeMobileMenu();
    };

    document.addEventListener("DOMContentLoaded", function () {
        const nav = document.querySelector("header nav");
        const toggle = document.getElementById("menu-toggle");
        const close = document.getElementById("menu-close");
        const backdrop = document.getElementById("menu-backdrop");

        function openMenu() {
            if (nav) {
                nav.classList.add("open");
            }
            document.body.classList.add("menu-open");
        }

        if (toggle) {
            toggle.addEventListener("click", openMenu);
        }
        if (close) {
            close.addEventListener("click", closeMobileMenu);
        }
        if (backdrop) {
            backdrop.addEventListener("click", closeMobileMenu);
        }


        const logo = document.querySelector("header img.logo");
        if (logo) {
            logo.addEventListener("click", function () {
                window.showPage("devices");
            });
            logo.addEventListener("keydown", function (e) {
                if (e.key === "Enter" || e.key === " ") {
                    e.preventDefault();
                    window.showPage("devices");
                }
            });
        }

        if (!pageElement("devices") || pageElement("devices").style.display === "none") {
            window.showPage("devices");
        }
    });
}());
