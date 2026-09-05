# SPDX-FileCopyrightText: 2026 CloudAXS
# SPDX-License-Identifier: LicenseRef-CloudAXS-Proprietary
import urllib.request
import json

HOST = "http://10.10.33.15"
checks = []

# 1. Check HTML
html = urllib.request.urlopen(f"{HOST}/", timeout=5).read().decode("utf-8")
checks.append(("HTML served", len(html) > 10000))
checks.append(("HTML contains help-page.grid", 'id="help-page" class="grid"' in html))
checks.append(("HTML contains nav.status", 'data-i18n="nav.status"' in html))
checks.append(("HTML contains img/system.svg", 'img/system.svg' in html))
checks.append(("HTML contains select-log", 'id="select-log"' in html))
checks.append(("HTML contains clear-log-button", 'id="clear-log-button"' in html))
checks.append(("HTML contains copy-log-button", 'id="copy-log-button"' in html))
checks.append(("HTML contains rf-sniffer-section", 'id="rf-sniffer-section"' in html))
checks.append(("HTML contains last-address-badge", 'id="last-address-badge"' in html))
checks.append(("HTML contains copy-address-button", 'id="copy-address-button"' in html))
checks.append(("HTML contains use-address-remote-button", 'id="use-address-remote-button"' in html))
checks.append(("HTML contains diagnostics-section", 'id="diagnostics-section"' in html))
checks.append(("HTML contains quick-help-section", 'id="quick-help-section"' in html))

# 2. Check CSS
css = urllib.request.urlopen(f"{HOST}/css/style.css", timeout=5).read().decode("utf-8")
checks.append(("CSS contains .log-actions", ".log-actions" in css))
checks.append(("CSS contains .rf-hex-badge", ".rf-hex-badge" in css))
checks.append(("CSS contains .help-accordion", ".help-accordion" in css))

# 3. Check JS
js = urllib.request.urlopen(f"{HOST}/js/app.js", timeout=5).read().decode("utf-8")
checks.append(("JS contains applyLogFilter", "applyLogFilter" in js))
checks.append(("JS contains updateLastAddressBadge", "updateLastAddressBadge" in js))
checks.append(("JS contains updateLogCount", "updateLogCount" in js))

# 4. Check Translations
for lang in ["nl", "en", "de", "fr"]:
    lang_data = json.loads(urllib.request.urlopen(f"{HOST}/lang/{lang}.json", timeout=5).read().decode("utf-8"))
    checks.append((f"{lang}.json has section.rf_sniffer", "section.rf_sniffer" in lang_data))
    checks.append((f"{lang}.json has section.test_console", "section.test_console" in lang_data))
    checks.append((f"{lang}.json has section.quick_help", "section.quick_help" in lang_data))
    checks.append((f"{lang}.json has copyright header", "_copyright" in lang_data))

# 5. Check API endpoints
info = json.loads(urllib.request.urlopen(f"{HOST}/api/info", timeout=5).read().decode("utf-8"))
checks.append(("API /api/info OK", info.get("ip") == "10.10.33.15"))

lastaddr = json.loads(urllib.request.urlopen(f"{HOST}/api/lastaddr", timeout=5).read().decode("utf-8"))
checks.append(("API /api/lastaddr OK", "address" in lastaddr))

logs = json.loads(urllib.request.urlopen(f"{HOST}/api/logs", timeout=5).read().decode("utf-8"))
checks.append(("API /api/logs OK", isinstance(logs, list)))

print("=== VALIDATION SUMMARY ON 10.10.33.15 ===")
all_ok = True
for name, passed in checks:
    status = "PASS" if passed else "FAIL"
    if not passed:
        all_ok = False
    print(f"[{status}] {name}")

print(f"\nTotal: {len(checks)} checks. Overall status: {'ALL PASSED' if all_ok else 'SOME FAILED'}")
