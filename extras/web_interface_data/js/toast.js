(function () {
    let hideTimer = null;

    function ensureContainer() {
        let container = document.getElementById("toast-container");
        if (!container) {
            container = document.createElement("div");
            container.id = "toast-container";
            document.body.appendChild(container);
        }
        return container;
    }

    function ensureToast() {
        const container = ensureContainer();
        let toast = document.getElementById("toast");
        if (!toast) {
            toast = document.createElement("div");
            toast.id = "toast";
            toast.className = "toast";
            toast.hidden = true;
            container.appendChild(toast);
        }
        return toast;
    }

    function hideElement(toast) {
        toast.classList.add("toast-hide");
        setTimeout(function () {
            toast.hidden = true;
        }, 300);
    }

    window.showToast = function (message, isError, timeoutMs) {
        const toast = ensureToast();
        toast.textContent = message || "";
        toast.className = "toast " + (isError ? "toast-error" : "toast-success");
        toast.hidden = false;

        if (hideTimer) {
            clearTimeout(hideTimer);
        }
        hideTimer = setTimeout(function () {
            hideElement(toast);
        }, timeoutMs || 7000);
    };

    window.hideToast = function () {
        const toast = ensureToast();
        toast.hidden = true;
        if (hideTimer) {
            clearTimeout(hideTimer);
            hideTimer = null;
        }
    };
}());
