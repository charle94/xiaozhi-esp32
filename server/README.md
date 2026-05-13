# xiaozhi-server-go (后端服务)

本目录包含 ESP32 固件配套的 Go 语言后端服务扩展代码，基于 [AnimeAIChat/xiaozhi-server-go](https://github.com/AnimeAIChat/xiaozhi-server-go) 项目进行定制开发。

## 目录结构

```
server/
├── src/
│   └── core/
│       └── providers/
│           └── llm/
│               └── deepseek/        # DeepSeek LLM Provider（OpenAI 兼容格式）
│                   └── deepseek.go
├── config.yaml.example              # 包含 DeepSeek 配置的示例配置文件
└── README.md
```

## 快速开始

### 1. 初始化服务

运行以下脚本克隆 xiaozhi-server-go 并将本目录的扩展代码合并进去：

```bash
bash ../scripts/setup-server.sh
```

### 2. 集成 DeepSeek Provider

将 `src/core/providers/llm/deepseek/deepseek.go` 复制到克隆后的服务目录中：

```bash
cp src/core/providers/llm/deepseek/deepseek.go \
   xiaozhi-server-go/src/core/providers/llm/deepseek/deepseek.go
```

然后在 `xiaozhi-server-go/src/main.go` 中添加对该 package 的空白导入（触发 `init()` 注册），例如：

```go
import (
    _ "xiaozhi-server-go/src/core/providers/llm/deepseek"
)
```

### 3. 配置服务

参考 `config.yaml.example` 中的 DeepSeekLLM 配置段，将其复制到服务的 `config.yaml` 中并填写 API Key。

```yaml
LLM:
  DeepSeekLLM:
    type: deepseek
    model_name: deepseek-chat
    url: https://api.deepseek.com/v1
    api_key: 你的deepseek_api_key
    max_tokens: 4096
```

## ESP32 固件侧配置

在 `idf.py menuconfig` → **Xiaozhi Assistant** 中，可以设置：

| 配置项 | 说明 |
|--------|------|
| `CONFIG_WEBSOCKET_URL` | 后端 WebSocket 地址（留空则从 OTA 服务获取） |
| `CONFIG_WEBSOCKET_ACCESS_TOKEN` | Bearer Token（留空则从 OTA 服务获取） |

例如，若本地运行 xiaozhi-server-go（默认端口 8000），可设置：

```
CONFIG_WEBSOCKET_URL="ws://192.168.1.100:8000"
CONFIG_WEBSOCKET_ACCESS_TOKEN="你的token"
```

固件在启动时，如果 NVS 中未保存 WebSocket 地址，会自动使用这两个编译期配置值；OTA 服务返回的配置会覆盖 NVS，优先级高于编译期默认值。

## 相关链接

- [xiaozhi-server-go 上游仓库](https://github.com/AnimeAIChat/xiaozhi-server-go)
- [DeepSeek API 文档](https://platform.deepseek.com/api-docs/)
- [ESP32 固件仓库](https://github.com/xinnan-tech/xiaozhi-esp32)
