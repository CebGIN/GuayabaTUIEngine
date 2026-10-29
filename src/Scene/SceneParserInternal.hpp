#ifndef GUAYABA_SCENE_SCENEPARSERINTERNAL_HPP
#define GUAYABA_SCENE_SCENEPARSERINTERNAL_HPP

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <guayaba/Scene/SceneFormat.hpp>
#include <guayaba/Scene/SceneParser.hpp>
#include "SceneLexer.hpp"

namespace guayaba::detail {

/*
    Small recursive-descent core shared by the scene and theme parsers. It owns the
    token stream and the syntax diagnostics; the two format parsers only describe
    their top-level structure.
*/
class ParserCore {
public:
    ParserCore(SceneLexResult lexed, SceneParseOptions options);

    const SceneToken& peek(std::size_t ahead = 0) const;
    const SceneToken& advance();
    bool check(SceneTokenKind kind) const;
    bool match(SceneTokenKind kind);
    bool at_end() const;

    bool expect(SceneTokenKind kind, std::string message, SceneToken* token = nullptr);

    SceneValue parse_value();
    std::string parse_key(SceneSourceSpan* span = nullptr);

    // Recovery at a property/node boundary: stop at the closing brace or at an
    // identifier on a later line. The caller decides whether to consume it.
    void recover_statement();

    // Current token must be LeftBrace. Consumes the whole block, including nested
    // braces, so a malformed node does not cascade into its siblings.
    void skip_balanced_block();

    void error(SceneDiagnosticCode code, SceneSourceSpan span, std::string message);

    const std::string& source() const { return source_; }
    const SceneParseOptions& options() const { return options_; }
    std::vector<SceneDiagnostic>& diagnostics() { return diagnostics_; }
    std::vector<SceneDiagnostic> take_diagnostics() { return std::move(diagnostics_); }

    std::size_t position() const { return index_; }
    void seek(std::size_t position) { index_ = position; }

private:
    SceneValue parse_value_impl(int depth);
    SceneValue parse_array(int depth);
    SceneValue parse_table(int depth);

    std::vector<SceneToken> tokens_;
    std::size_t index_ = 0;
    std::string source_;
    SceneParseOptions options_;
    std::vector<SceneDiagnostic> diagnostics_;
};

} // namespace guayaba::detail

#endif // GUAYABA_SCENE_SCENEPARSERINTERNAL_HPP
