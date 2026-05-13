#!/usr/bin/env bash
# setup-server.sh - 克隆 xiaozhi-server-go 并集成本地扩展代码
#
# 用法：
#   bash scripts/setup-server.sh [目标目录]
#
# 默认目标目录为项目根下的 xiaozhi-server-go/
# 如果目录已存在则只做增量集成（不重复克隆）。

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
SERVER_LOCAL="${ROOT_DIR}/server"

UPSTREAM_URL="https://github.com/AnimeAIChat/xiaozhi-server-go.git"
TARGET_DIR="${1:-${ROOT_DIR}/xiaozhi-server-go}"

echo "==> 目标目录: ${TARGET_DIR}"

# ── 1. 克隆或拉取上游仓库 ─────────────────────────────────────────────────────
if [ -d "${TARGET_DIR}/.git" ]; then
    echo "==> 仓库已存在，执行 git pull 更新..."
    git -C "${TARGET_DIR}" pull --ff-only
else
    echo "==> 克隆 xiaozhi-server-go ..."
    git clone "${UPSTREAM_URL}" "${TARGET_DIR}"
fi

# ── 2. 集成 DeepSeek Provider ─────────────────────────────────────────────────
DEEPSEEK_SRC="${SERVER_LOCAL}/src/core/providers/llm/deepseek/deepseek.go"
DEEPSEEK_DST="${TARGET_DIR}/src/core/providers/llm/deepseek"

if [ -f "${DEEPSEEK_SRC}" ]; then
    mkdir -p "${DEEPSEEK_DST}"
    cp -v "${DEEPSEEK_SRC}" "${DEEPSEEK_DST}/deepseek.go"
    echo "==> DeepSeek provider 已复制到 ${DEEPSEEK_DST}"
else
    echo "==> 警告: 找不到 ${DEEPSEEK_SRC}，跳过 DeepSeek 集成"
fi

# ── 3. 在 main.go 中注册 DeepSeek Provider ───────────────────────────────────
MAIN_GO="${TARGET_DIR}/src/main.go"
IMPORT_LINE='_ "xiaozhi-server-go/src/core/providers/llm/deepseek"'

if [ -f "${MAIN_GO}" ]; then
    if grep -q "deepseek" "${MAIN_GO}"; then
        echo "==> main.go 中已包含 deepseek 导入，跳过"
    else
        # 在第一个 import 块的末尾插入空白导入
        # 使用 Python 以避免 sed 在 macOS/Linux 间的差异
        PYEOF=$(mktemp /tmp/inject_import_XXXXXX.py)
        cat > "${PYEOF}" <<'PYSCRIPT'
import re, sys

main_go = sys.argv[1]
import_line = sys.argv[2]

with open(main_go, 'r') as f:
    content = f.read()

# Find the first import block and append the blank import before the closing paren
pattern = r'(import\s*\()(.*?)(\))'
def add_import(m):
    return m.group(1) + m.group(2) + '\t' + import_line + '\n' + m.group(3)

new_content = re.sub(pattern, add_import, content, count=1, flags=re.DOTALL)
with open(main_go, 'w') as f:
    f.write(new_content)
print("==> main.go 已更新，添加了 deepseek 导入")
PYSCRIPT
        python3 "${PYEOF}" "${MAIN_GO}" "${IMPORT_LINE}" || echo "==> 自动注入 import 失败，请手动在 ${MAIN_GO} 中添加: import ${IMPORT_LINE}"
        rm -f "${PYEOF}"
    fi
else
    echo "==> 警告: 找不到 ${MAIN_GO}"
fi

# ── 4. 复制示例配置 ───────────────────────────────────────────────────────────
EXAMPLE_CFG="${SERVER_LOCAL}/config.yaml.example"
SERVER_CFG="${TARGET_DIR}/config.yaml"

if [ ! -f "${SERVER_CFG}" ] && [ -f "${EXAMPLE_CFG}" ]; then
    echo "==> 未找到 config.yaml，复制 config.yaml.example ..."
    cp -v "${EXAMPLE_CFG}" "${SERVER_CFG}"
else
    echo "==> config.yaml 已存在或示例文件缺失，请手动将 DeepSeek 配置段合并进 config.yaml"
fi

echo ""
echo "✓ 完成！进入 ${TARGET_DIR} 后执行以下命令启动服务："
echo "  go mod tidy && go run src/main.go"
