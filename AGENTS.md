# Omni-IO Licensing Rules

When creating new files or modifying existing files in this repository, strictly adhere to the following licensing rules based on the hybrid Apache 2.0 / Proprietary model.

## 1. Creating NEW files
Any entirely new file created for this project must be licensed under the CloudAXS Proprietary License. You MUST prepend the following SPDX header (using the appropriate comment syntax for the language):

```
SPDX-FileCopyrightText: 2026 CloudAXS
SPDX-License-Identifier: LicenseRef-CloudAXS-Proprietary
```

## 2. Modifying EXISTING files
When modifying an existing file that originated from the upstream repository (Apache 2.0 licensed), do NOT apply the proprietary license to the whole file. Instead, you MUST prepend the following Modification Notice (using the appropriate comment syntax for the language):

```
Modifications Copyright 2026 CloudAXS.
Original upstream portions remain licensed under Apache-2.0.
```

## 3. JSON Files
Standard JSON does not support comments. When adding these notices to a `.json` file, inject them as a root-level key named `"_copyright"`.

## 4. Release & Deployment Rules
- **Branch builds with GitHub Actions**: Pushing commits to active branches (`master`, `Beta`, `dev-main`, `feature/**`) to trigger automated CI/CD branch builds via `.github/workflows/branch_build.yml` is encouraged during development.
- **NO version releases without approval**: DO NOT create or push release tags (`v*`) or publish release notes unless the user explicitly gives permission.
- **Proactively suggest releases**: When a build or milestone is mature, verified, and production-ready, proactively propose a version release to the user for consideration and approval.
- **Mandatory Self-Testing on Target Gateway**:
  - Always upload and verify changes directly on the local test device at `10.10.33.15` before considering a task completed.
  - Use `scratch/upload_ota.py` (run via `uv run python scratch/upload_ota.py`) to flash firmware and filesystem over OTA and verify `/api/info`.

