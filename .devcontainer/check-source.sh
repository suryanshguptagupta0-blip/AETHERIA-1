#!/usr/bin/env bash
# Source checks only. No Unreal download, remote provisioning or APK claim.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 Tests/validate_project.py
python3 -m unittest discover -s Tests -p 'test_*.py' -v
mkdir -p Tests/.build
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -fno-exceptions -fno-rtti \
  Source/Aetheria/Rules/AetheriaRules.cpp Tests/RulesTests.cpp \
  -I Source/Aetheria/Rules -o Tests/.build/rules-tests
Tests/.build/rules-tests
