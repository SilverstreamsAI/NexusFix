/* llm.js - LLM fallback for out-of-scope questions (Claude API) */

(function () {
    'use strict';

    var STORAGE_KEY = 'nexusfix_qa_api_key';
    var MODEL = 'claude-sonnet-4-5-20250929';
    var MAX_TOKENS = 300;
    var SYSTEM_PROMPT =
        'You are a Q&A assistant for NexusFIX, a modern C++23 FIX protocol engine. ' +
        'Answer questions about NexusFIX architecture, performance, C++ techniques, and FIX protocol. ' +
        'Keep answers concise (2-4 sentences). If the question is unrelated to NexusFIX or C++, politely decline.';

    // === LLMFallback Class ===
    function LLMFallback() {
        this.apiKey = localStorage.getItem(STORAGE_KEY) || '';
    }

    LLMFallback.prototype.isConfigured = function () {
        return this.apiKey.length > 0;
    };

    LLMFallback.prototype.setApiKey = function (key) {
        this.apiKey = key.trim();
        if (this.apiKey) {
            localStorage.setItem(STORAGE_KEY, this.apiKey);
        } else {
            localStorage.removeItem(STORAGE_KEY);
        }
    };

    LLMFallback.prototype.getApiKey = function () {
        return this.apiKey;
    };

    LLMFallback.prototype.ask = function (question, callback) {
        if (!this.isConfigured()) {
            callback(null);
            return;
        }

        var body = JSON.stringify({
            model: MODEL,
            max_tokens: MAX_TOKENS,
            system: SYSTEM_PROMPT,
            messages: [{ role: 'user', content: question }]
        });

        var xhr = new XMLHttpRequest();
        xhr.open('POST', 'https://api.anthropic.com/v1/messages', true);
        xhr.setRequestHeader('Content-Type', 'application/json');
        xhr.setRequestHeader('x-api-key', this.apiKey);
        xhr.setRequestHeader('anthropic-version', '2023-06-01');
        xhr.setRequestHeader('anthropic-dangerous-direct-browser-access', 'true');

        xhr.onload = function () {
            if (xhr.status >= 200 && xhr.status < 300) {
                try {
                    var data = JSON.parse(xhr.responseText);
                    var text = data.content && data.content[0] && data.content[0].text;
                    if (text) {
                        callback({
                            answer: text,
                            source: 'llm',
                            model: MODEL
                        });
                        return;
                    }
                } catch (e) {
                    console.error('LLM response parse error:', e);
                }
            } else {
                console.error('LLM API error:', xhr.status, xhr.statusText);
            }
            callback(null);
        };

        xhr.onerror = function () {
            console.error('LLM network error');
            callback(null);
        };

        xhr.send(body);
    };

    // === Settings Panel Logic ===
    function initSettingsPanel() {
        var toggle = document.getElementById('settings-toggle');
        var panel = document.getElementById('settings-panel');
        var keyInput = document.getElementById('api-key-input');
        var saveBtn = document.getElementById('api-key-save');
        var clearBtn = document.getElementById('api-key-clear');
        var status = document.getElementById('api-key-status');

        if (!toggle || !panel) return;

        toggle.addEventListener('click', function () {
            panel.classList.toggle('open');
        });

        // Close panel when clicking outside
        document.addEventListener('click', function (e) {
            if (!panel.contains(e.target) && e.target !== toggle) {
                panel.classList.remove('open');
            }
        });

        if (keyInput && window.llmFallback) {
            // Show masked key if already saved
            updateStatus();

            if (saveBtn) {
                saveBtn.addEventListener('click', function () {
                    var key = keyInput.value.trim();
                    if (key) {
                        window.llmFallback.setApiKey(key);
                        keyInput.value = '';
                        updateStatus();
                    }
                });
            }

            if (clearBtn) {
                clearBtn.addEventListener('click', function () {
                    window.llmFallback.setApiKey('');
                    keyInput.value = '';
                    updateStatus();
                });
            }

            keyInput.addEventListener('keydown', function (e) {
                if (e.key === 'Enter' && saveBtn) {
                    saveBtn.click();
                }
            });
        }

        function updateStatus() {
            if (!status || !window.llmFallback) return;
            if (window.llmFallback.isConfigured()) {
                var key = window.llmFallback.getApiKey();
                var masked = key.substring(0, 8) + '...' + key.substring(key.length - 4);
                status.textContent = 'Key: ' + masked;
                status.className = 'api-key-status configured';
            } else {
                status.textContent = 'No API key configured (optional)';
                status.className = 'api-key-status';
            }
        }
    }

    // === Expose globally ===
    window.llmFallback = new LLMFallback();

    // Init settings panel on DOM ready
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', initSettingsPanel);
    } else {
        initSettingsPanel();
    }
})();
