/* app.js - Search logic, UI interaction, category filters */

(function () {
    'use strict';

    // === Configuration ===
    var DEBOUNCE_MS = 200;
    var MAX_RESULTS = 5;
    var SCORE_KEYWORD = 10;
    var SCORE_QUESTION = 5;
    var SCORE_ANSWER = 2;
    var LLM_THRESHOLD = 10;

    // === State ===
    var currentCategory = null;
    var searchTimer = null;
    var flatQuestions = [];

    // === Initialize ===
    function init() {
        flattenData();
        renderCategories();
        renderWelcome();
        bindEvents();
    }

    // Flatten nested category structure into flat array for searching
    function flattenData() {
        flatQuestions = [];
        var categories = qaData.categories;
        for (var c = 0; c < categories.length; c++) {
            var cat = categories[c];
            var questions = cat.questions;
            for (var q = 0; q < questions.length; q++) {
                flatQuestions.push({
                    categoryId: cat.id,
                    categoryLabel: cat.label,
                    q: questions[q].q,
                    a: questions[q].a,
                    a_short: questions[q].a_short,
                    keywords: questions[q].keywords,
                    audio_short: questions[q].audio_short,
                    audio_full: questions[q].audio_full
                });
            }
        }
    }

    // === Search Algorithm ===
    function search(query) {
        var terms = query.toLowerCase().split(/\s+/).filter(function (t) {
            return t.length > 0;
        });
        if (terms.length === 0) return [];

        var results = [];
        for (var i = 0; i < flatQuestions.length; i++) {
            var item = flatQuestions[i];
            var score = calculateScore(terms, item);
            if (score > 0) {
                results.push({
                    item: item,
                    score: score
                });
            }
        }

        results.sort(function (a, b) { return b.score - a.score; });
        return results.slice(0, MAX_RESULTS);
    }

    function calculateScore(terms, item) {
        var score = 0;
        var qLower = item.q.toLowerCase();
        var aLower = item.a.toLowerCase();

        for (var i = 0; i < terms.length; i++) {
            var term = terms[i];
            // Exact keyword match (highest weight)
            if (item.keywords && item.keywords.indexOf(term) !== -1) {
                score += SCORE_KEYWORD;
            }
            // Question text match
            if (qLower.indexOf(term) !== -1) {
                score += SCORE_QUESTION;
            }
            // Answer text match
            if (aLower.indexOf(term) !== -1) {
                score += SCORE_ANSWER;
            }
        }
        return score;
    }

    // === Expose search for external modules (voice.js, llm.js) ===
    window.searchQA = search;
    window.renderResults = renderSearchResults;
    window.getSearchInput = function () {
        return document.getElementById('search-input');
    };

    // === Rendering ===
    function renderCategories() {
        var bar = document.getElementById('category-bar');
        if (!bar) return;

        var html = '';
        var categories = qaData.categories;
        for (var i = 0; i < categories.length; i++) {
            var cat = categories[i];
            html += '<button class="category-btn" data-id="' + cat.id + '">'
                + escapeHtml(cat.label)
                + '</button>';
        }
        bar.innerHTML = html;
    }

    function renderWelcome() {
        var area = document.getElementById('results-area');
        if (!area) return;

        area.innerHTML =
            '<div class="welcome">' +
            '<h2>NexusFix Interactive Q&A</h2>' +
            '<p>182 pre-answered questions about the project. Pick a topic above or type a keyword below.</p>' +
            '<div class="popular-questions">' +
            '<h3>Popular questions</h3>' +
            '<div class="popular-link" data-q="Why not just use QuickFIX?">Why not just use QuickFIX?</div>' +
            '<div class="popular-link" data-q="How does SIMD scanning work?">How does SIMD scanning work?</div>' +
            '<div class="popular-link" data-q="What is the P99 latency?">What is the P99 latency?</div>' +
            '<div class="popular-link" data-q="What is PMR?">What is PMR and how does it help?</div>' +
            '<div class="popular-link" data-q="What is the structural index?">What is the structural index?</div>' +
            '</div>' +
            '</div>';
    }

    function renderSearchResults(results, query) {
        var area = document.getElementById('results-area');
        if (!area) return;

        var topScore = results.length > 0 ? results[0].score : 0;

        // If no results or top score below threshold, try LLM fallback
        if (topScore < LLM_THRESHOLD) {
            renderLowScoreResults(results, query, area);
            return;
        }

        var html = '<div class="result-count">' + results.length + ' result' +
            (results.length !== 1 ? 's' : '') + ' for "' + escapeHtml(query) + '"</div>';

        for (var i = 0; i < results.length; i++) {
            html += renderCard(results[i].item, results[i].score, query);
        }
        area.innerHTML = html;
        area.scrollTop = 0;
    }

    function renderLowScoreResults(results, query, area) {
        var html = '';

        // Show any low-score keyword results first
        if (results.length > 0) {
            html += '<div class="result-count">' + results.length +
                ' partial match' + (results.length !== 1 ? 'es' : '') +
                ' for "' + escapeHtml(query) + '"</div>';
            for (var i = 0; i < results.length; i++) {
                html += renderCard(results[i].item, results[i].score, query);
            }
        }

        // Try LLM fallback
        if (window.llmFallback && window.llmFallback.isConfigured()) {
            html += '<div class="llm-loading" id="llm-loading">' +
                '<div class="llm-loading-text">Asking AI assistant...</div>' +
                '</div>';
            area.innerHTML = html;
            area.scrollTop = 0;

            window.llmFallback.ask(query, function (result) {
                var loadingEl = document.getElementById('llm-loading');
                if (!loadingEl) return;

                if (result) {
                    loadingEl.outerHTML = renderLlmCard(result.answer, query);
                } else {
                    loadingEl.outerHTML = renderFallbackMsg(query);
                }
            });
        } else {
            // No LLM configured
            if (results.length === 0) {
                html += renderFallbackMsg(query);
            }
            area.innerHTML = html;
            area.scrollTop = 0;
        }
    }

    function renderLlmCard(answer, query) {
        return '<div class="qa-card llm-card expanded">' +
            '<div class="qa-header" onclick="window.toggleCard(this)">' +
            '<span class="qa-question">AI-generated answer' +
            '<span class="llm-badge">AI-GENERATED</span></span>' +
            '<span class="qa-expand-icon">&#9654;</span>' +
            '</div>' +
            '<div class="qa-body">' +
            '<div class="qa-full-answer">' + escapeHtml(answer) + '</div>' +
            '<div class="llm-disclaimer">This answer was generated by AI and may not be fully accurate. ' +
            'Browse topics above for verified answers.</div>' +
            '<div class="qa-actions">' +
            '<button class="qa-action-btn" onclick="window.speakAnswer(this, \'' +
            escapeJs(answer) + '\')">Speak</button>' +
            '</div>' +
            '</div>' +
            '</div>';
    }

    function renderFallbackMsg(query) {
        return '<div class="status-msg">' +
            'No match found for "' + escapeHtml(query) + '".<br>' +
            'Try browsing topics above or use different keywords.<br>' +
            '<span class="fallback-contact">Or contact <strong>contact@silverstream.tech</strong></span>' +
            '</div>';
    }

    function renderCategoryResults(categoryId) {
        var area = document.getElementById('results-area');
        if (!area) return;

        var items = [];
        for (var i = 0; i < flatQuestions.length; i++) {
            if (flatQuestions[i].categoryId === categoryId) {
                items.push(flatQuestions[i]);
            }
        }

        var label = '';
        var categories = qaData.categories;
        for (var c = 0; c < categories.length; c++) {
            if (categories[c].id === categoryId) {
                label = categories[c].label;
                break;
            }
        }

        var html = '<div class="category-heading">' + escapeHtml(label) +
            ' (' + items.length + ' questions)</div>';

        for (var j = 0; j < items.length; j++) {
            html += renderCard(items[j], null, null);
        }
        area.innerHTML = html;
        area.scrollTop = 0;
    }

    // Detect presenter mode
    var isPresenterMode = document.body && document.body.classList.contains('presenter-mode');

    function renderCard(item, score, query) {
        var scoreHtml = '';
        if (score !== null && score !== undefined) {
            var scoreClass = score >= 20 ? 'high' : (score >= 10 ? 'mid' : 'low');
            scoreHtml = '<span class="qa-score ' + scoreClass + '">' + score + '</span>';
        }

        var questionText = query ? highlightTerms(escapeHtml(item.q), query) : escapeHtml(item.q);

        // Store item index for presenter play buttons
        var itemIdx = flatQuestions.indexOf(item);

        var actionsHtml;
        if (isPresenterMode) {
            actionsHtml =
                '<button class="qa-play-btn" onclick="window.playQA(' + itemIdx + ', \'short\')">Play Short</button>' +
                '<button class="qa-play-btn" onclick="window.playQA(' + itemIdx + ', \'full\')">Play Full</button>' +
                '<button class="qa-action-btn" onclick="window.speakAnswer(this, ' + "'" +
                escapeJs(item.a_short) + "'" + ')">Speak Short</button>' +
                '<button class="qa-action-btn" onclick="window.speakAnswer(this, ' + "'" +
                escapeJs(item.a) + "'" + ')">Speak Full</button>';
        } else {
            actionsHtml =
                '<button class="qa-action-btn" onclick="window.speakAnswer(this, ' + "'" +
                escapeJs(item.a_short) + "'" + ')">Speak Short</button>' +
                '<button class="qa-action-btn" onclick="window.speakAnswer(this, ' + "'" +
                escapeJs(item.a) + "'" + ')">Speak Full</button>';
        }

        var html =
            '<div class="qa-card">' +
            '<div class="qa-header" onclick="window.toggleCard(this)">' +
            scoreHtml +
            '<span class="qa-question">' + questionText + '</span>' +
            '<span class="qa-category-tag">' + escapeHtml(item.categoryLabel) + '</span>' +
            '<span class="qa-expand-icon">&#9654;</span>' +
            '</div>' +
            '<div class="qa-body">' +
            '<div class="qa-short-answer">' + escapeHtml(item.a_short) + '</div>' +
            '<div class="qa-full-answer">' + formatAnswer(item.a) + '</div>' +
            '<div class="qa-actions">' +
            actionsHtml +
            '</div>' +
            '</div>' +
            '</div>';
        return html;
    }

    // Presenter mode: play pre-generated audio via AudioPipeline
    window.playQA = function (itemIdx, version) {
        if (itemIdx < 0 || itemIdx >= flatQuestions.length) return;
        var item = flatQuestions[itemIdx];
        if (window.audioPipeline) {
            window.audioPipeline.playAnswer(item, version);
        }
    };

    // === Text Formatting ===
    function formatAnswer(text) {
        // Escape HTML first
        var html = escapeHtml(text);
        // Wrap inline code: backtick-wrapped terms
        html = html.replace(/`([^`]+)`/g, '<code>$1</code>');
        return html;
    }

    function highlightTerms(html, query) {
        var terms = query.toLowerCase().split(/\s+/).filter(function (t) {
            return t.length > 1;
        });
        for (var i = 0; i < terms.length; i++) {
            var escaped = terms[i].replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
            var re = new RegExp('(' + escaped + ')', 'gi');
            html = html.replace(re, '<mark>$1</mark>');
        }
        return html;
    }

    function escapeHtml(str) {
        return str
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;');
    }

    function escapeJs(str) {
        return str
            .replace(/\\/g, '\\\\')
            .replace(/'/g, "\\'")
            .replace(/\n/g, '\\n')
            .replace(/\r/g, '');
    }

    // === UI Interaction ===
    window.toggleCard = function (headerEl) {
        var card = headerEl.parentElement;
        card.classList.toggle('expanded');
    };

    window.speakAnswer = function (btn, text) {
        // Stop any current speech
        speechSynthesis.cancel();

        // Remove speaking class from all buttons
        var allBtns = document.querySelectorAll('.qa-action-btn.speaking');
        for (var i = 0; i < allBtns.length; i++) {
            allBtns[i].classList.remove('speaking');
        }

        var utterance = new SpeechSynthesisUtterance(text);
        utterance.lang = 'en-US';
        utterance.rate = 0.9;

        btn.classList.add('speaking');
        utterance.onend = function () {
            btn.classList.remove('speaking');
        };
        utterance.onerror = function () {
            btn.classList.remove('speaking');
        };

        speechSynthesis.speak(utterance);
    };

    // === Event Binding ===
    function bindEvents() {
        var input = document.getElementById('search-input');
        var searchBtn = document.getElementById('search-btn');
        var categoryBar = document.getElementById('category-bar');

        if (input) {
            input.addEventListener('input', function () {
                clearTimeout(searchTimer);
                var query = input.value.trim();
                if (query.length === 0) {
                    clearActiveCategory();
                    renderWelcome();
                    return;
                }
                searchTimer = setTimeout(function () {
                    clearActiveCategory();
                    var results = search(query);
                    renderSearchResults(results, query);
                }, DEBOUNCE_MS);
            });

            input.addEventListener('keydown', function (e) {
                if (e.key === 'Enter') {
                    clearTimeout(searchTimer);
                    var query = input.value.trim();
                    if (query.length > 0) {
                        clearActiveCategory();
                        var results = search(query);
                        renderSearchResults(results, query);
                    }
                }
            });
        }

        if (searchBtn) {
            searchBtn.addEventListener('click', function () {
                clearTimeout(searchTimer);
                var query = input ? input.value.trim() : '';
                if (query.length > 0) {
                    clearActiveCategory();
                    var results = search(query);
                    renderSearchResults(results, query);
                }
            });
        }

        if (categoryBar) {
            categoryBar.addEventListener('click', function (e) {
                var btn = e.target;
                if (!btn.classList.contains('category-btn')) return;

                var catId = btn.getAttribute('data-id');

                if (currentCategory === catId) {
                    // Deselect: go back to welcome
                    clearActiveCategory();
                    if (input) input.value = '';
                    renderWelcome();
                    return;
                }

                clearActiveCategory();
                currentCategory = catId;
                btn.classList.add('active');
                if (input) input.value = '';
                renderCategoryResults(catId);
            });
        }

        // Popular questions
        document.addEventListener('click', function (e) {
            var link = e.target;
            if (!link.classList.contains('popular-link')) return;

            var q = link.getAttribute('data-q');
            if (q && input) {
                input.value = q;
                clearActiveCategory();
                var results = search(q);
                renderSearchResults(results, q);
                input.focus();
            }
        });
    }

    function clearActiveCategory() {
        currentCategory = null;
        var btns = document.querySelectorAll('.category-btn.active');
        for (var i = 0; i < btns.length; i++) {
            btns[i].classList.remove('active');
        }
    }

    // === Start ===
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', init);
    } else {
        init();
    }
})();
