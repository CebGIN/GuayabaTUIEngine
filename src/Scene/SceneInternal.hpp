#ifndef GUAYABA_SCENE_SCENEINTERNAL_HPP
#define GUAYABA_SCENE_SCENEINTERNAL_HPP

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <guayaba/Nodes/Node.hpp>
#include <guayaba/Resources/Theme.hpp>
#include <guayaba/Scene/SceneFormat.hpp>
#include <guayaba/Scene/SceneSchema.hpp>

namespace guayaba::detail {

/*
    Where value readers put their diagnostics. The node path is prefixed to
    semantic messages, so an editor can tell which node rejected a property even
    when the file has several nodes with the same property name.
*/
struct SceneDiagnosticSink {
    std::string source;
    std::string node_path;
    std::vector<SceneDiagnostic>* diagnostics = nullptr;

    void error(SceneDiagnosticCode code, SceneSourceSpan span, std::string message);
    void warning(SceneDiagnosticCode code, SceneSourceSpan span, std::string message);
};

struct BuildContext {
    SceneDiagnosticSink sink;
    const Theme* theme = nullptr;
    bool apply_theme = true;
};

using ScenePropertySetter =
    std::function<bool(Node&, const SceneValue&, BuildContext&)>;

struct ScenePropertyBinding {
    ScenePropertyInfo info;
    ScenePropertySetter setter;
};

struct SceneTypeBinding {
    SceneTypeInfo info;
    std::function<std::unique_ptr<Node>(const std::string&)> factory;
    std::unordered_map<std::string, ScenePropertyBinding> properties;
};

class SceneSchemaRegistry {
public:
    static const SceneSchemaRegistry& instance();

    const SceneTypeBinding* find(const std::string& name) const;
    const std::vector<SceneTypeBinding>& types() const { return types_; }

private:
    SceneSchemaRegistry();

    void add_type(SceneTypeInfo info,
                  std::function<std::unique_ptr<Node>(const std::string&)> factory,
                  std::vector<ScenePropertyBinding> properties,
                  const std::vector<std::string>& hidden = {});

    std::vector<SceneTypeBinding> types_;
};

} // namespace guayaba::detail

#endif // GUAYABA_SCENE_SCENEINTERNAL_HPP
