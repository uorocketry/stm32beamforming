#!/usr/bin/env bash
set -euo pipefail

repo_root="$(git rev-parse --show-toplevel)"
cd "$repo_root"

revision="$(git rev-parse HEAD)"
node_id="${CAN_NODE_ID:-1}"

make -C tests clean test
make -C tests clean test \
  CFLAGS="-std=c17 -Wall -Wextra -Werror -pedantic -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer"
python3 tests/test_can_smoke_tool.py
cppcheck \
  --enable=warning,style,performance,portability \
  --error-exitcode=1 \
  --std=c17 \
  --suppress=missingIncludeSystem \
  --suppress=unusedFunction \
  -DSTM32F0 -DSTM32F072R8T6 -DBEAMFORMER_NODE_ID="$node_id" \
  -Iapp/include -Ilibopencm3/include \
  app/src
make -C libopencm3 TARGETS=stm32/f0 -j2
make -C app clean
make -C app all -j2 GIT_REVISION="$revision" CAN_NODE_ID="$node_id"
arm-none-eabi-size app/build/stm32beamforming.elf
git diff --check
