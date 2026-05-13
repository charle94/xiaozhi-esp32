package deepseek

import (
	"xiaozhi-server-go/src/core/providers/llm"
	openai_provider "xiaozhi-server-go/src/core/providers/llm/openai"
)

const defaultBaseURL = "https://api.deepseek.com/v1"

// DeepSeek is an OpenAI-compatible LLM provider backed by the DeepSeek API.
// It reuses the openai provider implementation and only overrides the default
// base URL so that no extra code path is needed for streaming, tool-calls, etc.
//
// Registration: import this package with a blank identifier to trigger init():
//
//	import _ "xiaozhi-server-go/src/core/providers/llm/deepseek"
func init() {
	llm.Register("deepseek", func(config *llm.Config) (llm.Provider, error) {
		// Apply the DeepSeek default endpoint when the caller leaves url empty.
		if config.BaseURL == "" {
			config.BaseURL = defaultBaseURL
		}
		return openai_provider.NewProvider(config)
	})
}
