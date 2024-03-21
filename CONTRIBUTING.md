# Contributing Guidelines

Welcome to the iperf-cmd project! We appreciate your interest in contributing. Whether you're reporting an issue, proposing a feature, or submitting a pull request (PR), your contributions are valuable.

## Submitting a Pull Request (PR)

- **Fork the Repository:**
  - Fork the [iperf-cmd repository](https://github.com/espressif/iperf-cmd) on GitHub to start making your changes.

- **Install Pre-commit Hooks:**
  - Run the following commands to set up pre-commit hooks for code linting:

    ```bash
    pip install pre-commit
    pre-commit install-hooks
    pre-commit install --hook-type commit-msg --hook-type pre-push

    # optional for doxygen-api-md
    sudo apt install doxygen
    pip install esp-doxybook
    ```

- **Commit Message Format:**
  - Please follow the [conventional commits](https://www.conventionalcommits.org/en/v1.0.0/) rule when writing commit messages.
  - A typical commit message title template:
    - Template: `[type]([scope]): Message`
    - e.g: `feat(iperf): Support option --format in iperf command`

- **Send a Pull Request (PR):**

  - Submit your PR and collaborate with us through the review process. Please be patient, as multiple rounds of review and adjustments may be needed.

  - For faster merging, keep your contribution focused on a single feature or topic. Larger contributions may require additional time for review.


Your PR may not be published until a new version of this component is released.

## Release process

When releasing a new component version we have to:

- Check and update the changelog.
- Create the version tag in this repository, then pass build/test jobs.
- Create a new github release and publish the component to component registry.
- Bump a new version for next release.

  - Create a new branch from the latest release

  ```
    # bump new dev version
    - git branch -D bump/new_patch_version || true
    - git checkout -b bump/new_patch_version
    - git tag --delete v$(cz version --project) || true
  ```

  - Bump new version

  ```bash
  # new patch version
  cz bump --increment=PATCH
  # new dev version
  cz bump --increment=PATCH --devrelease <number>
  # new custom version
  cz bump 0.2.0-dev1
  ```

  - Push the new branch to remote repo and create a MR.
  - NOTE: local tags needs to be deleted, do not push the new tag to remote repository.
