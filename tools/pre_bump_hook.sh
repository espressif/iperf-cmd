#!/bin/bash
# Document https://commitizen-tools.github.io/commitizen/
# eg usage: `cz bump --files-only --increment=PATCH`
set -e

test -n "$CZ_PRE_CURRENT_VERSION" || (echo "CZ_PRE_CURRENT_VERSION must be set!"; exit 1)
test -n "$CZ_PRE_NEW_VERSION" || (echo "CZ_PRE_NEW_VERSION must be set!"; exit 1)

# Update Changelog files
if [ "${CZ_PRE_INCREMENT:-}" == "PATCH" ]; then
    # For features or breaking changes, do not add new version to changelog
    for file in "iperf/CHANGELOG.md" "iperf-cmd/CHANGELOG.md";
    do
        # The CURRENT_VERSION line was replaced to NEW_VERSION by commitizen
        # Insert CURRENT_VERSION line to the changelog file
        CHANGELOG_STR=$(sed -n '3p' ${file})
        CHANGELOG_STR=${CHANGELOG_STR//$CZ_PRE_NEW_VERSION/$CZ_PRE_CURRENT_VERSION}
        sed -i "3 a $CHANGELOG_STR" ${file}
        sed -i "3 a
        " ${file}
    done
fi
