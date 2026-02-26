const qaData = {
  "categories": [
    {
      "id": "general",
      "label": "General Project",
      "questions": [
        {
          "q": "What motivated you to build NexusFix? Why not just use QuickFIX?",
          "a": "QuickFIX was designed in the early 2000s around C++98/03 idioms: virtual dispatch, heap allocation per message, string-based field storage. These patterns are fundamentally incompatible with sub-microsecond latency requirements. Rather than wrapping QuickFIX with optimizations (which would be a workaround, not a solution), we started from first principles using C++23 capabilities that didn't exist when QuickFIX was designed. The goal was to prove that modern C++ standard library features - PMR, constexpr, concepts, std::expected - can replace thousands of lines of hand-tuned C++11 infrastructure code.",
          "a_short": "QuickFIX uses C++98 idioms incompatible with sub-microsecond latency. NexusFix starts from first principles using C++23 features like PMR, constexpr, and std::expected.",
          "keywords": ["motivation", "why", "quickfix", "build", "started", "c++98", "latency"],
          "audio_short": "audio/general_q01_short.mp3",
          "audio_full": "audio/general_q01_full.mp3"
        },
        {
          "q": "Who is the target audience for NexusFix?",
          "a": "Quantitative trading firms and infrastructure teams that need deterministic, sub-microsecond FIX message processing. Specifically: firms running co-located strategies where every nanosecond of parsing latency translates to execution disadvantage, teams building custom trading gateways who need a parsing layer without framework lock-in, and C++ engineers who want to study how modern C++ techniques apply to real-world performance-critical systems.",
          "a_short": "Quantitative trading firms needing sub-microsecond FIX parsing, teams building custom gateways, and C++ engineers studying modern performance techniques.",
          "keywords": ["audience", "who", "target", "trading", "quantitative", "users"],
          "audio_short": "audio/general_q02_short.mp3",
          "audio_full": "audio/general_q02_full.mp3"
        },
        {
          "q": "Is anyone using NexusFix in production?",
          "a": "NexusFix is in active development and has not yet been deployed in production trading environments. The benchmark results are from controlled environments with CPU pinning and cache warming. Production deployment introduces additional variables (kernel scheduling, network jitter, competitor load) that we haven't characterized yet. We're transparent about this: the project demonstrates what's achievable with modern C++, and production hardening is a separate engineering effort.",
          "a_short": "Not yet in production. Benchmarks are from controlled environments. The project demonstrates modern C++ capabilities, with production hardening as a separate effort.",
          "keywords": ["production", "deployed", "using", "real", "live"],
          "audio_short": "audio/general_q03_short.mp3",
          "audio_full": "audio/general_q03_full.mp3"
        },
        {
          "q": "How long did it take to build NexusFix?",
          "a": "The core parser went from zero to sub-200ns in approximately 12 weeks, following a phased implementation plan. However, the time investment is misleading - each optimization phase required deep research into CPU microarchitecture, assembly analysis, and benchmark methodology. The code is ~2,000 lines, but the knowledge behind each line took disproportionate effort. The structural index alone (TICKET_208) required reading the simdjson paper and adapting the technique to FIX protocol semantics.",
          "a_short": "Core parser reached sub-200ns in about 12 weeks. The ~2,000 lines of code required deep research into CPU microarchitecture and benchmark methodology.",
          "keywords": ["time", "long", "duration", "weeks", "effort", "development"],
          "audio_short": "audio/general_q04_short.mp3",
          "audio_full": "audio/general_q04_full.mp3"
        },
        {
          "q": "Is this a solo project or a team effort?",
          "a": "NexusFix is developed by SilverstreamsAI. The architectural decisions, implementation, and benchmark methodology reflect a focused engineering effort. The project benefits from the broader C++ community's work - simdjson's two-stage parsing insight, the LMAX Disruptor pattern, and the C++ standards committee's work on PMR, constexpr, and SIMD support.",
          "a_short": "Developed by SilverstreamsAI, benefiting from the broader C++ community's work including simdjson, LMAX Disruptor, and C++ standards committee contributions.",
          "keywords": ["solo", "team", "who", "developer", "silverstreams"],
          "audio_short": "audio/general_q05_short.mp3",
          "audio_full": "audio/general_q05_full.mp3"
        },
        {
          "q": "What's on the roadmap? Any plans for FIX 5.0 or FIXT 1.1?",
          "a": "Near-term: production hardening, session management completion, and SIMD-accelerated integer parsing for numeric fields. Medium-term: FIX 5.0/FIXT 1.1 support, which is incremental because the structural index is version-agnostic - field scanning works identically across FIX versions. Longer-term: IPC pipeline integration using our MPSC queue and SBE encoding for internal message passing between gateway and strategy processes. We prioritize depth over breadth - each feature must meet the same latency standards before release.",
          "a_short": "Near-term: production hardening and SIMD integer parsing. Medium-term: FIX 5.0 support. Longer-term: IPC pipeline with MPSC queue and SBE encoding.",
          "keywords": ["roadmap", "future", "plans", "fix5", "fixt", "next"],
          "audio_short": "audio/general_q06_short.mp3",
          "audio_full": "audio/general_q06_full.mp3"
        },
        {
          "q": "How does NexusFix compare to other open-source FIX engines beyond QuickFIX?",
          "a": "Fix8 (C++11) is the closest architectural comparison - it also uses object pools and zero-copy techniques. NexusFix differs by leveraging C++17/20/23 standard library features (PMR, std::span, std::expected) instead of custom implementations, and by adding SIMD-accelerated parsing which Fix8 doesn't have. OnixS and other commercial engines are closed-source, so direct architectural comparison isn't possible, but their published latency numbers are in the low-microsecond range, which our benchmarks beat by 5-10x.",
          "a_short": "Fix8 is closest but lacks SIMD parsing. NexusFix uses C++23 standard library instead of custom implementations and beats commercial engines by 5-10x on latency.",
          "keywords": ["compare", "comparison", "fix8", "onixs", "other", "engines", "alternatives"],
          "audio_short": "audio/general_q07_short.mp3",
          "audio_full": "audio/general_q07_full.mp3"
        },
        {
          "q": "What was the biggest lesson learned during development?",
          "a": "That P99 latency matters more than median, and that optimizing for P99 requires fundamentally different techniques than optimizing for throughput. Early versions had excellent median latency but unpredictable P99 due to occasional heap allocations. Switching to PMR pools made the median slightly worse but collapsed the P99/P50 ratio from ~30x to ~1.3x. The trading industry's focus on worst-case latency drove every architectural decision.",
          "a_short": "P99 latency matters more than median. PMR pools collapsed the P99/P50 ratio from ~30x to ~1.3x by eliminating unpredictable heap allocations.",
          "keywords": ["lesson", "learned", "p99", "latency", "insight", "takeaway"],
          "audio_short": "audio/general_q08_short.mp3",
          "audio_full": "audio/general_q08_full.mp3"
        },
        {
          "q": "What would you do differently if starting over?",
          "a": "Start with the structural index from day one. We built a traditional single-pass parser first, then retrofitted the two-stage simdjson-style architecture in TICKET_208. The retrofit was clean because of good abstraction boundaries, but building on the two-stage model from the start would have simplified the intermediate releases. The lesson: invest in the right parsing architecture before optimizing individual operations.",
          "a_short": "Start with the structural index from day one instead of retrofitting it later. Invest in the right parsing architecture before optimizing individual operations.",
          "keywords": ["differently", "regret", "hindsight", "start over", "redo"],
          "audio_short": "audio/general_q09_short.mp3",
          "audio_full": "audio/general_q09_full.mp3"
        },
        {
          "q": "Is NexusFix applicable beyond FIX protocol? Could the techniques apply to other protocols?",
          "a": "The techniques are broadly applicable. SIMD delimiter scanning works for any delimited text protocol (CSV, HTTP headers, log parsing). The structural index applies to any protocol with key-value pairs. PMR pools benefit any allocation-heavy workload. SBE-style binary encoding is protocol-agnostic. The C++23 techniques (constexpr, [[assume]], std::expected) are universal. NexusFix is a case study in applying modern C++ to a specific domain, but the patterns transfer directly.",
          "a_short": "Yes. SIMD scanning works for any delimited protocol, structural indexing for key-value pairs, PMR for any allocation-heavy workload. The patterns transfer directly.",
          "keywords": ["beyond", "other", "protocols", "applicable", "general", "transfer", "csv", "http"],
          "audio_short": "audio/general_q10_short.mp3",
          "audio_full": "audio/general_q10_full.mp3"
        },
        {
          "q": "Why MIT license? Doesn't that let competitors use your work for free?",
          "a": "Open source drives adoption and trust. Trading firms need to audit infrastructure code, and proprietary licensing creates friction. MIT is the most permissive choice - firms can embed NexusFix in proprietary systems without legal concerns. The value is in the engineering quality and the community that forms around it, not in license restrictions. QuickFIX's success under a similar open model validates this approach.",
          "a_short": "MIT maximizes adoption and trust. Trading firms need to audit infrastructure code, and proprietary licensing creates friction. Value is in engineering quality, not license restrictions.",
          "keywords": ["license", "mit", "open source", "proprietary", "free"],
          "audio_short": "audio/general_q11_short.mp3",
          "audio_full": "audio/general_q11_full.mp3"
        },
        {
          "q": "How do you fund ongoing development?",
          "a": "NexusFix is developed by SilverstreamsAI as part of our infrastructure research. The open-source project serves dual purposes: advancing the state of modern C++ in trading infrastructure, and demonstrating engineering capability. The benchmark methodology and optimization techniques developed here feed back into our broader work.",
          "a_short": "Developed by SilverstreamsAI as infrastructure research. The open-source project advances modern C++ in trading and demonstrates engineering capability.",
          "keywords": ["fund", "money", "business", "sponsor", "commercial"],
          "audio_short": "audio/general_q12_short.mp3",
          "audio_full": "audio/general_q12_full.mp3"
        },
        {
          "q": "What's the minimum hardware to run NexusFix effectively?",
          "a": "Any modern x86-64 CPU with AVX2 support (Intel Haswell/2013 or later, AMD Zen/2017 or later). AVX-512 is optional and provides additional speedup on supported hardware. ARM64 with NEON is supported via xsimd portability layer. The parser itself needs minimal memory (under 1KB stack per message). PMR pools and session heaps are configurable from 1MB to 64MB depending on workload. No special hardware (FPGA, kernel bypass NIC) is required, though co-location and CPU pinning are recommended for production latency targets.",
          "a_short": "Any x86-64 with AVX2 (Intel 2013+ or AMD 2017+). ARM64 NEON supported via xsimd. Under 1KB stack per message. No special hardware required.",
          "keywords": ["hardware", "minimum", "requirements", "cpu", "memory", "avx2", "arm"],
          "audio_short": "audio/general_q13_short.mp3",
          "audio_full": "audio/general_q13_full.mp3"
        },
        {
          "q": "How does NexusFix handle errors and malformed messages in production?",
          "a": "Through std::expected<T, Error> on all parsing paths - no exceptions on the hot path. Malformed messages (invalid checksum, missing required tags, truncated data) return typed error values that the caller handles deterministically. The error path is as predictable as the success path: no stack unwinding, no exception allocation, no performance cliff. Error rates and types are countable for monitoring without runtime overhead.",
          "a_short": "All parsing uses std::expected - no exceptions on the hot path. Malformed messages return typed errors handled deterministically with no performance cliff.",
          "keywords": ["error", "malformed", "handling", "expected", "exception", "invalid"],
          "audio_short": "audio/general_q14_short.mp3",
          "audio_full": "audio/general_q14_full.mp3"
        },
        {
          "q": "What testing and quality assurance process does NexusFix follow?",
          "a": "Three layers of testing. Correctness: Catch2 tests covering edge cases, malformed messages, boundary conditions, and property-based fuzzing with random FIX messages. Performance: benchmark regression tests that fail CI if latency exceeds thresholds - we treat performance regressions as bugs. Safety: AddressSanitizer, UndefinedBehaviorSanitizer, and ThreadSanitizer in CI for memory errors, undefined behavior, and data races. Code is compiled with -Werror so all warnings are errors.",
          "a_short": "Three layers: Catch2 correctness tests with fuzzing, benchmark regression tests that fail CI on latency increase, and sanitizers (ASan, UBSan, TSan) for safety.",
          "keywords": ["testing", "quality", "qa", "catch2", "sanitizer", "ci", "benchmark"],
          "audio_short": "audio/general_q15_short.mp3",
          "audio_full": "audio/general_q15_full.mp3"
        },
        {
          "q": "Can NexusFix be used for learning modern C++ performance techniques?",
          "a": "Absolutely, and that's a deliberate secondary goal. Each release tag corresponds to a specific set of C++ techniques with benchmark data showing the impact. The codebase is intentionally small (~2,000 lines) so the techniques aren't buried in infrastructure code. The design documents (TICKET series) explain the reasoning behind each optimization. For C++ engineers studying HFT-style optimization, NexusFix is a self-contained, benchmarked reference implementation.",
          "a_short": "Yes, by design. Each release tag demonstrates specific C++ techniques with benchmarks. The ~2,000-line codebase is a self-contained reference for HFT-style optimization.",
          "keywords": ["learning", "education", "teaching", "study", "reference", "example"],
          "audio_short": "audio/general_q16_short.mp3",
          "audio_full": "audio/general_q16_full.mp3"
        }
      ]
    },
    {
      "id": "v100",
      "label": "v1.0.0 Architecture",
      "questions": [
        {
          "q": "Why did you choose C++23 specifically? Couldn't you achieve similar results with C++17 or C++20?",
          "a": "C++23 gives us std::expected for zero-cost error handling on the hot path (no exception overhead), [[assume]] for compiler optimization hints, and improved constexpr capabilities. C++20 would get us 80% of the way with concepts and coroutines, but C++23 closes critical gaps. For example, std::expected replaces the common pattern of returning error codes through output parameters, which is both error-prone and forces the compiler to assume aliasing.",
          "a_short": "C++23 provides std::expected for zero-cost error handling, [[assume]] for optimizer hints, and improved constexpr. C++20 gets 80% there but C++23 closes critical gaps.",
          "keywords": ["c++23", "c++20", "c++17", "why", "version", "standard", "expected", "assume"],
          "audio_short": "audio/v100_q01_short.mp3",
          "audio_full": "audio/v100_q01_full.mp3"
        },
        {
          "q": "Why FIX 4.4 and not FIX 5.0 or FIXT 1.1?",
          "a": "FIX 4.4 remains the most widely deployed version in production trading systems. Supporting it first maximizes real-world applicability. FIX 5.0 separates transport and application layers (FIXT 1.1), which is architecturally cleaner but adds complexity. Our parser architecture is version-agnostic at the structural level - field scanning works identically across FIX versions - so adding FIX 5.0 is an incremental effort, not a redesign.",
          "a_short": "FIX 4.4 is the most widely deployed version. The parser is version-agnostic at the structural level, so FIX 5.0 support is incremental.",
          "keywords": ["fix44", "fix50", "fixt", "version", "protocol", "why"],
          "audio_short": "audio/v100_q02_short.mp3",
          "audio_full": "audio/v100_q02_full.mp3"
        },
        {
          "q": "What's the design philosophy - library or framework?",
          "a": "Strictly a library. We provide parsing primitives, not a framework that owns your event loop. Users call our parser with a buffer and get structured field access back. This is deliberate: trading firms have vastly different infrastructure (FPGA gateways, kernel bypass, standard sockets) and a framework would force architectural compromises. The library approach also makes benchmarking honest - we measure parsing, not I/O.",
          "a_short": "Strictly a library providing parsing primitives, not a framework. Users call the parser with a buffer and get structured field access back.",
          "keywords": ["library", "framework", "design", "philosophy", "architecture"],
          "audio_short": "audio/v100_q03_short.mp3",
          "audio_full": "audio/v100_q03_full.mp3"
        },
        {
          "q": "How do you handle the trade-off between compile-time and runtime flexibility?",
          "a": "We use a layered approach. The lowest layer uses consteval and constexpr for protocol constants (SOH byte, tag-value separator, checksum validation). The middle layer uses templates for message-type-specific dispatch. The top layer uses runtime polymorphism via std::variant for session management where flexibility matters more than nanoseconds. The hot path is entirely compile-time resolved.",
          "a_short": "Layered approach: consteval for protocol constants, templates for message dispatch, std::variant for session management. Hot path is entirely compile-time resolved.",
          "keywords": ["compile-time", "runtime", "tradeoff", "flexibility", "consteval", "constexpr", "variant"],
          "audio_short": "audio/v100_q04_short.mp3",
          "audio_full": "audio/v100_q04_full.mp3"
        },
        {
          "q": "What's your testing strategy for a performance-critical library?",
          "a": "Three layers. Correctness tests with Catch2 covering edge cases (malformed messages, partial reads, boundary conditions). Performance regression tests that fail CI if latency exceeds thresholds. And property-based tests that generate random FIX messages to stress the parser. We treat a performance regression as a bug, not a tolerable change.",
          "a_short": "Three layers: Catch2 correctness tests, performance regression tests that fail CI, and property-based fuzzing. Performance regressions are treated as bugs.",
          "keywords": ["testing", "strategy", "catch2", "performance", "regression", "fuzzing"],
          "audio_short": "audio/v100_q05_short.mp3",
          "audio_full": "audio/v100_q05_full.mp3"
        },
        {
          "q": "MIT license for a trading infrastructure library - why not keep it proprietary?",
          "a": "Open source drives adoption and trust. Trading firms need to audit infrastructure code, and proprietary licensing creates friction. MIT is the most permissive choice - firms can embed NexusFix in proprietary systems without legal concerns. The value proposition is the engineering quality, not license restrictions.",
          "a_short": "Open source drives adoption and trust. Trading firms need to audit code, and MIT eliminates licensing friction.",
          "keywords": ["license", "mit", "proprietary", "open source", "trading"],
          "audio_short": "audio/v100_q06_short.mp3",
          "audio_full": "audio/v100_q06_full.mp3"
        },
        {
          "q": "What was the hardest architectural decision early on?",
          "a": "Header-only vs compiled library. We chose header-only for the parser because it enables full inlining on the hot path - the compiler can see through every function call and optimize globally. The downside is longer compile times for users, but we mitigate this with a single-header include model. Session management and transport are compiled separately since they're not on the hot path.",
          "a_short": "Header-only vs compiled library. Header-only enables full inlining on the hot path for global optimization, at the cost of longer compile times.",
          "keywords": ["hardest", "decision", "header-only", "compiled", "architecture", "inlining"],
          "audio_short": "audio/v100_q07_short.mp3",
          "audio_full": "audio/v100_q07_full.mp3"
        },
        {
          "q": "How do you ensure your benchmarks are reproducible across different hardware?",
          "a": "We use RDTSC with lfence serialization for cycle-accurate timing, CPU core pinning to avoid migration, and cache warming loops before measurement. We report P50/P99/P999 rather than just mean, and we run enough iterations (100K+) for statistical stability. We also document the exact hardware configuration for each benchmark report.",
          "a_short": "RDTSC with lfence for cycle-accurate timing, CPU pinning, cache warming, 100K+ iterations, and P50/P99/P999 reporting with documented hardware configs.",
          "keywords": ["benchmark", "reproducible", "rdtsc", "timing", "methodology", "hardware"],
          "audio_short": "audio/v100_q08_short.mp3",
          "audio_full": "audio/v100_q08_full.mp3"
        },
        {
          "q": "What's your approach to backward compatibility?",
          "a": "We follow semantic versioning strictly. The 0.x series explicitly reserves the right to break APIs between minor versions as we iterate on the design. v1.0.0 establishes the stable API contract. We prefer clean breaks over deprecation shims - trading systems need predictable performance, and compatibility layers add overhead.",
          "a_short": "Strict semantic versioning. 0.x series allows API breaks. v1.0.0 establishes the stable contract. Clean breaks preferred over compatibility shims.",
          "keywords": ["backward", "compatibility", "semver", "versioning", "api", "stable", "breaking"],
          "audio_short": "audio/v100_q09_short.mp3",
          "audio_full": "audio/v100_q09_full.mp3"
        },
        {
          "q": "How does NexusFix compare architecturally to QuickFIX?",
          "a": "QuickFIX uses a traditional OOP design with virtual dispatch, heap allocation per message, and string-based field storage. NexusFix inverts every one of these: static dispatch via templates/variants, zero-allocation parsing via std::span views, and structural indexing for O(1) field access. The fundamental difference is that QuickFIX treats FIX messages as objects to construct, while NexusFix treats them as byte streams to index.",
          "a_short": "QuickFIX uses virtual dispatch, heap allocation, string storage. NexusFix inverts all three: static dispatch, zero-allocation std::span views, and O(1) structural indexing.",
          "keywords": ["quickfix", "compare", "architecture", "virtual", "dispatch", "heap", "oop"],
          "audio_short": "audio/v100_q10_short.mp3",
          "audio_full": "audio/v100_q10_full.mp3"
        }
      ]
    },
    {
      "id": "v010",
      "label": "v0.1.0 Parser Foundation",
      "questions": [
        {
          "q": "Why a header-only parser? Doesn't that hurt compile times?",
          "a": "For the hot path, inlining is non-negotiable. A compiled library boundary forces the compiler to generate a function call, preventing optimizations like constant propagation and dead code elimination across the call site. In our benchmarks, the header-only parser is 15-20% faster than an equivalent compiled version because the compiler can see the entire parsing pipeline. Compile time impact is ~2 seconds on a modern machine.",
          "a_short": "Inlining is non-negotiable on the hot path. Header-only is 15-20% faster than compiled because the compiler sees the entire parsing pipeline.",
          "keywords": ["header-only", "compile", "inline", "performance", "parser"],
          "audio_short": "audio/v010_q01_short.mp3",
          "audio_full": "audio/v010_q01_full.mp3"
        },
        {
          "q": "How does your zero-copy design work with std::span?",
          "a": "The parser never copies input data. It takes a std::span<const char> view of the network buffer and returns std::string_view references into that same buffer for field values. The caller owns the buffer lifetime. This eliminates all allocation and copying during parsing. The key insight is that FIX messages are text protocols where field values are substrings of the original message - there's no transformation needed, just indexing.",
          "a_short": "Parser takes std::span view of the buffer and returns string_view references into it. No copies, no allocations - just indexing into the original message.",
          "keywords": ["zero-copy", "span", "string_view", "memory", "allocation", "copy"],
          "audio_short": "audio/v010_q02_short.mp3",
          "audio_full": "audio/v010_q02_full.mp3"
        },
        {
          "q": "What does consteval buy you for field offsets?",
          "a": "consteval guarantees compile-time evaluation, unlike constexpr which merely permits it. For protocol constants like tag numbers, SOH byte values, and checksum seeds, consteval ensures zero runtime cost. The compiler replaces function calls with literal values in the generated assembly. We verified this with objdump - no runtime computation for any protocol constant.",
          "a_short": "consteval guarantees compile-time evaluation for protocol constants. The compiler replaces function calls with literal values - verified with objdump.",
          "keywords": ["consteval", "constexpr", "compile-time", "field", "offset", "constant"],
          "audio_short": "audio/v010_q03_short.mp3",
          "audio_full": "audio/v010_q03_full.mp3"
        },
        {
          "q": "How do you handle partial messages from the network?",
          "a": "The parser is designed for complete messages. The transport layer handles framing - buffering bytes until a complete FIX message is detected (scanning for BeginString and Checksum tags). This separation keeps the parser simple and branch-free. The framing logic is not on the latency-critical path since it runs as data arrives from the network.",
          "a_short": "Parser handles complete messages only. Transport layer handles framing by buffering until a complete message is detected, keeping the parser branch-free.",
          "keywords": ["partial", "message", "network", "framing", "buffer", "incomplete"],
          "audio_short": "audio/v010_q04_short.mp3",
          "audio_full": "audio/v010_q04_full.mp3"
        },
        {
          "q": "What's the memory layout of a parsed message?",
          "a": "A ParsedMessage is a lightweight view object containing a span to the original buffer plus an array of field entries (tag number, value offset, value length). No strings are allocated. The field array is stack-allocated with a compile-time maximum (64 fields covers all standard FIX message types). Total overhead per parsed message is under 1KB on the stack.",
          "a_short": "A lightweight view with a span to the buffer plus a stack-allocated field array. Under 1KB total with no heap allocation.",
          "keywords": ["memory", "layout", "parsed", "message", "stack", "struct"],
          "audio_short": "audio/v010_q05_short.mp3",
          "audio_full": "audio/v010_q05_full.mp3"
        },
        {
          "q": "How do you validate FIX checksums without branching?",
          "a": "We compute the checksum as a running sum during the initial scan pass, using arithmetic modulo 256. The comparison with the declared checksum is a single equality check at the end. No conditional branches during accumulation. The compiler auto-vectorizes the sum loop on modern hardware.",
          "a_short": "Running sum during the scan pass with modulo 256. Single equality check at the end. No branches during accumulation, auto-vectorized by the compiler.",
          "keywords": ["checksum", "validate", "branch", "branchless", "modulo"],
          "audio_short": "audio/v010_q06_short.mp3",
          "audio_full": "audio/v010_q06_full.mp3"
        },
        {
          "q": "Why Catch2 for testing instead of Google Test?",
          "a": "Catch2's BDD-style sections and generators are excellent for property-based testing of parsers. We can write GENERATE(take(100, random_fix_message())) to fuzz the parser with random inputs. Catch2 also has better support for benchmarking integration via BENCHMARK macros, which we use for regression testing.",
          "a_short": "Catch2 offers BDD-style sections, generators for property-based fuzzing, and built-in BENCHMARK macros for performance regression testing.",
          "keywords": ["catch2", "gtest", "google test", "testing", "framework", "bdd"],
          "audio_short": "audio/v010_q07_short.mp3",
          "audio_full": "audio/v010_q07_full.mp3"
        },
        {
          "q": "What happens when a FIX message has more fields than your static array?",
          "a": "We use a compile-time constant MaxFields = 64 which covers all standard FIX 4.4 message types (ExecutionReport, the largest, has ~50 fields). For custom messages exceeding this, the parser returns an error via std::expected rather than silently truncating. The limit is a template parameter, so users can increase it at the cost of stack space.",
          "a_short": "MaxFields = 64 covers all standard FIX 4.4 types. Exceeding it returns an error via std::expected. The limit is a configurable template parameter.",
          "keywords": ["maxfields", "overflow", "limit", "static", "array", "fields"],
          "audio_short": "audio/v010_q08_short.mp3",
          "audio_full": "audio/v010_q08_full.mp3"
        },
        {
          "q": "How do you handle different FIX field types (int, float, string, timestamp)?",
          "a": "We don't - at the parser level. The parser returns raw std::string_view values for every field. Type conversion is the caller's responsibility, and we provide constexpr conversion utilities (e.g., fix_to_int, fix_to_price) that compile to minimal instructions. This keeps the parser focused on one job: structural decomposition.",
          "a_short": "Parser returns raw string_view for all fields. Separate constexpr conversion utilities handle type conversion. Parser focuses only on structural decomposition.",
          "keywords": ["types", "int", "float", "string", "timestamp", "conversion", "field"],
          "audio_short": "audio/v010_q09_short.mp3",
          "audio_full": "audio/v010_q09_full.mp3"
        },
        {
          "q": "Is this parser suitable for FPGA offloading?",
          "a": "The algorithm is, but the implementation isn't - it uses C++ features that don't map to HDL. However, the two-stage architecture (structural index + field access) maps naturally to FPGA pipelines. Stage 1 (byte scanning) is embarrassingly parallel and ideal for FPGA fabric. Stage 2 (field lookup) is a simple table lookup. We've had conversations with teams exploring this path.",
          "a_short": "The two-stage architecture maps naturally to FPGA pipelines. Stage 1 (byte scanning) is ideal for FPGA fabric, Stage 2 is a simple table lookup.",
          "keywords": ["fpga", "hardware", "offload", "hdl", "pipeline", "acceleration"],
          "audio_short": "audio/v010_q10_short.mp3",
          "audio_full": "audio/v010_q10_full.mp3"
        }
      ]
    },
    {
      "id": "v011",
      "label": "v0.1.1 PMR Pools",
      "questions": [
        {
          "q": "PMR shows worse median but 14x better P99 - how do you explain that to a team evaluating adoption?",
          "a": "The median difference comes from PMR's pool initialization overhead - the first few allocations set up internal bookkeeping. But P99 is what matters in trading. A single 780ns heap allocation spike during order execution can miss a fill. PMR's monotonic allocator does pure pointer arithmetic - no free lists, no coalescing, no syscalls. The latency distribution is nearly flat, which is exactly what you want for deterministic execution.",
          "a_short": "P99 matters more than median in trading. PMR's monotonic allocator does pure pointer arithmetic with a nearly flat latency distribution - no allocation spikes.",
          "keywords": ["pmr", "p99", "median", "latency", "allocation", "deterministic", "pool"],
          "audio_short": "audio/v011_q01_short.mp3",
          "audio_full": "audio/v011_q01_full.mp3"
        },
        {
          "q": "Why monotonic_buffer_resource specifically, rather than pool_resource?",
          "a": "Monotonic is ideal for request-scoped allocation patterns, which is exactly how FIX message processing works: allocate during message handling, deallocate everything at the end of the message lifecycle. pool_resource maintains free lists for reuse, which adds overhead we don't need. We reset the monotonic buffer between messages, getting O(1) deallocation (just reset the pointer).",
          "a_short": "Monotonic fits FIX message lifecycle: allocate during handling, reset everything at the end. O(1) deallocation by just resetting the pointer.",
          "keywords": ["monotonic", "pool_resource", "buffer", "allocation", "reset", "lifecycle"],
          "audio_short": "audio/v011_q02_short.mp3",
          "audio_full": "audio/v011_q02_full.mp3"
        },
        {
          "q": "What was the C++11 vs C++23 case study about?",
          "a": "We implemented the same FIX message store in C++11 style (custom allocator, manual pool management, placement new) and C++23 style (std::pmr::monotonic_buffer_resource). The C++11 version was ~200 lines of careful code. The C++23 version was ~20 lines using standard library PMR. Performance was identical. The case study demonstrates that the C++ standards committee successfully standardized patterns that were previously expert-only knowledge.",
          "a_short": "Same FIX message store: C++11 needed ~200 lines of custom allocator code, C++23 needed ~20 lines with standard PMR. Identical performance.",
          "keywords": ["c++11", "c++23", "case study", "comparison", "pmr", "allocator"],
          "audio_short": "audio/v011_q03_short.mp3",
          "audio_full": "audio/v011_q03_full.mp3"
        },
        {
          "q": "How do you handle pool exhaustion?",
          "a": "We size the pool based on the maximum expected message burst (configurable at construction). If the pool exhausts, the upstream allocator (new_delete_resource by default) takes over, which means a heap allocation. We monitor pool utilization and log warnings at 80% capacity. In production, pool exhaustion should never happen - it means the sizing calculation was wrong.",
          "a_short": "Pool sized for max burst. If exhausted, upstream allocator takes over. We log warnings at 80% capacity. Pool exhaustion in production means sizing was wrong.",
          "keywords": ["exhaustion", "overflow", "pool", "size", "capacity", "fallback"],
          "audio_short": "audio/v011_q04_short.mp3",
          "audio_full": "audio/v011_q04_full.mp3"
        },
        {
          "q": "The cross-platform transport layer - how do you abstract platform differences without virtual dispatch?",
          "a": "We use #ifdef platform detection at the header level with a common API surface. The compiler sees only the platform-specific implementation for the target build. No runtime dispatch, no vtables. The transport layer isn't on the hot path (network I/O latency dwarfs any dispatch overhead), but we still avoid virtual functions as a matter of principle - it prevents accidental use in latency-sensitive contexts.",
          "a_short": "Compile-time #ifdef platform detection with common API. No vtables. Transport isn't on the hot path but we avoid virtual dispatch on principle.",
          "keywords": ["cross-platform", "transport", "ifdef", "virtual", "dispatch", "windows", "linux"],
          "audio_short": "audio/v011_q05_short.mp3",
          "audio_full": "audio/v011_q05_full.mp3"
        },
        {
          "q": "Why did Windows Winsock initialization cause crashes?",
          "a": "WSAStartup must be called before any socket operations on Windows. On Linux, sockets work immediately. The fix was straightforward - RAII initialization in the transport layer constructor - but it's a classic cross-platform gotcha. We added it to our CI matrix to catch platform-specific issues early.",
          "a_short": "WSAStartup must be called before socket operations on Windows. Fixed with RAII initialization in the transport constructor.",
          "keywords": ["windows", "winsock", "wsastartup", "crash", "socket", "cross-platform"],
          "audio_short": "audio/v011_q06_short.mp3",
          "audio_full": "audio/v011_q06_full.mp3"
        },
        {
          "q": "How do you decide what goes in the message store vs what stays as a view?",
          "a": "The parser produces views (zero-copy). The message store takes ownership when persistence is needed (e.g., for gap fill, replay, or audit). The store uses PMR for allocation. The key insight is that most messages in a trading system are processed and discarded - only a fraction need storage. So we optimize the common case (view) and make the uncommon case (store) efficient but not minimal.",
          "a_short": "Parser produces zero-copy views. Store takes ownership only when persistence is needed. Most messages are processed and discarded, so views are the optimized path.",
          "keywords": ["store", "view", "persistence", "ownership", "copy", "discard"],
          "audio_short": "audio/v011_q07_short.mp3",
          "audio_full": "audio/v011_q07_full.mp3"
        },
        {
          "q": "What's the PMR memory overhead compared to raw allocation?",
          "a": "Approximately 0.1% for metadata. The monotonic buffer resource has no per-allocation overhead - it's literally a pointer bump. The only overhead is the initial pool allocation itself (a single mmap/VirtualAlloc call). Compared to heap allocation which has 16-32 bytes of per-allocation metadata, PMR is dramatically more memory-efficient for many small allocations.",
          "a_short": "~0.1% overhead. Monotonic buffer is a pointer bump with no per-allocation metadata, vs heap's 16-32 bytes per allocation.",
          "keywords": ["overhead", "memory", "pmr", "metadata", "efficiency", "pointer bump"],
          "audio_short": "audio/v011_q08_short.mp3",
          "audio_full": "audio/v011_q08_full.mp3"
        },
        {
          "q": "Can PMR pools be shared across threads?",
          "a": "monotonic_buffer_resource is not thread-safe by default, and that's a feature. Thread-safe allocators require atomic operations or locks, which destroy the latency benefits. Our design gives each session (thread) its own pool, eliminating contention entirely. For cross-thread communication, we use lock-free queues that move data by value or by buffer ownership transfer.",
          "a_short": "No, by design. Thread-safe allocators need atomics/locks that destroy latency. Each session gets its own pool, eliminating contention entirely.",
          "keywords": ["thread", "shared", "concurrent", "pool", "contention", "lock-free"],
          "audio_short": "audio/v011_q09_short.mp3",
          "audio_full": "audio/v011_q09_full.mp3"
        },
        {
          "q": "How do you benchmark PMR fairly? Isn't warm-up a factor?",
          "a": "Yes. We warm up both paths identically - 10,000 iterations before measurement. We also benchmark both cold (first allocation from fresh pool) and warm (steady-state) scenarios. The PMR advantage grows with message volume because heap fragmentation increases over time while PMR just resets. We report both short-burst and sustained-load results.",
          "a_short": "10,000 warm-up iterations for both paths. Benchmark cold and warm scenarios. PMR advantage grows over time as heap fragments while PMR just resets.",
          "keywords": ["benchmark", "warmup", "fair", "pmr", "cold", "warm", "fragmentation"],
          "audio_short": "audio/v011_q10_short.mp3",
          "audio_full": "audio/v011_q10_full.mp3"
        },
        {
          "q": "What's the release workflow you added?",
          "a": "GitHub Actions workflow with CMake build matrix (GCC, Clang, MSVC across Linux/macOS/Windows), automated testing, and artifact packaging. The key is that benchmark results are CI artifacts, not just local measurements. This prevents works on my machine benchmark regressions.",
          "a_short": "GitHub Actions with CMake build matrix across GCC/Clang/MSVC on Linux/macOS/Windows. Benchmark results are CI artifacts to prevent local-only regressions.",
          "keywords": ["release", "workflow", "ci", "github actions", "cmake", "build matrix"],
          "audio_short": "audio/v011_q11_short.mp3",
          "audio_full": "audio/v011_q11_full.mp3"
        }
      ]
    },
    {
      "id": "v012",
      "label": "v0.1.2 CMake",
      "questions": [
        {
          "q": "Why FetchContent over Conan or vcpkg for dependency management?",
          "a": "FetchContent is built into CMake, requires no external tools, and works identically across all platforms. For a library with minimal dependencies (Catch2 for testing), the complexity of a full package manager isn't justified. FetchContent also integrates cleanly with our CI pipeline - no package manager installation step required.",
          "a_short": "FetchContent is built into CMake with no external tools needed. For minimal dependencies like Catch2, a full package manager is unjustified complexity.",
          "keywords": ["fetchcontent", "conan", "vcpkg", "dependency", "cmake", "package manager"],
          "audio_short": "audio/v012_q01_short.mp3",
          "audio_full": "audio/v012_q01_full.mp3"
        },
        {
          "q": "What was the install export error you fixed?",
          "a": "When using FetchContent to pull dependencies and then exporting install targets, CMake would try to export the FetchContent targets too, causing errors. The fix was to use EXCLUDE_FROM_ALL in the FetchContent declaration and carefully scope our install targets. This is a common CMake pitfall that affects any project using both FetchContent and install(EXPORT).",
          "a_short": "CMake tried to export FetchContent targets during install. Fixed with EXCLUDE_FROM_ALL in FetchContent declaration and scoped install targets.",
          "keywords": ["install", "export", "cmake", "fetchcontent", "error", "targets"],
          "audio_short": "audio/v012_q02_short.mp3",
          "audio_full": "audio/v012_q02_full.mp3"
        },
        {
          "q": "How do you structure CMake targets for a header-only library?",
          "a": "We use an INTERFACE library target. Users do find_package(NexusFix) and target_link_libraries(myapp NexusFix::NexusFix). The INTERFACE target propagates include paths, compile features (C++23), and any necessary compile definitions. No binary linking required. This is the modern CMake idiom for header-only libraries.",
          "a_short": "INTERFACE library target propagating include paths, C++23 compile features, and definitions. Users just find_package and target_link_libraries.",
          "keywords": ["cmake", "interface", "target", "header-only", "find_package", "link"],
          "audio_short": "audio/v012_q03_short.mp3",
          "audio_full": "audio/v012_q03_full.mp3"
        },
        {
          "q": "What compile features do you propagate through your CMake target?",
          "a": "cxx_std_23 as a minimum, plus we detect and propagate SIMD availability flags (-mavx2, -mavx512f) based on the target architecture. Users can override these, but the defaults enable full optimization on x86-64. We use generator expressions to handle compiler-specific flags (MSVC vs GCC/Clang).",
          "a_short": "cxx_std_23 minimum plus auto-detected SIMD flags (-mavx2, -mavx512f). Generator expressions handle compiler-specific differences.",
          "keywords": ["compile", "features", "c++23", "simd", "avx2", "flags", "cmake"],
          "audio_short": "audio/v012_q04_short.mp3",
          "audio_full": "audio/v012_q04_full.mp3"
        },
        {
          "q": "How do consumers control whether SIMD is enabled?",
          "a": "Through CMake options: NFX_ENABLE_AVX2, NFX_ENABLE_AVX512. These default to auto-detection via check_cxx_compiler_flag. Users can force-disable SIMD for portability (e.g., building for ARM or older x86). The parser falls back to scalar code gracefully - same API, different performance.",
          "a_short": "CMake options NFX_ENABLE_AVX2 and NFX_ENABLE_AVX512 with auto-detection defaults. Users can force-disable for portability with graceful scalar fallback.",
          "keywords": ["simd", "enable", "disable", "cmake", "option", "avx2", "avx512", "scalar"],
          "audio_short": "audio/v012_q05_short.mp3",
          "audio_full": "audio/v012_q05_full.mp3"
        },
        {
          "q": "Do you support CMake presets?",
          "a": "Yes, we provide CMakePresets.json with configurations for Debug, Release, RelWithDebInfo, and benchmark-specific presets that enable LTO and native architecture optimization. This standardizes build configurations across developers and CI.",
          "a_short": "Yes. CMakePresets.json provides Debug, Release, RelWithDebInfo, and benchmark presets with LTO and native optimization.",
          "keywords": ["presets", "cmake", "debug", "release", "lto", "configuration"],
          "audio_short": "audio/v012_q06_short.mp3",
          "audio_full": "audio/v012_q06_full.mp3"
        },
        {
          "q": "What's the minimum CMake version you require?",
          "a": "CMake 3.21 for FetchContent improvements and C++23 support in target_compile_features. This is recent enough to have good C++23 support but old enough to be available in most CI environments and package managers.",
          "a_short": "CMake 3.21 for FetchContent improvements and C++23 support. Recent enough for features, old enough for CI availability.",
          "keywords": ["cmake", "version", "minimum", "3.21", "requirement"],
          "audio_short": "audio/v012_q07_short.mp3",
          "audio_full": "audio/v012_q07_full.mp3"
        },
        {
          "q": "How do you handle the FetchContent download during offline builds?",
          "a": "FetchContent supports FETCHCONTENT_FULLY_DISCONNECTED mode, which uses previously downloaded sources. We also document how to use pre-installed system packages as alternatives. For CI, we cache the FetchContent downloads between runs.",
          "a_short": "FETCHCONTENT_FULLY_DISCONNECTED mode uses cached sources. CI caches downloads between runs. System packages documented as alternatives.",
          "keywords": ["offline", "fetchcontent", "cache", "download", "disconnected"],
          "audio_short": "audio/v012_q08_short.mp3",
          "audio_full": "audio/v012_q08_full.mp3"
        },
        {
          "q": "What's your approach to version compatibility in CMake exports?",
          "a": "We generate a NexusFIXConfigVersion.cmake with SameMajorVersion compatibility. During the 0.x series, this means only exact minor version matches are compatible, enforcing our API stability guarantee (or lack thereof pre-1.0). Post-1.0, any 1.x version is compatible.",
          "a_short": "SameMajorVersion compatibility in CMake config. Pre-1.0: exact minor version match required. Post-1.0: any 1.x is compatible.",
          "keywords": ["version", "compatibility", "cmake", "export", "config", "semver"],
          "audio_short": "audio/v012_q09_short.mp3",
          "audio_full": "audio/v012_q09_full.mp3"
        },
        {
          "q": "Any lessons learned about CMake and header-only libraries?",
          "a": "The biggest lesson: always test your install targets in a clean environment. It's easy to have a working build tree but broken install tree because of missing target exports or incorrect include paths. We have a CI step that does cmake --install followed by a consumer project build to catch this.",
          "a_short": "Always test install targets in a clean environment. We have a CI step that does cmake --install then builds a consumer project to verify.",
          "keywords": ["cmake", "lesson", "install", "header-only", "clean", "test"],
          "audio_short": "audio/v012_q10_short.mp3",
          "audio_full": "audio/v012_q10_full.mp3"
        }
      ]
    },
    {
      "id": "v014",
      "label": "v0.1.4 CI/CD",
      "questions": [
        {
          "q": "Why is there no v0.1.3?",
          "a": "We had a version bump that was superseded before release. Rather than rewrite history, we skipped the number. Semantic versioning doesn't require contiguous numbers - each version just needs to be greater than the previous one. This is a common practice in production software.",
          "a_short": "Version bump was superseded before release. Semantic versioning doesn't require contiguous numbers - just greater than the previous one.",
          "keywords": ["v013", "skip", "version", "missing", "number"],
          "audio_short": "audio/v014_q01_short.mp3",
          "audio_full": "audio/v014_q01_full.mp3"
        },
        {
          "q": "How do you manage version numbers across CMake, code, and git tags?",
          "a": "Single source of truth in CMakeLists.txt via the project(VERSION) command. The version propagates to C++ code through a generated header (version.hpp) using configure_file. Git tags are created manually at release time and must match. CI validates that the tag version matches the CMake version.",
          "a_short": "Single source of truth in CMakeLists.txt project(VERSION). Propagates to code via configure_file. CI validates tag matches CMake version.",
          "keywords": ["version", "cmake", "git", "tag", "management", "single source"],
          "audio_short": "audio/v014_q02_short.mp3",
          "audio_full": "audio/v014_q02_full.mp3"
        },
        {
          "q": "What does your CI pipeline look like?",
          "a": "GitHub Actions with a build matrix: GCC 13+, Clang 17+, MSVC 2022 across Linux, macOS, Windows. Each configuration runs the full test suite. Release builds additionally run benchmarks and archive results. We also have a sanitizer job (ASan, UBSan) that catches undefined behavior.",
          "a_short": "GitHub Actions with GCC 13+, Clang 17+, MSVC 2022 across Linux/macOS/Windows. Full test suite plus benchmark archival and sanitizer jobs.",
          "keywords": ["ci", "pipeline", "github actions", "build", "matrix", "gcc", "clang", "msvc"],
          "audio_short": "audio/v014_q03_short.mp3",
          "audio_full": "audio/v014_q03_full.mp3"
        },
        {
          "q": "How do you prevent performance regressions in CI?",
          "a": "Benchmark results are compared against baseline thresholds stored in the repository. If any metric regresses beyond a tolerance (10% for latency, 5% for throughput), the CI job fails. This catches regressions before merge. The thresholds are generous enough to account for CI hardware variability.",
          "a_short": "Baseline thresholds in the repo. CI fails if latency regresses >10% or throughput >5%. Catches regressions before merge.",
          "keywords": ["regression", "performance", "benchmark", "ci", "threshold", "baseline"],
          "audio_short": "audio/v014_q04_short.mp3",
          "audio_full": "audio/v014_q04_full.mp3"
        },
        {
          "q": "What sanitizers do you use?",
          "a": "AddressSanitizer (ASan) for memory errors, UndefinedBehaviorSanitizer (UBSan) for undefined behavior, and ThreadSanitizer (TSan) for data races in the lock-free queue tests. We don't use MemorySanitizer because it requires an instrumented libc++, which is too fragile for CI.",
          "a_short": "ASan for memory errors, UBSan for undefined behavior, TSan for data races. No MSan due to instrumented libc++ fragility in CI.",
          "keywords": ["sanitizer", "asan", "ubsan", "tsan", "memory", "undefined behavior", "data race"],
          "audio_short": "audio/v014_q05_short.mp3",
          "audio_full": "audio/v014_q05_full.mp3"
        },
        {
          "q": "How do you handle SIMD code in CI where hardware varies?",
          "a": "CI runners have AVX2 support (standard on modern x86-64). For AVX-512, we have a separate optional CI job that runs on capable hardware. The build system detects available instruction sets and conditionally compiles SIMD paths. Tests include runtime feature detection that skips SIMD tests on unsupported hardware.",
          "a_short": "CI runners have AVX2. AVX-512 has a separate optional job. Runtime feature detection skips SIMD tests on unsupported hardware.",
          "keywords": ["simd", "ci", "avx2", "avx512", "hardware", "detection", "runtime"],
          "audio_short": "audio/v014_q06_short.mp3",
          "audio_full": "audio/v014_q06_full.mp3"
        },
        {
          "q": "What's your branching strategy?",
          "a": "Trunk-based development on main. Feature branches are short-lived (1-2 days max). No long-running release branches - tags mark releases directly on main. This minimizes merge conflicts and keeps the development velocity high.",
          "a_short": "Trunk-based on main with short-lived feature branches (1-2 days). Tags mark releases directly on main. No long-running release branches.",
          "keywords": ["branching", "git", "trunk", "feature branch", "strategy", "release"],
          "audio_short": "audio/v014_q07_short.mp3",
          "audio_full": "audio/v014_q07_full.mp3"
        },
        {
          "q": "How do you handle flaky tests?",
          "a": "We don't tolerate them. A flaky test indicates either a real race condition or a test design problem. We've had two cases of flaky benchmarks due to CI CPU frequency scaling - the fix was to use relative comparisons (before/after ratio) rather than absolute thresholds. Every flaky test we've encountered was a real bug.",
          "a_short": "Zero tolerance. Every flaky test was a real bug. Fixed CI benchmark flakiness by using relative comparisons instead of absolute thresholds.",
          "keywords": ["flaky", "tests", "intermittent", "race condition", "reliability"],
          "audio_short": "audio/v014_q08_short.mp3",
          "audio_full": "audio/v014_q08_full.mp3"
        },
        {
          "q": "Do you use code coverage?",
          "a": "Yes, via gcov/llvm-cov with a minimum threshold of 90% for parser code. Coverage is informative, not a gate - we don't block merges on coverage percentage. The parser is well-tested by necessity (correctness matters), but some error paths in transport code are harder to exercise meaningfully.",
          "a_short": "Yes, gcov/llvm-cov with 90% minimum for parser code. Informative, not a merge gate. Some transport error paths are hard to exercise meaningfully.",
          "keywords": ["coverage", "gcov", "llvm-cov", "testing", "percentage", "threshold"],
          "audio_short": "audio/v014_q09_short.mp3",
          "audio_full": "audio/v014_q09_full.mp3"
        },
        {
          "q": "How do you test across compiler versions?",
          "a": "Our CI matrix includes the latest two major versions of each compiler. C++23 support varies across compilers, so we use feature detection macros (NFX_HAS_EXPECTED, NFX_HAS_ASSUME) to provide fallbacks. This is documented in TICKET_026 (compiler feature macros).",
          "a_short": "CI matrix with latest two major versions of GCC, Clang, MSVC. Feature detection macros provide fallbacks for varying C++23 support.",
          "keywords": ["compiler", "version", "gcc", "clang", "msvc", "feature detection", "macro"],
          "audio_short": "audio/v014_q10_short.mp3",
          "audio_full": "audio/v014_q10_full.mp3"
        }
      ]
    },
    {
      "id": "v015",
      "label": "v0.1.5 Object Construction",
      "questions": [
        {
          "q": "What does std::construct_at give you over placement new?",
          "a": "std::construct_at is constexpr-compatible, which means it works in constant evaluation contexts. Placement new cannot be used in constexpr functions. For our use case, this means we can construct objects in pre-allocated buffers at compile time for protocol constants. It also provides better type safety - the compiler checks the constructed type matches the pointer type.",
          "a_short": "constexpr-compatible unlike placement new. Enables compile-time construction in pre-allocated buffers for protocol constants, with better type safety.",
          "keywords": ["construct_at", "placement new", "constexpr", "construction", "compile-time"],
          "audio_short": "audio/v015_q01_short.mp3",
          "audio_full": "audio/v015_q01_full.mp3"
        },
        {
          "q": "Where specifically do you use std::construct_at?",
          "a": "In the buffer pool for constructing message objects in pre-allocated aligned storage, and in the PMR-backed message store for in-place construction. Previously we used placement new, which worked but prevented those code paths from being constexpr-evaluable. The migration was mechanical but enabled further compile-time optimization.",
          "a_short": "Buffer pool for message objects in aligned storage, and PMR message store for in-place construction. Migration from placement new enabled constexpr evaluation.",
          "keywords": ["construct_at", "buffer pool", "pmr", "store", "aligned", "where"],
          "audio_short": "audio/v015_q02_short.mp3",
          "audio_full": "audio/v015_q02_full.mp3"
        },
        {
          "q": "Does this have any performance impact?",
          "a": "Zero. std::construct_at compiles to identical machine code as placement new for runtime construction. The benefit is purely at compile time - enabling constexpr evaluation of construction patterns. Any performance improvement here is really about unlocking further compiler optimizations through constexpr propagation.",
          "a_short": "Zero runtime impact. Identical machine code to placement new. The benefit is enabling constexpr evaluation which unlocks further compiler optimizations.",
          "keywords": ["performance", "impact", "construct_at", "overhead", "cost", "zero"],
          "audio_short": "audio/v015_q03_short.mp3",
          "audio_full": "audio/v015_q03_full.mp3"
        },
        {
          "q": "Why is constexpr construction useful for a network protocol parser?",
          "a": "Protocol constants (tag-to-type mappings, validation tables, default field values) can be constructed at compile time and placed in read-only memory. This eliminates initialization code at startup and keeps constant data in the CPU's instruction cache. For frequently accessed lookup tables, this can eliminate L1 data cache misses.",
          "a_short": "Protocol constants placed in read-only memory at compile time. No startup initialization, better instruction cache utilization, fewer L1 data cache misses.",
          "keywords": ["constexpr", "construction", "protocol", "constants", "cache", "readonly"],
          "audio_short": "audio/v015_q04_short.mp3",
          "audio_full": "audio/v015_q04_full.mp3"
        },
        {
          "q": "What was the migration effort from placement new to construct_at?",
          "a": "Small - about 15 call sites. The pattern is mechanical: new (ptr) T(args...) becomes std::construct_at(ptr, args...). The real work was ensuring the enclosing functions could be marked constexpr after the migration, which required reviewing their entire call chain.",
          "a_short": "~15 call sites, mechanical pattern replacement. Real work was ensuring enclosing functions could be marked constexpr after migration.",
          "keywords": ["migration", "effort", "placement new", "construct_at", "refactor"],
          "audio_short": "audio/v015_q05_short.mp3",
          "audio_full": "audio/v015_q05_full.mp3"
        },
        {
          "q": "How does std::construct_at interact with your PMR pools?",
          "a": "PMR allocates raw memory, and std::construct_at constructs objects in it. They're complementary: PMR handles allocation (where the bytes come from), construct_at handles initialization (what's written to those bytes). Together they give us full control over both allocation strategy and construction timing.",
          "a_short": "Complementary: PMR handles allocation (where bytes come from), construct_at handles initialization (what's written). Full control over both.",
          "keywords": ["construct_at", "pmr", "allocation", "initialization", "complementary"],
          "audio_short": "audio/v015_q06_short.mp3",
          "audio_full": "audio/v015_q06_full.mp3"
        },
        {
          "q": "Is std::destroy_at similarly useful?",
          "a": "Yes, and we use it paired with std::construct_at for symmetric object lifecycle management. In the buffer pool, objects are explicitly destroyed before the buffer is reused. std::destroy_at is also constexpr, maintaining the compile-time evaluability chain.",
          "a_short": "Yes, paired with construct_at for symmetric lifecycle management. Also constexpr, maintaining the compile-time evaluability chain.",
          "keywords": ["destroy_at", "lifecycle", "destruction", "symmetric", "constexpr"],
          "audio_short": "audio/v015_q07_short.mp3",
          "audio_full": "audio/v015_q07_full.mp3"
        },
        {
          "q": "What about std::uninitialized_construct_using_allocator?",
          "a": "We considered it but it's overly complex for our needs. Our allocation and construction are intentionally separated - PMR for allocation, construct_at for construction. The combined function adds abstraction without benefit when you want explicit control over each step.",
          "a_short": "Too complex for our needs. We intentionally separate allocation (PMR) from construction (construct_at) for explicit control over each step.",
          "keywords": ["uninitialized_construct", "allocator", "complex", "alternative"],
          "audio_short": "audio/v015_q08_short.mp3",
          "audio_full": "audio/v015_q08_full.mp3"
        },
        {
          "q": "How do you ensure constructed objects are properly aligned in your pools?",
          "a": "alignas on the pool buffer plus std::assume_aligned hints to the compiler. The pool allocator returns pointers that are always aligned to alignof(std::max_align_t) at minimum, and we can request stricter alignment (e.g., 32 bytes for SIMD) through the allocator interface.",
          "a_short": "alignas on pool buffer plus std::assume_aligned hints. Minimum alignof(max_align_t), with optional stricter alignment (32 bytes for SIMD).",
          "keywords": ["alignment", "alignas", "assume_aligned", "simd", "pool", "memory"],
          "audio_short": "audio/v015_q09_short.mp3",
          "audio_full": "audio/v015_q09_full.mp3"
        },
        {
          "q": "Any compiler differences in std::construct_at support?",
          "a": "GCC 12+, Clang 15+, and MSVC 2022 all support it fully. It's a C++20 feature so it's well-established by now. The only edge case is that some compilers are more aggressive about constexpr evaluation than others, so we verify with static_assert that our compile-time constructions actually happen at compile time.",
          "a_short": "GCC 12+, Clang 15+, MSVC 2022. Well-established C++20 feature. We verify compile-time evaluation with static_assert.",
          "keywords": ["compiler", "support", "gcc", "clang", "msvc", "c++20", "construct_at"],
          "audio_short": "audio/v015_q10_short.mp3",
          "audio_full": "audio/v015_q10_full.mp3"
        }
      ]
    },
    {
      "id": "v016",
      "label": "v0.1.6 MPSC Queue",
      "questions": [
        {
          "q": "Why did you implement your own MPSC queue instead of using an existing library?",
          "a": "Existing libraries (Boost.Lockfree, folly::MPMCQueue) bring large dependency trees and are designed for general-purpose use. Our queue is specialized for FIX message passing: fixed-size entries, known producer count, and cache-line-padded slots. This specialization gives us 30ns enqueue latency vs 60-100ns for general-purpose queues. The implementation is also under 200 lines, making it auditable.",
          "a_short": "Specialized for FIX: fixed-size entries, cache-line-padded slots, 30ns enqueue vs 60-100ns for general-purpose queues. Under 200 lines, fully auditable.",
          "keywords": ["mpsc", "queue", "lock-free", "boost", "folly", "custom", "why"],
          "audio_short": "audio/v016_q01_short.mp3",
          "audio_full": "audio/v016_q01_full.mp3"
        },
        {
          "q": "Explain the Disruptor pattern and what you took from it.",
          "a": "The LMAX Disruptor is a lock-free ring buffer that eliminates false sharing through cache-line padding and uses sequence counters instead of head/tail pointers. We adopted three key ideas: (1) each slot is padded to a full cache line, preventing false sharing between adjacent slots; (2) sequence numbers embedded in each slot for publication detection; (3) monotonically increasing sequence counters that never wrap (we use 64-bit counters). We did not adopt the full Disruptor API (barriers, event handlers) which is Java-specific.",
          "a_short": "Adopted three LMAX Disruptor ideas: cache-line padding per slot, embedded sequence numbers for publication detection, and 64-bit monotonic counters.",
          "keywords": ["disruptor", "lmax", "ring buffer", "cache line", "sequence", "pattern"],
          "audio_short": "audio/v016_q02_short.mp3",
          "audio_full": "audio/v016_q02_full.mp3"
        },
        {
          "q": "SPSC gets 34.74 M/s but MPSC drops to 16.10 M/s with 4 producers - is that expected?",
          "a": "Yes. MPSC requires a CAS (compare-and-swap) loop for the producer claim, while SPSC uses a simple store. With 4 producers, CAS contention scales sub-linearly. The 16.10 M/s figure means each producer gets ~4 M/s, which is still 120ns per enqueue. For FIX message processing where each message takes ~150ns to parse, the queue is never the bottleneck.",
          "a_short": "Expected. MPSC needs CAS vs SPSC's simple store. 16.10 M/s with 4 producers means 120ns per enqueue - never the bottleneck vs 150ns message parsing.",
          "keywords": ["spsc", "mpsc", "throughput", "cas", "contention", "producers", "performance"],
          "audio_short": "audio/v016_q03_short.mp3",
          "audio_full": "audio/v016_q03_full.mp3"
        },
        {
          "q": "How do you handle the case where the queue is full?",
          "a": "try_push returns false, and the caller decides the policy. In trading systems, the correct response to a full queue depends on context: for market data, drop the oldest entry (it's stale anyway); for orders, apply backpressure (slow down submission). We don't bake a policy into the queue - that's an architectural decision for the user.",
          "a_short": "try_push returns false, caller decides policy. Market data: drop stale entries. Orders: apply backpressure. No policy baked into the queue.",
          "keywords": ["full", "queue", "overflow", "backpressure", "policy", "try_push"],
          "audio_short": "audio/v016_q04_short.mp3",
          "audio_full": "audio/v016_q04_full.mp3"
        },
        {
          "q": "What's the memory ordering strategy for your atomics?",
          "a": "Producer uses memory_order_release on the sequence store (publishes the written data). Consumer uses memory_order_acquire on the sequence load (sees the published data). We never use seq_cst because the stronger ordering adds memory barrier instructions on x86 that aren't needed for our single-direction data flow. This is ~5ns per operation on modern Intel CPUs.",
          "a_short": "Release on producer store, acquire on consumer load. Never seq_cst - unnecessary memory barriers for single-direction data flow. ~5ns per operation.",
          "keywords": ["memory order", "atomic", "release", "acquire", "seq_cst", "barrier"],
          "audio_short": "audio/v016_q05_short.mp3",
          "audio_full": "audio/v016_q05_full.mp3"
        },
        {
          "q": "How do you size the queue for production use?",
          "a": "Power-of-two sizes (4096 is our default) for fast modulo via bitmask. The size should be large enough to absorb burst traffic but small enough to stay in L2 cache. 4096 entries * 64 bytes per slot = 256KB, which fits in L2 on all modern x86 CPUs. We've found that queue depths beyond 8192 rarely help because if you're that far behind, latency is already unacceptable.",
          "a_short": "Power-of-two (default 4096) for bitmask modulo. 4096 * 64 bytes = 256KB fits in L2 cache. Beyond 8192 rarely helps - if that far behind, latency is unacceptable.",
          "keywords": ["size", "queue", "power of two", "cache", "l2", "4096", "depth"],
          "audio_short": "audio/v016_q06_short.mp3",
          "audio_full": "audio/v016_q06_full.mp3"
        },
        {
          "q": "False sharing - how do you verify it's actually eliminated?",
          "a": "perf c2c (cache-to-cache transfer profiling) on Linux. We run a multi-producer benchmark and check for HITM (hit in modified) events on queue slots. With proper padding, HITM drops to near zero. We also verify with pahole that struct layout matches expectations (64-byte aligned slots).",
          "a_short": "perf c2c profiling checks for HITM events on queue slots. With padding, HITM drops to near zero. pahole verifies 64-byte aligned struct layout.",
          "keywords": ["false sharing", "cache", "perf", "c2c", "hitm", "padding", "verify"],
          "audio_short": "audio/v016_q07_short.mp3",
          "audio_full": "audio/v016_q07_full.mp3"
        },
        {
          "q": "Why not use std::atomic_ref instead of embedding atomics in slots?",
          "a": "std::atomic_ref requires the referenced object to meet alignment requirements, and the lifetime semantics are more complex. Embedding std::atomic<uint64_t> in the slot struct is explicit and makes the memory ordering visible in the type. For a lock-free data structure, explicitness is more valuable than abstraction.",
          "a_short": "atomic_ref has complex lifetime semantics. Embedding std::atomic in slots is explicit and makes memory ordering visible in the type. Explicitness over abstraction.",
          "keywords": ["atomic_ref", "atomic", "slot", "embedded", "explicit", "lock-free"],
          "audio_short": "audio/v016_q08_short.mp3",
          "audio_full": "audio/v016_q08_full.mp3"
        },
        {
          "q": "How does this queue compare to Linux's io_uring submission queue?",
          "a": "Similar concept - both are shared-memory ring buffers with separate producer/consumer pointers. io_uring is kernel-user shared memory, ours is user-space only. io_uring uses memory barriers (smp_store_release, smp_load_acquire) which map directly to our C++ memory ordering. The key difference is that io_uring carries I/O requests while ours carries FIX messages.",
          "a_short": "Similar shared-memory ring buffers. io_uring is kernel-user, ours is user-space only. Same memory barrier patterns. Different payload: I/O requests vs FIX messages.",
          "keywords": ["io_uring", "ring buffer", "kernel", "comparison", "shared memory"],
          "audio_short": "audio/v016_q09_short.mp3",
          "audio_full": "audio/v016_q09_full.mp3"
        },
        {
          "q": "Could you use std::latch or std::barrier instead of rolling your own synchronization?",
          "a": "No - std::latch and std::barrier are blocking synchronization primitives. They put the calling thread to sleep when the condition isn't met. On a hot path, sleeping is catastrophic for latency. Our lock-free queue uses spin-polling with _mm_pause() hints, which keeps the thread active and ready to process the next message within nanoseconds.",
          "a_short": "No. latch/barrier are blocking (sleep). Lock-free queue uses spin-polling with _mm_pause() to stay active and process within nanoseconds.",
          "keywords": ["latch", "barrier", "blocking", "spin", "lock-free", "synchronization"],
          "audio_short": "audio/v016_q10_short.mp3",
          "audio_full": "audio/v016_q10_full.mp3"
        },
        {
          "q": "What about using std::jthread and std::stop_token for the consumer?",
          "a": "We do use std::stop_token for clean shutdown. The consumer loop checks stop_token.stop_requested() between queue polls. This is cleaner than the traditional std::atomic<bool> running_ flag because the stop request integrates with the thread's lifecycle. However, on the hot path during market hours, we never check - the thread runs until market close.",
          "a_short": "Yes, std::stop_token for clean shutdown. Cleaner than atomic<bool> running_ flag. During market hours the thread runs without checking until market close.",
          "keywords": ["jthread", "stop_token", "shutdown", "consumer", "thread", "lifecycle"],
          "audio_short": "audio/v016_q11_short.mp3",
          "audio_full": "audio/v016_q11_full.mp3"
        },
        {
          "q": "How do you handle producer failure (crash) mid-enqueue?",
          "a": "Our CAS-based enqueue is atomic at the slot level. A producer either completes the write and publishes the sequence number, or it doesn't. There's no intermediate state visible to the consumer. If a producer crashes between claiming a slot and writing data, that slot is permanently reserved and the consumer will spin on it forever. In practice, this means the consumer must have a timeout or heartbeat mechanism at a higher protocol level.",
          "a_short": "CAS enqueue is atomic at slot level - no intermediate state visible. A crashed producer leaves a reserved slot requiring higher-level timeout/heartbeat detection.",
          "keywords": ["crash", "failure", "producer", "atomic", "stuck", "timeout", "heartbeat"],
          "audio_short": "audio/v016_q12_short.mp3",
          "audio_full": "audio/v016_q12_full.mp3"
        }
      ]
    },
    {
      "id": "v017",
      "label": "v0.1.7 SBE Encoding",
      "questions": [
        {
          "q": "SBE shows 5.5x speedup - but doesn't it require format conversion at the boundary?",
          "a": "Yes, and that's by design. SBE is for internal IPC between co-located processes (e.g., gateway to strategy engine), not for external FIX communication. The FIX gateway parses text FIX once and converts to SBE. All downstream processing uses the binary format. The conversion cost is amortized across all consumers. If you have N consumers, the total cost is: 1 parse + 1 encode + N decodes, vs N parses without SBE.",
          "a_short": "By design. SBE is for internal IPC. Parse FIX text once, convert to SBE, all downstream uses binary. Cost amortized: 1 parse + 1 encode + N decodes.",
          "keywords": ["sbe", "conversion", "boundary", "ipc", "binary", "encode", "speedup"],
          "audio_short": "audio/v017_q01_short.mp3",
          "audio_full": "audio/v017_q01_full.mp3"
        },
        {
          "q": "How does the flyweight pattern work for SBE decode?",
          "a": "The SBE decoder is a zero-allocation flyweight - it wraps a pointer to the raw buffer and provides typed accessor methods. Each accessor reads directly from the buffer at a compile-time offset. There's no intermediate object, no field map, no memory allocation. The accessor inlines to a single mov instruction in assembly. The decode is really just casting a byte pointer.",
          "a_short": "Zero-allocation flyweight wrapping a buffer pointer. Each accessor reads at a compile-time offset, inlining to a single mov instruction. No intermediate objects.",
          "keywords": ["flyweight", "decode", "sbe", "zero-allocation", "accessor", "mov"],
          "audio_short": "audio/v017_q02_short.mp3",
          "audio_full": "audio/v017_q02_full.mp3"
        },
        {
          "q": "Fixed offsets require fixed message layout - how do you handle optional fields?",
          "a": "SBE uses a schema-driven approach. Fixed fields are at known offsets. Optional fields use sentinel values (e.g., INT64_NULL = INT64_MIN). Variable-length fields (like free text) go at the end of the message with a length prefix. This is different from FIX text where any field can appear in any order. The trade-off is flexibility for performance.",
          "a_short": "Schema-driven: fixed fields at known offsets, optional fields use sentinel values, variable-length fields at the end with length prefix. Flexibility traded for performance.",
          "keywords": ["optional", "fields", "sbe", "schema", "sentinel", "layout", "variable"],
          "audio_short": "audio/v017_q03_short.mp3",
          "audio_full": "audio/v017_q03_full.mp3"
        },
        {
          "q": "What about endianness?",
          "a": "SBE specifies little-endian encoding, which is native on x86-64. No byte-swapping needed on our target platform. For big-endian platforms (rare in modern trading), the schema defines byte order and the generated code includes conversion. We use std::endian (C++20) for compile-time endianness detection.",
          "a_short": "SBE uses little-endian, native on x86-64. No byte-swapping needed. std::endian for compile-time detection on other platforms.",
          "keywords": ["endian", "byte order", "little-endian", "big-endian", "sbe", "swap"],
          "audio_short": "audio/v017_q04_short.mp3",
          "audio_full": "audio/v017_q04_full.mp3"
        },
        {
          "q": "How does SBE message size compare to FIX text?",
          "a": "ExecutionReport: 144 bytes SBE vs ~350 bytes FIX text (59% reduction). NewOrderSingle: 64 bytes vs ~200 bytes (68% reduction). Smaller messages mean better cache utilization - more messages fit in a cache line - and lower memory bandwidth consumption. For high-frequency market data, the bandwidth savings alone can be significant.",
          "a_short": "ExecutionReport: 144 bytes SBE vs ~350 bytes FIX text (59% reduction). Smaller messages mean better cache utilization and lower bandwidth.",
          "keywords": ["size", "sbe", "fix", "comparison", "bytes", "reduction", "bandwidth"],
          "audio_short": "audio/v017_q05_short.mp3",
          "audio_full": "audio/v017_q05_full.mp3"
        },
        {
          "q": "Did you generate the SBE codecs from a schema or hand-write them?",
          "a": "Both. We have an XML schema that defines message layouts (field types, offsets, sizes) and a code generator that produces C++ accessor classes. The generated code is checked into the repository so users don't need the generator. The hand-tuned parts are the constexpr offset calculations and the std::assume_aligned hints.",
          "a_short": "Both. XML schema with code generator for C++ accessor classes. Generated code checked in. Hand-tuned constexpr offsets and assume_aligned hints.",
          "keywords": ["schema", "codegen", "generate", "xml", "sbe", "codec", "hand-written"],
          "audio_short": "audio/v017_q06_short.mp3",
          "audio_full": "audio/v017_q06_full.mp3"
        },
        {
          "q": "Why not use FlatBuffers or Cap'n Proto instead of SBE?",
          "a": "SBE is designed specifically for financial messaging by the FIX Trading Community. It has the simplest possible encoding: fixed-size fields at fixed offsets, no pointer indirection, no schema evolution overhead. FlatBuffers adds vtable indirection for field access. Cap'n Proto adds pointer manipulation. For a known, stable schema like FIX messages, SBE's simplicity wins.",
          "a_short": "SBE has the simplest encoding: fixed offsets, no pointer indirection. FlatBuffers adds vtable, Cap'n Proto adds pointers. For stable FIX schemas, simplicity wins.",
          "keywords": ["flatbuffers", "capnproto", "sbe", "comparison", "serialization", "alternative"],
          "audio_short": "audio/v017_q07_short.mp3",
          "audio_full": "audio/v017_q07_full.mp3"
        },
        {
          "q": "What's the encode path performance?",
          "a": "Encoding is symmetric to decoding - direct writes at fixed offsets. NewOrderSingle encode takes ~15ns (vs ~35ns decode). Encode is faster because there's no validation step. We validate field values during parsing from FIX text; by the time we encode to SBE, the data is already validated.",
          "a_short": "~15ns encode for NewOrderSingle vs ~35ns decode. Encode is faster because data is already validated from FIX text parsing.",
          "keywords": ["encode", "performance", "speed", "sbe", "write", "nanoseconds"],
          "audio_short": "audio/v017_q08_short.mp3",
          "audio_full": "audio/v017_q08_full.mp3"
        },
        {
          "q": "How do you handle schema evolution?",
          "a": "SBE supports backward-compatible evolution by appending new fields and incrementing the schema version. Decoders check the version and only access fields present in their schema version. For internal IPC within a single deployment, schema mismatch is a deployment error (both sides should be the same version). We version the schema in the SBE header.",
          "a_short": "Append new fields and increment schema version. Decoders check version and access only known fields. For internal IPC, mismatch is a deployment error.",
          "keywords": ["schema", "evolution", "backward", "compatible", "version", "sbe"],
          "audio_short": "audio/v017_q09_short.mp3",
          "audio_full": "audio/v017_q09_full.mp3"
        },
        {
          "q": "reinterpret_cast for field access - isn't that undefined behavior?",
          "a": "Technically, yes - strict aliasing rules apply. In practice, all major compilers on our target platforms (GCC, Clang, MSVC) handle this correctly for POD types with proper alignment. We also have -fno-strict-aliasing as a build option for paranoid users. The alternative (memcpy + optimizer) generates identical code but is less readable. We chose pragmatism over pedantic correctness for this specific case.",
          "a_short": "Technically UB due to strict aliasing. Practically, all major compilers handle it correctly for aligned POD types. memcpy alternative generates identical code.",
          "keywords": ["reinterpret_cast", "undefined behavior", "aliasing", "strict", "ub", "cast"],
          "audio_short": "audio/v017_q10_short.mp3",
          "audio_full": "audio/v017_q10_full.mp3"
        },
        {
          "q": "Could you use std::bit_cast instead of reinterpret_cast?",
          "a": "std::bit_cast is for value types of the same size, not for pointer-based access patterns. For SBE decode, we're reading from a byte buffer at an offset, which is inherently a pointer operation. The correct standards-compliant approach would be memcpy into a local variable, which compilers optimize to the same mov instruction. We may migrate to memcpy style for strict conformance in a future release.",
          "a_short": "bit_cast is for same-size value types, not pointer-based buffer access. Standards-compliant approach is memcpy, which compilers optimize to the same mov.",
          "keywords": ["bit_cast", "reinterpret_cast", "memcpy", "standards", "conformance"],
          "audio_short": "audio/v017_q11_short.mp3",
          "audio_full": "audio/v017_q11_full.mp3"
        }
      ]
    },
    {
      "id": "v018",
      "label": "v0.1.8 Utilities",
      "questions": [
        {
          "q": "What utilities were added in this Phase 2?",
          "a": "Scope guards (ScopeExit, ScopeSuccess, ScopeFail), a constexpr string hash for compile-time string matching, enhanced static_assert messages for better error diagnostics, and std::source_location-based logging utilities. These are infrastructure pieces that other components depend on.",
          "a_short": "Scope guards, constexpr string hash, enhanced static_assert messages, and std::source_location logging. Infrastructure for other components.",
          "keywords": ["utilities", "scope guard", "string hash", "source_location", "static_assert"],
          "audio_short": "audio/v018_q01_short.mp3",
          "audio_full": "audio/v018_q01_full.mp3"
        },
        {
          "q": "How does the scope guard pattern compare to RAII?",
          "a": "Scope guards are RAII for ad-hoc cleanup. Traditional RAII requires defining a class for each resource type. ScopeExit lets you attach cleanup logic to a scope without a dedicated class: auto guard = ScopeExit([&]{ buffer.release(); });. It's particularly useful for error paths where multiple resources need cleanup in reverse order.",
          "a_short": "RAII for ad-hoc cleanup without dedicated classes. ScopeExit attaches cleanup logic to a scope. Useful for error paths with multiple resources.",
          "keywords": ["scope guard", "raii", "cleanup", "scopeexit", "resource", "ad-hoc"],
          "audio_short": "audio/v018_q02_short.mp3",
          "audio_full": "audio/v018_q02_full.mp3"
        },
        {
          "q": "Why implement scope guards instead of using the Guideline Support Library (GSL)?",
          "a": "GSL's gsl::finally is C++14-era design. Our ScopeExit is noexcept-correct, move-only, and uses C++23 deduction guides. We also provide ScopeSuccess and ScopeFail (using std::uncaught_exceptions() counting) which GSL doesn't offer. Total implementation is ~50 lines - not worth a dependency.",
          "a_short": "GSL's finally is C++14 design. Our version is noexcept-correct with C++23 deduction guides, plus ScopeSuccess/ScopeFail. ~50 lines, not worth a dependency.",
          "keywords": ["gsl", "guideline", "finally", "scope guard", "dependency", "noexcept"],
          "audio_short": "audio/v018_q03_short.mp3",
          "audio_full": "audio/v018_q03_full.mp3"
        },
        {
          "q": "What's the compile-time string hash used for?",
          "a": "Fast dispatch on FIX message type strings. Instead of if (msgtype == \"8\") ... else if (msgtype == \"D\"), we compute a constexpr hash at compile time for each known type and use a switch statement on the hash at runtime. This compiles to a jump table, which is O(1) instead of O(N) string comparisons.",
          "a_short": "Fast O(1) dispatch on FIX message types via constexpr hash and switch/jump table, replacing O(N) string comparisons.",
          "keywords": ["string hash", "constexpr", "dispatch", "jump table", "msgtype", "fnv"],
          "audio_short": "audio/v018_q04_short.mp3",
          "audio_full": "audio/v018_q04_full.mp3"
        },
        {
          "q": "How do you handle hash collisions in the string hash?",
          "a": "FIX message types are 1-2 character strings with a very small domain (about 30 types). We use FNV-1a which has no collisions in this domain - verified at compile time with static_assert. For general use, collision handling would be needed, but for a known, small, fixed set of strings, we can prove collision-freedom.",
          "a_short": "FNV-1a has zero collisions for FIX message types (~30 types). Verified at compile time with static_assert. Small fixed domain allows provable collision-freedom.",
          "keywords": ["collision", "hash", "fnv", "static_assert", "string", "verified"],
          "audio_short": "audio/v018_q05_short.mp3",
          "audio_full": "audio/v018_q05_full.mp3"
        },
        {
          "q": "What does std::source_location give you for logging?",
          "a": "File name, line number, column, and function name - all captured at compile time with zero runtime overhead. The caller doesn't need to pass __FILE__ and __LINE__ macros. It integrates with std::format for clean, type-safe log messages: log::debug(\"parsed {} fields\", count) automatically includes the call site.",
          "a_short": "File, line, column, function captured at compile time with zero overhead. No __FILE__/__LINE__ macros needed. Integrates with std::format.",
          "keywords": ["source_location", "logging", "file", "line", "debug", "compile-time"],
          "audio_short": "audio/v018_q06_short.mp3",
          "audio_full": "audio/v018_q06_full.mp3"
        },
        {
          "q": "How do you ensure these utilities don't add overhead?",
          "a": "Everything is constexpr or consteval where possible, noexcept everywhere, and [[nodiscard]] on all return values. We verify with objdump that utility functions inline completely at optimization level -O2. If a utility doesn't inline, it doesn't belong on the hot path and we mark it accordingly.",
          "a_short": "constexpr/consteval, noexcept, [[nodiscard]] everywhere. Verified with objdump that all utilities inline at -O2.",
          "keywords": ["overhead", "inline", "constexpr", "noexcept", "objdump", "zero cost"],
          "audio_short": "audio/v018_q07_short.mp3",
          "audio_full": "audio/v018_q07_full.mp3"
        },
        {
          "q": "Are these utilities tested independently?",
          "a": "Yes, each utility has dedicated Catch2 tests including edge cases (empty strings for hash, exception-during-construction for scope guards, nested scopes). We also have compile-time tests using static_assert to verify constexpr evaluation.",
          "a_short": "Yes. Dedicated Catch2 tests with edge cases plus compile-time static_assert tests to verify constexpr evaluation.",
          "keywords": ["testing", "utilities", "catch2", "static_assert", "edge cases"],
          "audio_short": "audio/v018_q08_short.mp3",
          "audio_full": "audio/v018_q08_full.mp3"
        },
        {
          "q": "What static_assert improvements did you add?",
          "a": "C++23 allows computed static_assert messages using std::format-like syntax. Instead of static_assert(sizeof(T) == 64, \"wrong size\"), we can write static_assert(sizeof(T) == 64, std::format(\"expected 64, got {}\", sizeof(T))). This makes template errors actionable rather than cryptic.",
          "a_short": "C++23 computed static_assert messages with std::format syntax. Makes template errors actionable instead of cryptic.",
          "keywords": ["static_assert", "c++23", "format", "error message", "template", "diagnostic"],
          "audio_short": "audio/v018_q09_short.mp3",
          "audio_full": "audio/v018_q09_full.mp3"
        },
        {
          "q": "How do these utilities compose with the rest of the codebase?",
          "a": "They're foundational - used by every other component. The scope guard manages buffer pool lifetimes, the string hash powers message dispatch, source_location drives all diagnostic output. They're in a nfx::util namespace and header-only, so they're available everywhere with zero coupling.",
          "a_short": "Foundational: scope guard for buffer pools, string hash for dispatch, source_location for diagnostics. In nfx::util namespace, header-only, zero coupling.",
          "keywords": ["compose", "foundation", "namespace", "coupling", "dependency", "reuse"],
          "audio_short": "audio/v018_q10_short.mp3",
          "audio_full": "audio/v018_q10_full.mp3"
        }
      ]
    },
    {
      "id": "v019",
      "label": "v0.1.9 MsgType Dispatch",
      "questions": [
        {
          "q": "What's compile-time MsgType dispatch and why does it matter?",
          "a": "FIX messages have a MsgType field (tag 35) that determines processing logic. Traditional parsers use runtime string comparison (if/else chains or std::unordered_map). We use a constexpr lookup table indexed by the MsgType character(s), resolved to function pointers at compile time. This eliminates hash computation, string comparison, and branch misprediction on the dispatch path.",
          "a_short": "constexpr lookup table indexed by MsgType character, resolved to function pointers at compile time. Eliminates hashing, string comparison, and branch misprediction.",
          "keywords": ["msgtype", "dispatch", "compile-time", "tag35", "lookup table", "function pointer"],
          "audio_short": "audio/v019_q01_short.mp3",
          "audio_full": "audio/v019_q01_full.mp3"
        },
        {
          "q": "How do you handle the MsgType encoding? Some types are single character, some are multi-character.",
          "a": "FIX 4.4 MsgTypes are predominantly single ASCII characters ('0'-'9', 'A'-'Z'). For single-char types, we use direct array indexing: dispatch_table[msgtype_char]. For multi-char types (rare, like \"AA\"), we fall back to the compile-time hash from v0.1.8. The single-char path covers >95% of real-world traffic.",
          "a_short": "Single-char types (>95% of traffic) use direct array indexing. Rare multi-char types fall back to compile-time hash from v0.1.8.",
          "keywords": ["msgtype", "encoding", "single char", "multi char", "ascii", "array index"],
          "audio_short": "audio/v019_q02_short.mp3",
          "audio_full": "audio/v019_q02_full.mp3"
        },
        {
          "q": "What's the benchmark result for dispatch specifically?",
          "a": "Compile-time dispatch: ~2ns per message type resolution. std::unordered_map dispatch: ~25ns. if/else chain: ~8ns average, ~20ns worst case (last branch). The 2ns figure is essentially the cost of an array index and indirect function call - both are single instructions.",
          "a_short": "~2ns compile-time dispatch vs ~25ns unordered_map vs ~8-20ns if/else chain. Just an array index plus indirect function call.",
          "keywords": ["benchmark", "dispatch", "2ns", "25ns", "performance", "speed"],
          "audio_short": "audio/v019_q03_short.mp3",
          "audio_full": "audio/v019_q03_full.mp3"
        },
        {
          "q": "Can users register custom message type handlers?",
          "a": "Yes, the dispatch table is populated at initialization time. Users provide handler functions for each MsgType they care about. Unhandled types route to a default handler. The table is constexpr-constructible if all handlers are known at compile time, or runtime-initialized if handlers are registered dynamically.",
          "a_short": "Yes. Users register handler functions per MsgType. Unhandled types route to a default. Table is constexpr if all handlers known at compile time.",
          "keywords": ["custom", "handler", "register", "dispatch", "callback", "dynamic"],
          "audio_short": "audio/v019_q04_short.mp3",
          "audio_full": "audio/v019_q04_full.mp3"
        },
        {
          "q": "How does this interact with the structural index from v0.1.13?",
          "a": "The structural index finds the MsgType field position. The dispatch table routes to the handler. They're complementary stages: index gives O(1) field access, dispatch gives O(1) handler selection. Together, the overhead from raw bytes to executing the right handler with parsed fields is under 50ns.",
          "a_short": "Complementary: structural index provides O(1) field access, dispatch provides O(1) handler selection. Combined overhead: under 50ns from raw bytes to handler.",
          "keywords": ["structural index", "dispatch", "interaction", "combined", "50ns"],
          "audio_short": "audio/v019_q05_short.mp3",
          "audio_full": "audio/v019_q05_full.mp3"
        },
        {
          "q": "Why function pointers instead of std::function or virtual dispatch?",
          "a": "std::function has heap allocation for large captures and indirection through a type-erased wrapper. Virtual dispatch has vtable indirection. Function pointers are a single indirect call instruction. For a dispatch table that's consulted on every message, the difference is measurable - ~2ns for function pointer vs ~8ns for std::function.",
          "a_short": "Function pointers are a single indirect call (~2ns). std::function adds heap allocation and type erasure (~8ns). Virtual dispatch adds vtable indirection.",
          "keywords": ["function pointer", "std::function", "virtual", "dispatch", "overhead", "indirection"],
          "audio_short": "audio/v019_q06_short.mp3",
          "audio_full": "audio/v019_q06_full.mp3"
        },
        {
          "q": "What happens if the MsgType is invalid or unknown?",
          "a": "The dispatch table has a default handler at every index. Unknown types route to a configurable reject handler that can log, count, or ignore the message. No branch is needed to check validity - every possible byte value has a handler. This is the branch-free design philosophy: handle all cases through the table, not through conditionals.",
          "a_short": "Default handler at every index. Unknown types route to configurable reject handler. No validity branches - every byte value has a handler.",
          "keywords": ["invalid", "unknown", "default", "handler", "branch-free", "reject"],
          "audio_short": "audio/v019_q07_short.mp3",
          "audio_full": "audio/v019_q07_full.mp3"
        },
        {
          "q": "How do you test the dispatch table for correctness?",
          "a": "static_assert at compile time verifies that all required FIX 4.4 message types have handlers. Runtime tests send each message type through the dispatcher and verify the correct handler was invoked. We also test with random byte values to ensure the default handler catches everything.",
          "a_short": "Compile-time static_assert for all FIX 4.4 types. Runtime tests verify correct handler invocation. Random byte values test the default handler.",
          "keywords": ["test", "dispatch", "static_assert", "verify", "correctness"],
          "audio_short": "audio/v019_q08_short.mp3",
          "audio_full": "audio/v019_q08_full.mp3"
        },
        {
          "q": "Is this technique applicable to protocols other than FIX?",
          "a": "Absolutely. Any protocol with a type discriminator field can use this pattern: HTTP methods, DNS record types, network packet types. The key requirements are: (1) small, bounded discriminator domain, (2) known at compile time, (3) maps to distinct processing logic. FIX MsgType is a textbook case.",
          "a_short": "Yes. Works for any protocol with a small, bounded, compile-time-known type discriminator: HTTP methods, DNS types, packet types.",
          "keywords": ["applicable", "other protocols", "http", "dns", "generic", "pattern"],
          "audio_short": "audio/v019_q09_short.mp3",
          "audio_full": "audio/v019_q09_full.mp3"
        },
        {
          "q": "What's the memory cost of the dispatch table?",
          "a": "256 entries (one per byte value) * 8 bytes per function pointer = 2KB. Fits in L1 cache on any modern CPU. For multi-char MsgTypes, the hash table adds another ~1KB. Total dispatch infrastructure is under 4KB - trivial compared to the message buffers it operates on.",
          "a_short": "256 entries * 8 bytes = 2KB, fits in L1 cache. Multi-char hash adds ~1KB. Total under 4KB - trivial compared to message buffers.",
          "keywords": ["memory", "cost", "size", "dispatch table", "cache", "2kb"],
          "audio_short": "audio/v019_q10_short.mp3",
          "audio_full": "audio/v019_q10_full.mp3"
        }
      ]
    },
    {
      "id": "v0110",
      "label": "v0.1.10 Compile-time",
      "questions": [
        {
          "q": "What does compile-time optimization roadmap mean in practice?",
          "a": "We systematically identified every computation in the parser that could be moved from runtime to compile time. Field offset calculations, tag number validation, checksum seeds, message type dispatch tables - all converted to constexpr or consteval. The roadmap document (TICKET_023) catalogs 23 specific optimizations with before/after assembly analysis.",
          "a_short": "Systematic migration of parser computations to compile time: field offsets, tag validation, checksum seeds, dispatch tables. 23 optimizations cataloged with assembly analysis.",
          "keywords": ["compile-time", "optimization", "roadmap", "constexpr", "consteval", "systematic"],
          "audio_short": "audio/v0110_q01_short.mp3",
          "audio_full": "audio/v0110_q01_full.mp3"
        },
        {
          "q": "How do you verify that something actually evaluates at compile time?",
          "a": "Three methods: (1) static_assert(value == expected) - if it compiles, the value was computed at compile time. (2) consteval functions - the compiler errors if it can't evaluate at compile time. (3) Assembly inspection with objdump to confirm no runtime computation. We use all three for different confidence levels.",
          "a_short": "Three methods: static_assert (compiles = compile-time), consteval (compiler-enforced), and objdump assembly inspection. All three for different confidence levels.",
          "keywords": ["verify", "compile-time", "static_assert", "consteval", "objdump", "assembly"],
          "audio_short": "audio/v0110_q02_short.mp3",
          "audio_full": "audio/v0110_q02_full.mp3"
        },
        {
          "q": "What's the performance impact of moving computation to compile time?",
          "a": "For individual operations, the impact is small (a few nanoseconds each). The cumulative impact is significant: 23 operations * ~3ns average = ~70ns saved per message parse. More importantly, it reduces instruction count on the hot path, which improves instruction cache utilization and reduces branch misprediction opportunities.",
          "a_short": "23 operations * ~3ns = ~70ns saved per parse. More importantly, fewer instructions on hot path improves cache utilization and reduces branch misprediction.",
          "keywords": ["performance", "impact", "compile-time", "nanoseconds", "cumulative", "cache"],
          "audio_short": "audio/v0110_q03_short.mp3",
          "audio_full": "audio/v0110_q03_full.mp3"
        },
        {
          "q": "Are there diminishing returns to compile-time optimization?",
          "a": "Yes, we hit them in this release. The high-value targets (dispatch tables, field offsets) were converted in earlier releases. This phase covered the long tail: validation constants, error message strings, configuration defaults. Each individual optimization saves single-digit nanoseconds. We stopped when the remaining candidates were below 1ns impact.",
          "a_short": "Yes. High-value targets done earlier. This phase covered the long tail at single-digit nanoseconds each. Stopped when candidates fell below 1ns impact.",
          "keywords": ["diminishing returns", "optimization", "long tail", "limit", "compile-time"],
          "audio_short": "audio/v0110_q04_short.mp3",
          "audio_full": "audio/v0110_q04_full.mp3"
        },
        {
          "q": "Does constexpr everywhere increase compile time significantly?",
          "a": "Moderately. Our full build went from ~15s to ~18s (20% increase). Template-heavy code and deep constexpr evaluation trees are the main cost. We mitigate this with explicit template instantiation for common types and extern template declarations. For users consuming the library, the impact is smaller since they typically instantiate fewer templates.",
          "a_short": "~20% compile time increase (15s to 18s). Mitigated with explicit template instantiation and extern template declarations.",
          "keywords": ["compile time", "slow", "build", "constexpr", "template", "instantiation"],
          "audio_short": "audio/v0110_q05_short.mp3",
          "audio_full": "audio/v0110_q05_full.mp3"
        },
        {
          "q": "How do you decide between constexpr and consteval?",
          "a": "consteval for things that must always be compile-time (protocol constants, lookup tables). constexpr for things that can be either (utility functions used in both compile-time table construction and runtime fallback paths). The rule: if a runtime call would be a bug, use consteval. If runtime is acceptable but compile-time is preferred, use constexpr.",
          "a_short": "consteval for must-be-compile-time (protocol constants). constexpr for can-be-either (utilities). Rule: runtime call = bug? Use consteval.",
          "keywords": ["constexpr", "consteval", "decide", "when", "rule", "compile-time"],
          "audio_short": "audio/v0110_q06_short.mp3",
          "audio_full": "audio/v0110_q06_full.mp3"
        },
        {
          "q": "What did the assembly analysis reveal?",
          "a": "The most surprising finding was that GCC was already optimizing some constexpr functions to compile-time constants even without explicit consteval marking. However, this optimization is fragile - small changes to the function or its call site can cause the compiler to give up. consteval makes the guarantee explicit and resilient to refactoring.",
          "a_short": "GCC already optimized some constexpr to compile-time, but fragile - small changes break it. consteval makes the guarantee explicit and refactoring-resilient.",
          "keywords": ["assembly", "analysis", "gcc", "optimization", "fragile", "consteval"],
          "audio_short": "audio/v0110_q07_short.mp3",
          "audio_full": "audio/v0110_q07_full.mp3"
        },
        {
          "q": "Can compile-time computation cause code bloat?",
          "a": "Yes, if templates are instantiated with many different parameters. Each unique instantiation generates separate code. We control this by using NTTP sparingly and preferring runtime parameters for values with large domains. For example, tag numbers are runtime values (thousands of possible tags) but message types are compile-time parameters (30 types).",
          "a_short": "Yes, via template instantiation. Controlled by using NTTP sparingly. Large domains (tag numbers) stay runtime; small domains (message types) are compile-time.",
          "keywords": ["code bloat", "template", "instantiation", "nttp", "binary size"],
          "audio_short": "audio/v0110_q08_short.mp3",
          "audio_full": "audio/v0110_q08_full.mp3"
        },
        {
          "q": "How does LTO interact with compile-time optimization?",
          "a": "LTO (Link-Time Optimization) can perform cross-TU inlining and constant propagation, which catches some opportunities that per-TU constexpr evaluation misses. However, LTO is expensive and unpredictable. We prefer explicit constexpr/consteval because the optimization is guaranteed and visible in the source code.",
          "a_short": "LTO catches cross-TU opportunities that per-TU constexpr misses, but is expensive and unpredictable. We prefer explicit constexpr/consteval for guaranteed optimization.",
          "keywords": ["lto", "link-time", "optimization", "cross-tu", "inlining", "constant propagation"],
          "audio_short": "audio/v0110_q09_short.mp3",
          "audio_full": "audio/v0110_q09_full.mp3"
        },
        {
          "q": "What's left to optimize after this phase?",
          "a": "Runtime computation that can't be moved to compile time: SIMD operations (data-dependent), memory allocation (dynamic), I/O (inherently runtime). The remaining optimization frontier is algorithmic (SIMD, structural indexing) rather than compile-time. This is why v0.1.13 (structural index) was the next major performance milestone.",
          "a_short": "SIMD (data-dependent), allocation (dynamic), I/O (runtime) can't move to compile time. Remaining frontier is algorithmic - hence v0.1.13 structural index.",
          "keywords": ["remaining", "optimize", "frontier", "simd", "algorithmic", "next"],
          "audio_short": "audio/v0110_q10_short.mp3",
          "audio_full": "audio/v0110_q10_full.mp3"
        }
      ]
    },
    {
      "id": "v0111",
      "label": "v0.1.11 C++23 Quick Wins",
      "questions": [
        {
          "q": "What C++23 quick wins did you adopt?",
          "a": "[[assume(expr)]] for optimization hints, std::unreachable() for dead code elimination, if consteval for better compile-time branching, and std::to_underlying() for safe enum-to-integer conversion. Each is a small change that enables the compiler to generate better code. The cumulative effect was 8-12% improvement on the hot path.",
          "a_short": "[[assume]], std::unreachable(), if consteval, std::to_underlying(). Small changes enabling better codegen. Cumulative 8-12% hot path improvement.",
          "keywords": ["c++23", "assume", "unreachable", "consteval", "to_underlying", "quick wins"],
          "audio_short": "audio/v0111_q01_short.mp3",
          "audio_full": "audio/v0111_q01_full.mp3"
        },
        {
          "q": "How does [[assume]] work and what's the risk?",
          "a": "[[assume(expr)]] tells the compiler that expr is always true. The compiler uses this to eliminate branches, simplify arithmetic, and enable vectorization. The risk: if the assumption is violated at runtime, it's undefined behavior - anything can happen. We use it only for conditions verified by earlier validation (e.g., after bounds checking, we assume the index is in range).",
          "a_short": "Tells compiler an expression is always true for branch elimination and vectorization. Risk: UB if violated. Only used after validation confirms the condition.",
          "keywords": ["assume", "optimization", "hint", "undefined behavior", "risk", "branch"],
          "audio_short": "audio/v0111_q02_short.mp3",
          "audio_full": "audio/v0111_q02_full.mp3"
        },
        {
          "q": "What was the Clang 18 compatibility issue with [[assume]]?",
          "a": "Clang 18 supported [[assume]] but had a bug where complex expressions inside [[assume]] could trigger incorrect codegen. Our fix was to add a version detection macro (NFX_HAS_ASSUME) that checks both the compiler version and a known-good version threshold. We also simplified our assume expressions to avoid triggering the Clang bug.",
          "a_short": "Clang 18 had a bug with complex [[assume]] expressions causing incorrect codegen. Fixed with version detection macro and simplified expressions.",
          "keywords": ["clang", "bug", "assume", "compatibility", "codegen", "macro"],
          "audio_short": "audio/v0111_q03_short.mp3",
          "audio_full": "audio/v0111_q03_full.mp3"
        },
        {
          "q": "Apple Clang has different version numbers - how do you handle that?",
          "a": "Apple Clang uses its own versioning that doesn't correspond to upstream Clang versions. We detect __apple_build_version__ and maintain a separate mapping table. Apple Clang 15 corresponds roughly to upstream Clang 16, for example. This is one of the most frustrating aspects of cross-platform C++ development.",
          "a_short": "Detect __apple_build_version__ with separate mapping table. Apple Clang 15 roughly equals upstream Clang 16. Frustrating cross-platform reality.",
          "keywords": ["apple", "clang", "version", "mapping", "cross-platform", "macos"],
          "audio_short": "audio/v0111_q04_short.mp3",
          "audio_full": "audio/v0111_q04_full.mp3"
        },
        {
          "q": "What does std::unreachable() compile to?",
          "a": "On GCC/Clang, it compiles to __builtin_unreachable(), which tells the optimizer that the code path is never taken. The compiler can eliminate any code that would only execute on that path. On MSVC, it compiles to __assume(false). In debug builds, we replace it with an assertion to catch logic errors during development.",
          "a_short": "GCC/Clang: __builtin_unreachable(). MSVC: __assume(false). Debug builds: replaced with assertion. Tells optimizer the path is never taken.",
          "keywords": ["unreachable", "builtin", "optimizer", "dead code", "elimination"],
          "audio_short": "audio/v0111_q05_short.mp3",
          "audio_full": "audio/v0111_q05_full.mp3"
        },
        {
          "q": "Give a concrete example of [[assume]] improving codegen.",
          "a": "In the structural index, after verifying a message has at least one field, we add [[assume(index.count > 0)]] before the field access loop. The compiler eliminates the empty message branch in the loop, saving a comparison and conditional jump per iteration. Assembly shows the loop body is 3 instructions shorter.",
          "a_short": "After verifying fields exist, [[assume(index.count > 0)]] eliminates the empty-message branch. Loop body is 3 instructions shorter in assembly.",
          "keywords": ["assume", "example", "codegen", "structural index", "loop", "branch"],
          "audio_short": "audio/v0111_q06_short.mp3",
          "audio_full": "audio/v0111_q06_full.mp3"
        },
        {
          "q": "How do you test that your [[assume]] statements are correct?",
          "a": "In debug builds, every [[assume(expr)]] is replaced with assert(expr) via a macro. This means debug builds crash immediately if an assumption is violated, while release builds get the optimization benefit. Our CI runs both debug (with sanitizers) and release (with benchmarks) configurations.",
          "a_short": "Debug builds replace [[assume]] with assert() via macro. CI runs both debug (sanitizers catch violations) and release (benchmarks verify speedup).",
          "keywords": ["test", "assume", "assert", "debug", "verify", "macro"],
          "audio_short": "audio/v0111_q07_short.mp3",
          "audio_full": "audio/v0111_q07_full.mp3"
        },
        {
          "q": "What's the risk of [[assume]] becoming stale after refactoring?",
          "a": "This is the primary maintenance risk. An assumption that was valid before a code change might become invalid after. Our mitigation: (1) assumptions are placed immediately after the validation that establishes them, (2) code review checks assumption validity, (3) sanitizer CI catches violations in debug builds. We also document why each assumption holds.",
          "a_short": "Primary maintenance risk. Mitigated by placing assumptions next to validations, code review, sanitizer CI in debug, and documenting why each holds.",
          "keywords": ["stale", "refactoring", "maintenance", "risk", "assume", "validity"],
          "audio_short": "audio/v0111_q08_short.mp3",
          "audio_full": "audio/v0111_q08_full.mp3"
        },
        {
          "q": "How does if consteval differ from if constexpr?",
          "a": "if constexpr selects branches based on compile-time-evaluable conditions. if consteval selects branches based on whether the function is currently being evaluated at compile time. This lets the same function use a fast-but-unsafe path at compile time (where UB is a compile error, not silent corruption) and a safe path at runtime.",
          "a_short": "if constexpr: branch on compile-time condition. if consteval: branch on whether currently evaluating at compile time. Enables different paths for compile vs runtime.",
          "keywords": ["if consteval", "if constexpr", "difference", "compile-time", "branch"],
          "audio_short": "audio/v0111_q09_short.mp3",
          "audio_full": "audio/v0111_q09_full.mp3"
        },
        {
          "q": "What's std::to_underlying() and why prefer it over a cast?",
          "a": "std::to_underlying(enum_val) converts an enum to its underlying integer type. It's safer than static_cast<int>(enum_val) because it automatically uses the correct underlying type. If the enum's underlying type changes from int to uint16_t, the cast silently narrows while to_underlying() adapts. Small benefit, but eliminates a class of subtle bugs.",
          "a_short": "Converts enum to its underlying type safely. Unlike static_cast<int>, automatically adapts if underlying type changes. Eliminates subtle narrowing bugs.",
          "keywords": ["to_underlying", "enum", "cast", "safe", "type", "conversion"],
          "audio_short": "audio/v0111_q10_short.mp3",
          "audio_full": "audio/v0111_q10_full.mp3"
        }
      ]
    },
    {
      "id": "v0112",
      "label": "v0.1.12 Ranges & C++23",
      "questions": [
        {
          "q": "Where do you use C++20/C++23 ranges in a parser?",
          "a": "Primarily in non-hot-path code: filtering fields by tag group (fields | filter(is_header_tag)), transforming field values (fields | transform(to_string_view)), and composing validation pipelines. Ranges replace manual loops with declarative intent. On the hot path, we still use hand-written loops for predictable performance.",
          "a_short": "Non-hot-path: filtering fields, transforming values, validation pipelines. Ranges replace manual loops with declarative intent. Hot path still uses hand-written loops.",
          "keywords": ["ranges", "c++20", "c++23", "filter", "transform", "pipeline", "views"],
          "audio_short": "audio/v0112_q01_short.mp3",
          "audio_full": "audio/v0112_q01_full.mp3"
        },
        {
          "q": "Why not use ranges on the hot path?",
          "a": "Range adaptors introduce layers of iterator abstraction that can inhibit optimization. While compilers are improving, we've measured 10-15% regression when replacing hand-written hot loops with equivalent range pipelines. The compiler struggles to see through multiple layers of lazy evaluation and iterator wrapping. We expect this gap to close in future compiler versions.",
          "a_short": "10-15% regression measured. Compiler struggles to see through multiple layers of lazy evaluation and iterator wrapping. Gap expected to close in future compilers.",
          "keywords": ["ranges", "hot path", "regression", "performance", "iterator", "abstraction"],
          "audio_short": "audio/v0112_q02_short.mp3",
          "audio_full": "audio/v0112_q02_full.mp3"
        },
        {
          "q": "Which specific range adaptors do you use most?",
          "a": "std::views::filter for field selection, std::views::transform for type conversion, std::views::take for limiting results, and std::views::enumerate (C++23) for indexed iteration. enumerate replaced a lot of manual size_t i = 0 counter variables, improving readability.",
          "a_short": "filter for field selection, transform for type conversion, take for limiting results, enumerate (C++23) for indexed iteration replacing manual counters.",
          "keywords": ["filter", "transform", "take", "enumerate", "views", "adaptors"],
          "audio_short": "audio/v0112_q03_short.mp3",
          "audio_full": "audio/v0112_q03_full.mp3"
        },
        {
          "q": "What C++23 language features did you adopt beyond ranges?",
          "a": "Deducing this for CRTP replacement, multidimensional subscript operator for matrix-like data access, std::print for formatted output (replacing std::cout), and auto(x) decay-copy syntax. Deducing this was the biggest win - it eliminated three CRTP base classes.",
          "a_short": "Deducing this (eliminated 3 CRTP classes), multidimensional subscript, std::print, and auto(x) decay-copy. Deducing this was the biggest win.",
          "keywords": ["c++23", "deducing this", "crtp", "print", "subscript", "features"],
          "audio_short": "audio/v0112_q04_short.mp3",
          "audio_full": "audio/v0112_q04_full.mp3"
        },
        {
          "q": "What does deducing this replace CRTP for?",
          "a": "CRTP (Curiously Recurring Template Pattern) is used for static polymorphism - the base class calls derived class methods without virtual dispatch. Deducing this achieves the same thing more naturally: void process(this auto& self) lets the compiler resolve the actual type at the call site. The code is shorter, more readable, and produces identical assembly.",
          "a_short": "CRTP provides static polymorphism. Deducing this (void process(this auto& self)) achieves the same more naturally. Shorter, more readable, identical assembly.",
          "keywords": ["deducing this", "crtp", "static polymorphism", "template", "pattern"],
          "audio_short": "audio/v0112_q05_short.mp3",
          "audio_full": "audio/v0112_q05_full.mp3"
        },
        {
          "q": "How does std::print compare to std::format + std::cout?",
          "a": "std::print is essentially std::format piped to stdout in a single call, avoiding the overhead of constructing an intermediate string. For logging, this saves one allocation per log line. More importantly, std::print is atomic with respect to multithreaded output (no interleaving), which operator<< chains are not.",
          "a_short": "Single call to stdout avoiding intermediate string allocation. Atomic multithreaded output (no interleaving), unlike operator<< chains.",
          "keywords": ["print", "format", "cout", "logging", "atomic", "threading"],
          "audio_short": "audio/v0112_q06_short.mp3",
          "audio_full": "audio/v0112_q06_full.mp3"
        },
        {
          "q": "Did ranges adoption change your API surface?",
          "a": "Minimally. Internal implementation uses ranges for clarity, but the public API returns std::span views, not range objects. This keeps the API stable regardless of internal range usage. Users who want ranges can wrap the spans themselves. API stability trumps internal implementation preferences.",
          "a_short": "Minimal. Public API returns std::span, not range objects. Internal uses ranges for clarity. API stability trumps implementation preferences.",
          "keywords": ["api", "ranges", "span", "stability", "public", "interface"],
          "audio_short": "audio/v0112_q07_short.mp3",
          "audio_full": "audio/v0112_q07_full.mp3"
        },
        {
          "q": "What's the compile-time impact of ranges?",
          "a": "Measurable. Range-heavy translation units take ~30% longer to compile due to heavy template instantiation. We mitigate this by keeping range usage in non-header implementation details. Since our library is header-only, we're careful about what ends up in the public headers. Range-based implementations are in .ipp files included only by the main headers.",
          "a_short": "~30% longer for range-heavy TUs. Mitigated by keeping ranges in .ipp implementation files, not public headers.",
          "keywords": ["compile time", "ranges", "template", "instantiation", "ipp", "header"],
          "audio_short": "audio/v0112_q08_short.mp3",
          "audio_full": "audio/v0112_q08_full.mp3"
        },
        {
          "q": "Any gotchas with ranges and std::string_view?",
          "a": "Yes - dangling references. A range pipeline over string_view values doesn't extend the lifetime of the underlying data. If the source buffer is freed while a view pipeline is still alive, you get undefined behavior. We document this clearly: range results from our API are valid only while the source buffer is alive.",
          "a_short": "Dangling references. Range pipelines over string_view don't extend buffer lifetime. Results valid only while source buffer is alive.",
          "keywords": ["dangling", "string_view", "ranges", "lifetime", "gotcha", "ub"],
          "audio_short": "audio/v0112_q09_short.mp3",
          "audio_full": "audio/v0112_q09_full.mp3"
        },
        {
          "q": "What's auto(x) decay-copy and where did you use it?",
          "a": "auto(x) creates a prvalue copy of x, applying array-to-pointer and function-to-pointer decay. It replaces std::decay_t<decltype(x)>(x) in generic code. We use it in template functions that need to capture a decayed copy of a forwarded argument, particularly in our scope guard and callback utilities.",
          "a_short": "Creates a prvalue copy with decay, replacing verbose std::decay_t<decltype(x)>(x). Used in scope guard and callback template functions.",
          "keywords": ["auto", "decay", "copy", "prvalue", "template", "generic"],
          "audio_short": "audio/v0112_q10_short.mp3",
          "audio_full": "audio/v0112_q10_full.mp3"
        }
      ]
    },
    {
      "id": "v0113",
      "label": "v0.1.13 Structural Index",
      "questions": [
        {
          "q": "What does simdjson-style mean in the FIX context?",
          "a": "simdjson's key insight is separating structural character detection (Stage 1) from value interpretation (Stage 2). We apply the same principle to FIX: Stage 1 scans the entire message for SOH delimiters and '=' separators using SIMD, building an index. Stage 2 uses the index for O(1) field access. This decoupling is why we achieve 4.4x improvement - Stage 1 is embarrassingly parallel.",
          "a_short": "Two-stage parsing: Stage 1 SIMD-scans for SOH/'=' building an index, Stage 2 uses the index for O(1) field access. 4.4x improvement from parallelism.",
          "keywords": ["simdjson", "structural index", "two-stage", "simd", "soh", "parsing", "4.4x"],
          "audio_short": "audio/v0113_q01_short.mp3",
          "audio_full": "audio/v0113_q01_full.mp3"
        },
        {
          "q": "4.4x improvement - where exactly does the speedup come from?",
          "a": "Three sources: (1) SIMD-parallel delimiter scanning processes 32 bytes per instruction vs 1 byte (contributing ~2x), (2) O(1) field access eliminates repeated linear scans when accessing multiple fields (contributing ~1.5x), (3) better branch prediction because the access pattern is regular (contributing ~1.5x). The factors multiply rather than add.",
          "a_short": "Three multiplied factors: SIMD 32-byte scanning (~2x), O(1) field access (~1.5x), better branch prediction (~1.5x). Factors multiply, not add.",
          "keywords": ["speedup", "4.4x", "simd", "o1", "branch prediction", "performance"],
          "audio_short": "audio/v0113_q02_short.mp3",
          "audio_full": "audio/v0113_q02_full.mp3"
        },
        {
          "q": "How does the structural index handle variable-length fields?",
          "a": "Each index entry stores (tag_number, value_offset, value_length) as a fixed-size struct. Variable-length fields (like FreeText, tag 58) have the correct length computed during Stage 1 by measuring the distance between the '=' and the next SOH. The index doesn't care about field content - it just records boundaries.",
          "a_short": "Fixed-size index entries store (tag, offset, length). Variable-length fields measured by distance between '=' and next SOH during Stage 1.",
          "keywords": ["variable-length", "fields", "index", "offset", "length", "soh"],
          "audio_short": "audio/v0113_q03_short.mp3",
          "audio_full": "audio/v0113_q03_full.mp3"
        },
        {
          "q": "What's the runtime SIMD dispatch strategy?",
          "a": "On first call, we detect CPU capabilities with __builtin_cpu_supports(\"avx2\") and __builtin_cpu_supports(\"avx512f\"), then set a function pointer to the best available implementation. Subsequent calls go through the function pointer with zero dispatch overhead. This is the same technique used by glibc's memcpy and strlen implementations.",
          "a_short": "First-call CPU detection sets function pointer to best SIMD implementation. Subsequent calls: zero dispatch overhead. Same technique as glibc memcpy.",
          "keywords": ["simd", "dispatch", "runtime", "avx2", "avx512", "cpu detection", "function pointer"],
          "audio_short": "audio/v0113_q04_short.mp3",
          "audio_full": "audio/v0113_q04_full.mp3"
        },
        {
          "q": "Why not always use AVX-512 when available?",
          "a": "AVX-512 can cause frequency throttling on some Intel CPUs (particularly Skylake-X). Processing 64 bytes per iteration is faster per-instruction but slower per-wall-clock-second if the core downclocks by 15-20%. We benchmark on the target hardware and select the optimal path. On Ice Lake and newer, AVX-512 doesn't throttle and is always beneficial.",
          "a_short": "AVX-512 causes frequency throttling on Skylake-X (15-20% downclock). We benchmark target hardware. Ice Lake+ doesn't throttle - always beneficial there.",
          "keywords": ["avx512", "throttling", "frequency", "skylake", "ice lake", "downclock"],
          "audio_short": "audio/v0113_q05_short.mp3",
          "audio_full": "audio/v0113_q05_full.mp3"
        },
        {
          "q": "What's the index memory overhead per message?",
          "a": "MaxFields * sizeof(FieldEntry) = 64 * 6 bytes = 384 bytes per message, stack-allocated. FieldEntry is (uint16_t tag, uint16_t value_offset, uint16_t value_length). This fits in 6 cache lines, which is acceptable for the O(1) access benefit. For messages with fewer fields, the unused portion doesn't affect performance.",
          "a_short": "384 bytes per message (64 fields * 6 bytes), stack-allocated. Fits in 6 cache lines. Acceptable overhead for O(1) field access.",
          "keywords": ["memory", "overhead", "index", "384 bytes", "stack", "cache line"],
          "audio_short": "audio/v0113_q06_short.mp3",
          "audio_full": "audio/v0113_q06_full.mp3"
        },
        {
          "q": "How does 2.3ns per-field extraction compare to the state of the art?",
          "a": "It's competitive with binary protocol decoders. SBE decode is ~3ns per field (fixed offset read). Our 2.3ns includes the index lookup plus string_view construction. The reason we're slightly faster than SBE for extraction is that the index is hot in L1 cache after being built, while SBE fields may span multiple cache lines in the message buffer.",
          "a_short": "2.3ns competitive with SBE's ~3ns. Slightly faster because index is hot in L1 cache after being built, while SBE fields may span cache lines.",
          "keywords": ["2.3ns", "per-field", "state of art", "sbe", "comparison", "cache"],
          "audio_short": "audio/v0113_q07_short.mp3",
          "audio_full": "audio/v0113_q07_full.mp3"
        },
        {
          "q": "Can the structural index be reused across messages?",
          "a": "No, each message gets its own index (stack-allocated). FIX messages have different field counts and orderings, so an index from one message is useless for another. The build cost (~60ns for a typical ExecutionReport) is small enough that reuse isn't worth the complexity of index management.",
          "a_short": "No. Each message gets its own stack-allocated index. Different field counts/orderings make reuse pointless. Build cost ~60ns is small enough.",
          "keywords": ["reuse", "index", "message", "stack", "per-message"],
          "audio_short": "audio/v0113_q08_short.mp3",
          "audio_full": "audio/v0113_q08_full.mp3"
        },
        {
          "q": "How does this compare to a hash map approach for field lookup?",
          "a": "Hash map (std::unordered_map<int, string_view>) has ~25ns per lookup due to hashing and potential collisions. Our index array has ~2.3ns per lookup (direct array access). The hash map also requires heap allocation for the hash table. Our approach sacrifices generality (fixed max fields) for speed and zero allocation.",
          "a_short": "Index array: ~2.3ns (direct access, zero allocation). Hash map: ~25ns (hashing, collisions, heap allocation). 10x faster by sacrificing generality.",
          "keywords": ["hash map", "unordered_map", "comparison", "lookup", "2.3ns", "25ns"],
          "audio_short": "audio/v0113_q09_short.mp3",
          "audio_full": "audio/v0113_q09_full.mp3"
        },
        {
          "q": "What if someone needs random access by tag number rather than field index?",
          "a": "We provide find_by_tag(uint16_t tag) which does a linear scan of the index entries. For typical FIX messages (20-50 fields), this is ~10-20ns - still faster than a hash map. For repeated tag lookups, the caller can cache the index position. We considered building a secondary tag-to-index hash, but the linear scan is fast enough that the additional complexity isn't justified.",
          "a_short": "find_by_tag() does linear scan: ~10-20ns for 20-50 fields, still faster than hash map. Callers can cache positions for repeated lookups.",
          "keywords": ["tag", "random access", "find", "linear scan", "lookup", "cache"],
          "audio_short": "audio/v0113_q10_short.mp3",
          "audio_full": "audio/v0113_q10_full.mp3"
        },
        {
          "q": "How do you handle repeating groups in FIX messages?",
          "a": "Repeating groups appear as consecutive field sequences in the structural index. The count field (e.g., tag 453 NoPartyIDs) tells how many groups follow. Users iterate the index starting from the count field's position, advancing by the group size. The structural index makes this efficient because field positions are pre-computed.",
          "a_short": "Consecutive sequences in the index. Count field (e.g., NoPartyIDs) indicates group count. Iterate from count field position, advancing by group size.",
          "keywords": ["repeating groups", "group", "consecutive", "count", "iterate", "nopartyids"],
          "audio_short": "audio/v0113_q11_short.mp3",
          "audio_full": "audio/v0113_q11_full.mp3"
        },
        {
          "q": "Is the simdjson analogy exact, or are there FIX-specific differences?",
          "a": "Key differences: (1) FIX uses SOH (0x01) as delimiter while JSON uses structural characters ({, }, [, ], :, ,). SOH is simpler to scan for. (2) FIX has no nesting, while JSON has recursive structure. This means our Stage 1 is simpler and faster. (3) FIX field ordering is defined by the protocol, enabling index-based access. JSON keys require string matching. FIX is actually a better fit for the structural index technique than JSON.",
          "a_short": "FIX is simpler than JSON for this technique: single SOH delimiter, no nesting, protocol-defined field ordering. FIX is actually a better fit than JSON.",
          "keywords": ["simdjson", "comparison", "difference", "json", "fix", "soh", "nesting"],
          "audio_short": "audio/v0113_q12_short.mp3",
          "audio_full": "audio/v0113_q12_full.mp3"
        }
      ]
    },
    {
      "id": "v0114",
      "label": "v0.1.14 mimalloc",
      "questions": [
        {
          "q": "What's a per-session heap and why use mimalloc?",
          "a": "A per-session heap gives each FIX session its own memory allocator instance. Allocations from different sessions never contend on the same heap lock. mimalloc (Microsoft's allocator) provides thread-local heap support with excellent performance: ~7ns allocation vs ~25ns for glibc malloc. Per-session heaps eliminate cross-thread allocation contention entirely.",
          "a_short": "Each FIX session gets its own allocator instance. mimalloc provides ~7ns allocation vs ~25ns glibc malloc. Eliminates cross-thread contention entirely.",
          "keywords": ["mimalloc", "per-session", "heap", "allocator", "contention", "thread-local"],
          "audio_short": "audio/v0114_q01_short.mp3",
          "audio_full": "audio/v0114_q01_full.mp3"
        },
        {
          "q": "How does mimalloc compare to jemalloc and tcmalloc?",
          "a": "mimalloc wins on allocation latency (~7ns vs ~12ns jemalloc, ~10ns tcmalloc) and memory overhead (~2% vs ~5% jemalloc). It also has better small allocation performance, which matters for FIX string handling. jemalloc is better for large allocation patterns (multi-MB). For our workload (many small allocations per message), mimalloc is the clear winner.",
          "a_short": "mimalloc: ~7ns, ~2% overhead. jemalloc: ~12ns, ~5% overhead. tcmalloc: ~10ns. mimalloc wins for many small allocations per message.",
          "keywords": ["mimalloc", "jemalloc", "tcmalloc", "comparison", "allocator", "latency"],
          "audio_short": "audio/v0114_q02_short.mp3",
          "audio_full": "audio/v0114_q02_full.mp3"
        },
        {
          "q": "If you already have PMR pools, why do you need mimalloc?",
          "a": "PMR pools cover the hot path (message parsing and processing). mimalloc covers everything else: session management, configuration, logging, connection handling. These off-hot-path allocations still benefit from a fast allocator. PMR + mimalloc is a layered strategy: PMR for the innermost loop, mimalloc for the rest.",
          "a_short": "PMR for hot path (parsing). mimalloc for everything else (session management, config, logging). Layered strategy: PMR innermost, mimalloc outer.",
          "keywords": ["pmr", "mimalloc", "layered", "hot path", "session", "why both"],
          "audio_short": "audio/v0114_q03_short.mp3",
          "audio_full": "audio/v0114_q03_full.mp3"
        },
        {
          "q": "How do you integrate mimalloc without forcing it on library users?",
          "a": "mimalloc is an optional dependency, controlled by a CMake option (NFX_USE_MIMALLOC). When enabled, session objects are constructed with a mimalloc heap. When disabled, the system allocator is used. The API is identical either way - the allocator choice is an internal implementation detail.",
          "a_short": "Optional CMake option NFX_USE_MIMALLOC. When disabled, system allocator used. API identical either way - allocator is an internal detail.",
          "keywords": ["optional", "mimalloc", "cmake", "integration", "dependency", "api"],
          "audio_short": "audio/v0114_q04_short.mp3",
          "audio_full": "audio/v0114_q04_full.mp3"
        },
        {
          "q": "What's the memory fragmentation picture with per-session heaps?",
          "a": "Better than a shared heap. Each session's allocations are locality-grouped, so when a session closes, its entire heap can be freed without fragmentation. With a shared heap, interleaved allocations from different sessions create fragmentation that persists until all sessions close. Per-session heaps provide natural memory locality.",
          "a_short": "Better than shared heap. Session close frees entire heap without fragmentation. Shared heaps suffer from interleaved allocation fragmentation.",
          "keywords": ["fragmentation", "memory", "per-session", "locality", "heap", "free"],
          "audio_short": "audio/v0114_q05_short.mp3",
          "audio_full": "audio/v0114_q05_full.mp3"
        },
        {
          "q": "How do you handle memory when a session disconnects and reconnects?",
          "a": "The session heap is destroyed on disconnect and a fresh one is created on reconnect. This is a clean-slate approach that prevents memory growth over long-running sessions. Any state that must survive reconnection (sequence numbers, replay buffer) is stored outside the session heap.",
          "a_short": "Heap destroyed on disconnect, fresh one on reconnect. Clean-slate prevents memory growth. Surviving state stored outside the session heap.",
          "keywords": ["disconnect", "reconnect", "session", "heap", "destroy", "clean slate"],
          "audio_short": "audio/v0114_q06_short.mp3",
          "audio_full": "audio/v0114_q06_full.mp3"
        },
        {
          "q": "Does mimalloc work with PMR?",
          "a": "Yes. We implement a custom std::pmr::memory_resource that delegates to a mimalloc heap. This means PMR pools within a session are backed by that session's mimalloc heap. The layering is: PMR monotonic buffer -> mimalloc heap -> OS memory. Each layer adds zero contention because the heap is session-private.",
          "a_short": "Yes. Custom PMR memory_resource delegates to mimalloc heap. Layering: PMR buffer -> mimalloc heap -> OS memory. Zero contention, session-private.",
          "keywords": ["mimalloc", "pmr", "memory_resource", "layering", "integration"],
          "audio_short": "audio/v0114_q07_short.mp3",
          "audio_full": "audio/v0114_q07_full.mp3"
        },
        {
          "q": "What's the benchmark methodology for allocator comparison?",
          "a": "We allocate and free 100K objects of varying sizes (16B to 4KB) in patterns that simulate FIX message processing: burst allocation during message arrival, batch deallocation at session checkpoints. We measure P50 and P99 allocation latency, peak RSS, and fragmentation ratio (peak RSS / useful bytes). mimalloc wins on all metrics.",
          "a_short": "100K objects (16B-4KB) in FIX-like patterns: burst allocate, batch deallocate. Measure P50/P99 latency, peak RSS, fragmentation. mimalloc wins all metrics.",
          "keywords": ["benchmark", "allocator", "methodology", "rss", "fragmentation", "comparison"],
          "audio_short": "audio/v0114_q08_short.mp3",
          "audio_full": "audio/v0114_q08_full.mp3"
        },
        {
          "q": "Any downsides to per-session heaps?",
          "a": "Memory overhead. Each heap has its own page cache and metadata, typically 64KB-256KB per session. With 100 sessions, that's 6-25MB of overhead. For trading systems with 10-50 sessions, this is negligible. For systems with thousands of sessions, a shared allocator with thread-local caching (like tcmalloc) might be more memory-efficient.",
          "a_short": "64KB-256KB overhead per session. Negligible for 10-50 sessions. For thousands of sessions, shared allocator with thread-local caching may be better.",
          "keywords": ["downside", "overhead", "per-session", "memory", "scalability"],
          "audio_short": "audio/v0114_q09_short.mp3",
          "audio_full": "audio/v0114_q09_full.mp3"
        },
        {
          "q": "How do you prevent memory leaks in session heaps?",
          "a": "The session owns its heap via RAII. When the session destructor runs, the heap is destroyed, freeing all memory unconditionally. There are no per-object destructors to miss - the entire heap is returned to the OS. This is similar to arena allocation: bulk free instead of individual free. Leak detection (ASan) is used in debug builds to catch logic errors.",
          "a_short": "RAII ownership. Session destructor destroys entire heap unconditionally - bulk free like arena allocation. ASan in debug builds catches logic errors.",
          "keywords": ["leak", "raii", "destructor", "arena", "bulk free", "asan"],
          "audio_short": "audio/v0114_q10_short.mp3",
          "audio_full": "audio/v0114_q10_full.mp3"
        }
      ]
    },
    {
      "id": "v0115",
      "label": "v0.1.15 xsimd SIMD",
      "questions": [
        {
          "q": "Why xsimd instead of writing intrinsics directly?",
          "a": "Direct intrinsics are architecture-specific: AVX2 code won't compile on ARM. xsimd provides a portable API that maps to the best available instruction set at compile time. One codebase targets SSE4.2, AVX2, AVX-512, and NEON. We still have hand-tuned intrinsics for the absolute hot path, but xsimd handles the 80% case with 95% of the performance.",
          "a_short": "Portable API across SSE4.2, AVX2, AVX-512, and NEON from one codebase. xsimd handles 80% of cases at 95% performance. Hand-tuned intrinsics for the absolute hot path.",
          "keywords": ["xsimd", "intrinsics", "portable", "simd", "avx2", "neon", "arm"],
          "audio_short": "audio/v0115_q01_short.mp3",
          "audio_full": "audio/v0115_q01_full.mp3"
        },
        {
          "q": "What's the performance overhead of the abstraction layer?",
          "a": "Near zero. xsimd is a header-only C++ template library that compiles to the same instructions as hand-written intrinsics. We benchmarked our SOH scanner: hand-written AVX2 at 10.6ns vs xsimd AVX2 at 10.9ns for 256 bytes. The 3% overhead comes from slightly different instruction scheduling, not from abstraction penalty.",
          "a_short": "Near zero. SOH scanner: hand-written 10.6ns vs xsimd 10.9ns (3% overhead from instruction scheduling, not abstraction).",
          "keywords": ["overhead", "xsimd", "performance", "abstraction", "benchmark", "3%"],
          "audio_short": "audio/v0115_q02_short.mp3",
          "audio_full": "audio/v0115_q02_full.mp3"
        },
        {
          "q": "How does xsimd compare to Highway (Google's SIMD library)?",
          "a": "Both provide portable SIMD abstractions. xsimd uses expression templates and C++ operator overloading for a natural syntax. Highway uses a more explicit dispatch model with tag types. We chose xsimd because the syntax is closer to scalar C++ code, making it easier for team members who aren't SIMD experts to read and maintain. Performance is comparable between the two.",
          "a_short": "Both portable. xsimd uses operator overloading (natural syntax). Highway uses tag types (explicit dispatch). xsimd chosen for readability. Comparable performance.",
          "keywords": ["xsimd", "highway", "google", "comparison", "simd", "library"],
          "audio_short": "audio/v0115_q03_short.mp3",
          "audio_full": "audio/v0115_q03_full.mp3"
        },
        {
          "q": "Does xsimd support runtime dispatch?",
          "a": "xsimd itself is compile-time dispatch - you compile for a specific architecture. We handle runtime dispatch ourselves using the same function-pointer technique from v0.1.13. At startup, we detect the CPU and select the xsimd-compiled variant. This gives us both portability (xsimd) and runtime flexibility (our dispatch layer).",
          "a_short": "xsimd is compile-time only. We add runtime dispatch via function pointers (same technique as v0.1.13). Combines xsimd portability with runtime flexibility.",
          "keywords": ["runtime", "dispatch", "xsimd", "compile-time", "function pointer", "cpu detection"],
          "audio_short": "audio/v0115_q04_short.mp3",
          "audio_full": "audio/v0115_q04_full.mp3"
        },
        {
          "q": "What about ARM NEON support for Apple Silicon?",
          "a": "xsimd supports NEON, so our xsimd-based code compiles and runs on Apple Silicon (M1/M2/M3). Performance is different - NEON processes 16 bytes per iteration vs AVX2's 32 bytes - but the algorithm is identical. On M2, our SOH scanner achieves ~6x speedup over scalar, vs ~12x with AVX2 on x86. ARM's memory subsystem partially compensates for the narrower SIMD width.",
          "a_short": "xsimd supports NEON for Apple Silicon. ~6x speedup on M2 vs ~12x AVX2 on x86. Same algorithm, ARM memory subsystem partially compensates for narrower SIMD.",
          "keywords": ["arm", "neon", "apple silicon", "m1", "m2", "m3", "aarch64"],
          "audio_short": "audio/v0115_q05_short.mp3",
          "audio_full": "audio/v0115_q05_full.mp3"
        },
        {
          "q": "When do you use raw intrinsics vs xsimd?",
          "a": "Raw intrinsics for the innermost SOH scanning loop where we've hand-tuned instruction scheduling. xsimd for everything else: checksum computation, field value conversion (atoi SIMD), batch string comparison. The rule is: if we've verified the hand-tuned version is measurably faster, we keep intrinsics. If xsimd matches or is within 5%, we prefer xsimd for maintainability.",
          "a_short": "Raw intrinsics for hand-tuned SOH scanning. xsimd for checksum, atoi, string comparison. Rule: keep intrinsics only if measurably faster; prefer xsimd within 5%.",
          "keywords": ["intrinsics", "xsimd", "when", "rule", "hand-tuned", "maintainability"],
          "audio_short": "audio/v0115_q06_short.mp3",
          "audio_full": "audio/v0115_q06_full.mp3"
        },
        {
          "q": "How do you test SIMD code portability?",
          "a": "Our CI matrix includes x86-64 (Linux/Windows, testing SSE4.2 and AVX2) and ARM64 (macOS, testing NEON). We also compile with SIMD disabled to verify the scalar fallback works. Each SIMD function has a scalar reference implementation, and tests verify identical results between SIMD and scalar for all inputs.",
          "a_short": "CI matrix: x86-64 (SSE4.2, AVX2) and ARM64 (NEON). SIMD-disabled builds verify scalar fallback. Tests verify SIMD matches scalar for all inputs.",
          "keywords": ["portability", "testing", "ci", "x86", "arm64", "scalar", "fallback"],
          "audio_short": "audio/v0115_q07_short.mp3",
          "audio_full": "audio/v0115_q07_full.mp3"
        },
        {
          "q": "What was the most challenging part of the xsimd migration?",
          "a": "Mask handling. AVX2 produces 32-bit masks via _mm256_movemask_epi8. xsimd uses a batch_bool type that abstracts mask operations. Converting our bit-manipulation tricks (mask &= mask - 1 for popcnt iteration) to xsimd's API required understanding how batch_bool maps to native masks on each architecture.",
          "a_short": "Mask handling. Converting bit-manipulation tricks (mask &= mask - 1) to xsimd's batch_bool required understanding native mask mapping per architecture.",
          "keywords": ["challenge", "xsimd", "mask", "batch_bool", "migration", "bit manipulation"],
          "audio_short": "audio/v0115_q08_short.mp3",
          "audio_full": "audio/v0115_q08_full.mp3"
        },
        {
          "q": "Does xsimd support AVX-512 mask registers?",
          "a": "Yes. xsimd maps to AVX-512 opmask registers (__mmask64) on supported architectures. This is where AVX-512 has a genuine architectural advantage over AVX2: dedicated mask registers instead of general-purpose register masks. xsimd abstracts this, but the performance benefit is real - ~10% faster for mask-heavy code like delimiter scanning.",
          "a_short": "Yes. Maps to AVX-512 opmask registers (__mmask64). Dedicated mask registers give ~10% advantage over AVX2 for mask-heavy delimiter scanning.",
          "keywords": ["avx512", "mask", "opmask", "registers", "xsimd", "performance"],
          "audio_short": "audio/v0115_q09_short.mp3",
          "audio_full": "audio/v0115_q09_full.mp3"
        },
        {
          "q": "What's the future plan for SIMD in NexusFix?",
          "a": "Three areas: (1) SIMD-accelerated integer parsing (atoi for FIX numeric fields), which can process 8 digits per instruction. (2) SIMD string comparison for tag matching, processing 32 bytes per compare. (3) Exploring SVE (Scalable Vector Extensions) on ARM for variable-width SIMD. xsimd doesn't support SVE yet, so this may require Highway or hand-written intrinsics.",
          "a_short": "Three areas: SIMD atoi (8 digits/instruction), SIMD string comparison (32 bytes/compare), and ARM SVE exploration (may need Highway).",
          "keywords": ["future", "simd", "atoi", "sve", "arm", "plan", "roadmap"],
          "audio_short": "audio/v0115_q10_short.mp3",
          "audio_full": "audio/v0115_q10_full.mp3"
        }
      ]
    }
  ]
};
