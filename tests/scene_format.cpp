// Headless checks for the scene format: parse, validate, build, theme, canonical
// writing and the editor-facing schema.

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <guayaba/Guayaba.hpp>

using namespace guayaba;

namespace {

const char* const SCENE = R"(format = 1
name = "Main"

VBox "Root" {
    anchors = "full_rect"
    separation = 1

    Label "Title" {
        text = "Hola, Guayaba"
        fit_content = true
        text_color = "BRIGHT_WHITE"
    }

    PanelContainer "Body" {
        anchors = "full_rect"
        style = "panel"

        ListView "List" {
            anchors = "full_rect"
            items = ["Uno", "Dos", "Tres"]
            selected = 1
        }
    }

    HBox "Actions" {
        separation = 2

        Button "Accept" {
            text = "Aceptar"
            style.normal = "button.primary"
        }
    }
}
)";

const char* const THEME = R"(format = 1
name = "Default"

style "panel" {
    background = "BLUE"
    border = "WHITE"
    border_style = "ascii"
    border_width = 1
    margin = [1, 0, 1, 0]
}

style "label" {
    background = "BLACK"
}

style "button.normal" {
    background = "GREEN"
}

style "button.primary" {
    background = "MAGENTA"
    border = "BRIGHT_WHITE"
    border_width = 1
}

style "spin_box" {
    margin = [0, 0, 0, 0]
}

color "text.primary" = "YELLOW"
color "text.secondary" = "BRIGHT_BLACK"
)";

void test_parse_and_schema() {
    SceneParseResult parsed = parse_scene(SCENE, SceneParseOptions{"main.gscene"});
    if (!parsed.ok()) {
        for (const SceneDiagnostic& diagnostic : parsed.diagnostics) {
            std::cerr << diagnostic.format() << "\n";
        }
    }
    assert(parsed.ok());
    assert(parsed.document.format_version == 1);
    assert(parsed.document.name == "Main");
    assert(parsed.document.root != nullptr);
    assert(parsed.document.root->type == "VBox");
    assert(parsed.document.root->name == "Root");
    assert(parsed.document.root->children.size() == 3);
    assert(parsed.document.root->children[0]->type == "Label");
    assert(parsed.document.root->children[1]->type == "PanelContainer");
    assert(parsed.document.root->children[2]->type == "HBox");

    const SceneSchema& schema = SceneSchema::builtins();
    assert(schema.find_type("Button") != nullptr);
    assert(schema.find_type("NoSuchThing") == nullptr);
    assert(schema.find_property("SpinBox", "value") != nullptr);
    assert(schema.find_property("SpinBox", "text") == nullptr);
    assert(schema.find_property("SpinBox", "caret") == nullptr);
    assert(schema.find_property("PanelContainer", "content_alignment") == nullptr);
    assert(schema.find_property("ScrollView", "content_alignment") == nullptr);
    assert(schema.find_property("MarginContainer", "content_alignment") == nullptr);
    assert(schema.find_property("SplitContainer", "content_alignment") == nullptr);
    assert(schema.find_property("Button", "focus_mode") != nullptr);
    assert(schema.find_property("CheckBox", "focus_mode") != nullptr);
    assert(schema.find_property("ListView", "focus_mode") != nullptr);
    assert(schema.find_property("LineEdit", "focus_mode") != nullptr);
    assert(schema.find_property("Label", "visible") != nullptr);
    assert(schema.find_property("Label", "anchors") != nullptr);
    assert(schema.find_property("Label", "position") != nullptr);
    assert(schema.find_property("Label", "text") != nullptr);
    assert(schema.types().size() == 22);
    assert(schema.find_property("Button", "focus_mode")->default_value ==
           SceneValue::string("click"));
    assert(schema.find_property("ProgressBar", "range")->default_value ==
           (SceneValue::array({SceneValue::number(0.0), SceneValue::number(100.0)})));
}

void test_build_with_theme() {
    SceneParseResult parsed = parse_scene(SCENE, SceneParseOptions{"main.gscene"});
    assert(parsed.ok());

    ThemeParseResult parsed_theme = parse_theme(THEME, SceneParseOptions{"default.gtheme"});
    assert(parsed_theme.ok());
    ThemeBuildResult built_theme = build_theme(parsed_theme.document);
    assert(built_theme.ok());

    SceneBuildResult built = build_scene(parsed.document, built_theme.theme.get());
    assert(built.ok());
    assert(built.root != nullptr);
    assert(built.root->get_name() == "Root");

    VBox* root = dynamic_cast<VBox*>(built.root.get());
    assert(root != nullptr);
    assert(root->get_grow_x() == Grow::Both);
    assert(root->get_grow_y() == Grow::Both);
    assert(root->get_separation() == 1);

    Label* title = dynamic_cast<Label*>(built.root->get_node<Label>("Title").get());
    assert(title != nullptr);
    assert(title->get_text() == "Hola, Guayaba");
    assert(title->get_fit_content());
    assert(title->get_stylebox().has_background());
    assert(title->get_stylebox().get_background_color() == 0);

    PanelContainer* body =
        dynamic_cast<PanelContainer*>(built.root->get_node<PanelContainer>("Body").get());
    assert(body != nullptr);
    assert(body->get_stylebox().get_background_color() == 4);

    ListView* list =
        dynamic_cast<ListView*>(built.root->get_node<ListView>("Body/List").get());
    assert(list != nullptr);
    assert(list->get_items().size() == 3);
    assert(list->selected() == 1);

    Button* button =
        dynamic_cast<Button*>(built.root->get_node<Button>("Actions/Accept").get());
    assert(button != nullptr);
    assert(button->get_stylebox(ButtonState::Normal).get_background_color() == 5);
}

void test_spinbox_and_hidden_properties() {
    const char* scene = R"(format = 1
SpinBox "Spin" {
    range = [10, 20]
    decimals = 1
    value = 12.3
    style = "spin_box"
}
)";
    SceneParseResult parsed = parse_scene(scene, SceneParseOptions{"spin.gscene"});
    assert(parsed.ok());

    ThemeParseResult parsed_theme = parse_theme(THEME, SceneParseOptions{"default.gtheme"});
    ThemeBuildResult built_theme = build_theme(parsed_theme.document);
    SceneBuildResult built = build_scene(parsed.document, built_theme.theme.get());
    assert(built.ok());

    SpinBox* spin = dynamic_cast<SpinBox*>(built.root.get());
    assert(spin != nullptr);
    assert(spin->get_min() == 10.0);
    assert(spin->get_max() == 20.0);
    assert(spin->get_value() == 12.3);
    assert(spin->get_text() == "12.3");
    // apply_theme reserves two columns for the arrows; the explicit style must
    // keep that reservation.
    assert(spin->get_stylebox().get_content_margin().right == 2);

    SceneParseResult hidden = parse_scene(R"(format = 1
SpinBox "Spin" {
    text = "nope"
}
)", SceneParseOptions{"hidden.gscene"});
    assert(hidden.ok()); // syntax is fine; schema validation rejects it
    SceneBuildResult hidden_built = build_scene(hidden.document, nullptr);
    assert(!hidden_built.ok());
    assert(!hidden_built.diagnostics.empty());
}

void test_validation_errors() {
    SceneParseResult duplicate = parse_scene(R"(format = 1
VBox "Root" {
    Label "A" {}
    Label "A" {}
}
)", SceneParseOptions{"duplicate.gscene"});
    assert(!duplicate.ok());
    assert(!duplicate.diagnostics.empty());

    SceneParseResult bad_value = parse_scene(R"(format = 1
VBox "Root" {
    separation = "two"
}
)", SceneParseOptions{"bad.gscene"});
    assert(bad_value.ok());
    SceneBuildResult built = build_scene(bad_value.document, nullptr);
    assert(!built.ok());
    assert(!built.diagnostics.empty());

    SceneParseResult bad_color = parse_scene(R"(format = 1
Label "Root" {
    text_color = "NOT_A_COLOR"
}
)", SceneParseOptions{"color.gscene"});
    assert(bad_color.ok());
    SceneBuildResult color_built = build_scene(bad_color.document, nullptr);
    assert(!color_built.ok());

    SceneParseResult no_format = parse_scene("VBox \"Root\" {}\n",
                                             SceneParseOptions{"noformat.gscene"});
    assert(!no_format.ok());
}

void test_anchor_rules() {
    SceneParseResult conflict = parse_scene(R"(format = 1
Label "Root" {
    anchors = "full_rect"
    anchor_left = 0.0
}
)", SceneParseOptions{"anchors.gscene"});
    assert(conflict.ok());
    SceneBuildResult built = build_scene(conflict.document, nullptr);
    assert(!built.ok());

    SceneParseResult custom = parse_scene(R"(format = 1
Label "Root" {
    anchors = "full_rect"
    grow_x = "none"
}
)", SceneParseOptions{"grow.gscene"});
    assert(custom.ok());
    SceneBuildResult custom_built = build_scene(custom.document, nullptr);
    assert(custom_built.ok());
    Control* control = dynamic_cast<Control*>(custom_built.root.get());
    assert(control != nullptr);
    assert(control->get_grow_x() == Grow::None);
    assert(control->get_grow_y() == Grow::Both);
}

void test_writer_round_trip() {
    SceneParseResult parsed = parse_scene(SCENE, SceneParseOptions{"main.gscene"});
    assert(parsed.ok());

    const std::string written = write_scene(parsed.document);
    assert(written.find("format = 1") == 0);
    assert(written.find("#") == std::string::npos);

    SceneParseResult reparsed = parse_scene(written, SceneParseOptions{"roundtrip.gscene"});
    assert(reparsed.ok());
    assert(reparsed.document.root->type == parsed.document.root->type);
    assert(reparsed.document.root->children.size() == parsed.document.root->children.size());

    const std::string second = write_scene(reparsed.document);
    assert(written == second);
}

void test_writer_anchors_and_defaults() {
    SceneParseResult parsed = parse_scene(R"(format = 1
Label "Root" {
    anchors = "full_rect"
    grow_x = "none"
    visible = true
}
)", SceneParseOptions{"write.gscene"});
    assert(parsed.ok());

    const std::string written = write_scene(parsed.document);
    assert(written.find("anchors = \"full_rect\"") != std::string::npos);
    assert(written.find("grow_x = \"none\"") != std::string::npos);
    assert(written.find("visible = true") != std::string::npos);

    SceneParseResult reparsed = parse_scene(written, SceneParseOptions{"write2.gscene"});
    assert(reparsed.ok());
    SceneBuildResult built = build_scene(reparsed.document, nullptr);
    assert(built.ok());
    Control* control = dynamic_cast<Control*>(built.root.get());
    assert(control != nullptr);
    assert(control->get_grow_x() == Grow::None);
    assert(control->get_grow_y() == Grow::Both);
}

void test_theme_writer() {
    ThemeParseResult parsed = parse_theme(THEME, SceneParseOptions{"default.gtheme"});
    assert(parsed.ok());
    const std::string written = write_theme(parsed.document);
    assert(written.find("format = 1") == 0);
    assert(written.find("color \"text.primary\" = \"YELLOW\"") != std::string::npos);
    assert(written.find("border_style = \"ascii\"") != std::string::npos);

    ThemeParseResult reparsed = parse_theme(written, SceneParseOptions{"theme2.gtheme"});
    assert(reparsed.ok());
    assert(write_theme(reparsed.document) == written);
}

void test_example_round_trip_can_build() {
    SceneParseResult parsed = parse_scene(SCENE, SceneParseOptions{"main.gscene"});
    assert(parsed.ok());
    const std::string written = write_scene(parsed.document);
    SceneParseResult reparsed = parse_scene(written, SceneParseOptions{"main2.gscene"});
    assert(reparsed.ok());

    ThemeParseResult parsed_theme = parse_theme(THEME, SceneParseOptions{"default.gtheme"});
    ThemeBuildResult built_theme = build_theme(parsed_theme.document);
    assert(built_theme.ok());
    SceneBuildResult built = build_scene(reparsed.document, built_theme.theme.get());
    if (!built.ok()) {
        for (const SceneDiagnostic& diagnostic : built.diagnostics) {
            std::cerr << diagnostic.format() << "\n";
        }
    }
    assert(built.ok());
}


void test_every_type_builds_by_default() {
    const SceneSchema& schema = SceneSchema::builtins();
    assert(!schema.types().empty());

    for (const SceneTypeInfo& type : schema.types()) {
        SceneDocument document;
        document.root = std::make_unique<SceneNodeDesc>();
        document.root->type = type.name;
        document.root->name = "Root";

        SceneBuildResult built = build_scene(document, nullptr);
        if (!built.ok()) {
            std::cerr << "default build failed for " << type.name << "\n";
            for (const SceneDiagnostic& diagnostic : built.diagnostics) {
                std::cerr << diagnostic.format() << "\n";
            }
        }
        assert(built.ok());
    }
}

void test_default_build_values() {
    const SceneSchema& schema = SceneSchema::builtins();
    for (const SceneTypeInfo& type : schema.types()) {
        SceneDocument document;
        document.root = std::make_unique<SceneNodeDesc>();
        document.root->type = type.name;
        document.root->name = "Root";
        SceneBuildResult built = build_scene(document, nullptr);
        assert(built.ok());

        if (type.name == "Button") {
            Button* button = dynamic_cast<Button*>(built.root.get());
            assert(button != nullptr);
            assert(button->get_focus_mode() == Control::FocusMode::CLICK);
            assert(!button->is_disabled());
        } else if (type.name == "CheckBox") {
            CheckBox* box = dynamic_cast<CheckBox*>(built.root.get());
            assert(box != nullptr);
            assert(!box->is_checked());
            assert(box->get_focus_mode() == Control::FocusMode::CLICK);
        } else if (type.name == "LineEdit") {
            LineEdit* edit = dynamic_cast<LineEdit*>(built.root.get());
            assert(edit != nullptr);
            assert(edit->get_text().empty());
            assert(edit->get_caret() == 0);
            assert(edit->get_focus_mode() == Control::FocusMode::CLICK);
        } else if (type.name == "SpinBox") {
            SpinBox* spin = dynamic_cast<SpinBox*>(built.root.get());
            assert(spin != nullptr);
            assert(spin->get_min() == 0.0);
            assert(spin->get_max() == 100.0);
            assert(spin->get_value() == 0.0);
            assert(spin->get_step() == 1.0);
            assert(spin->get_decimals() == 0);
            assert(spin->get_focus_mode() == Control::FocusMode::CLICK);
            assert(spin->get_stylebox().get_content_margin().right == 2);
        } else if (type.name == "ListView") {
            ListView* list = dynamic_cast<ListView*>(built.root.get());
            assert(list != nullptr);
            assert(list->get_items().empty());
            assert(list->selected() == -1);
            assert(list->get_focus_mode() == Control::FocusMode::CLICK);
        } else if (type.name == "ProgressBar") {
            ProgressBar* bar = dynamic_cast<ProgressBar*>(built.root.get());
            assert(bar != nullptr);
            assert(bar->get_min() == 0.0);
            assert(bar->get_max() == 100.0);
            assert(bar->get_value() == 0.0);
            assert(!bar->get_show_percentage());
        } else if (type.name == "Spinner") {
            Spinner* spinner = dynamic_cast<Spinner*>(built.root.get());
            assert(spinner != nullptr);
            assert(spinner->get_frames().size() == 4);
            assert(spinner->get_interval() == 0.1);
            assert(spinner->is_running());
        } else if (type.name == "Graph") {
            Graph* graph = dynamic_cast<Graph*>(built.root.get());
            assert(graph != nullptr);
            assert(graph->get_values().empty());
            assert(!graph->has_range());
            assert(graph->is_fill());
        } else if (type.name == "Timer") {
            Timer* timer = dynamic_cast<Timer*>(built.root.get());
            assert(timer != nullptr);
            assert(timer->get_wait_time() == 1.0);
            assert(!timer->is_one_shot());
            assert(!timer->get_autostart());
        }
    }
}

void test_all_properties_scene() {
    const char* scene = R"(format = 1
Node "Root" {
    visible = true

    Node2D "N2" {
        position = [1, 2]
    }

    Control "C" {
        anchor_left = 0.0
        anchor_right = 0.5
        anchor_top = 0.0
        anchor_bottom = 0.5
        size = [4, 2]
        custom_minimum_size = [1, 1]
        grow_x = "end"
        grow_y = "begin"
        clip_children = true
        focus_mode = "all"
    }

    Container "Cont" {
        content_alignment = "end"
    }

    HBox "H" {
        separation = 2
    }

    VBox "V" {
        separation = 3
    }

    GridContainer "Grid" {
        columns = 2
        h_separation = 1
        v_separation = 1
    }

    MarginContainer "Margin" {
        margins = [1, 2, 3, 4]
    }

    PanelContainer "Panel" {
        style = { background = "RED", border = "WHITE", border_style = "ascii", border_width = 1, corner_radius = 0, margin = [0, 0, 0, 0] }
    }

    ScrollView "Scroll" {
        style = { background = "BLACK" }
        track_style = { background = "BRIGHT_BLACK" }
        handle_style = { background = "WHITE" }
    }

    SplitContainer "Split" {
        orientation = "horizontal_divider"
        split_ratio = 0.25
        min_first = 2
        min_second = 3
        divider_width = 2
        divider_style = { background = "CYAN" }
    }

    Label "Label" {
        text = "line one\nline two"
        fit_content = true
        text_color = "BLUE"
        background_color = "BLACK"
        style = { border = "WHITE", border_width = 1 }
    }

    Button "Button" {
        text = "OK"
        text_align = "center"
        disabled = false
        text_color.normal = "WHITE"
        text_color.hover = "YELLOW"
        text_color.pressed = "GREEN"
        text_color.disabled = "BRIGHT_BLACK"
        style.normal = { background = "BLUE" }
        style.hover = { background = "CYAN" }
        style.pressed = { background = "GREEN" }
        style.disabled = { background = "BLACK" }
    }

    CheckBox "Check" {
        text = "Enable"
        checked = true
        glyphs = ["x", " "]
        text_color = "WHITE"
        background_color = "BLACK"
        style = { border = "WHITE", border_width = 1 }
    }

    LineEdit "Edit" {
        text = "hello"
        placeholder = "name"
        max_length = 8
        caret = 2
        secret = true
        secret_character = "*"
        text_color = "WHITE"
        placeholder_color = "BRIGHT_BLACK"
        background_color = "BLACK"
        style = { border = "WHITE", border_width = 1 }
    }

    SpinBox "Spin" {
        range = [1, 9]
        step = 0.5
        decimals = 1
        value = 2.5
        wrap = true
        placeholder = "number"
        style = { border = "WHITE", border_width = 1 }
    }

    ListView "List" {
        items = ["a", "b", "c"]
        selected = 1
        item_colors = [[0, "RED"], [2, "BLUE"]]
        text_color = "WHITE"
        selection_background_color = "BLUE"
        selection_text_color = "WHITE"
        style = { border = "WHITE", border_width = 1 }
    }

    ProgressBar "Progress" {
        range = [0, 10]
        value = 4
        indeterminate = false
        text = "loading"
        show_percentage = true
        fill_color = "GREEN"
        background_color = "BLACK"
        text_color = "WHITE"
        style = { border = "WHITE", border_width = 1 }
    }

    Spinner "Spinner" {
        frames = ["|", "/", "-", "\\"]
        interval = 0.2
        running = false
        text_color = "WHITE"
        background_color = "BLACK"
        style = { border = "WHITE", border_width = 1 }
    }

    Graph "Graph" {
        values = [1, 2.5, 3]
        capacity = 8
        range = [0, 10]
        fill = false
        line_color = "GREEN"
        background_color = "BLACK"
    }

    Timer "Timer" {
        wait_time = 0.5
        one_shot = true
        autostart = false
    }

    NodeTextMatrix "Matrix" {
        position = [2, 3]
        text = ["abc", "def"]
        text_color = "WHITE"
        background_color = "BLACK"
    }
}
)";

    SceneParseResult parsed = parse_scene(scene, SceneParseOptions{"all.gscene"});
    if (!parsed.ok()) {
        for (const SceneDiagnostic& diagnostic : parsed.diagnostics) {
            std::cerr << diagnostic.format() << "\n";
        }
    }
    assert(parsed.ok());

    SceneBuildResult built = build_scene(parsed.document, nullptr);
    if (!built.ok()) {
        for (const SceneDiagnostic& diagnostic : built.diagnostics) {
            std::cerr << diagnostic.format() << "\n";
        }
    }
    assert(built.ok());
    assert(built.root->get_child_count() == 21);

    const std::string written = write_scene(parsed.document);
    SceneParseResult reparsed = parse_scene(written, SceneParseOptions{"all2.gscene"});
    assert(reparsed.ok());
    SceneBuildResult rebuilt = build_scene(reparsed.document, nullptr);
    assert(rebuilt.ok());

    LineEdit* edit = dynamic_cast<LineEdit*>(built.root->get_node<LineEdit>("Edit").get());
    assert(edit != nullptr);
    assert(edit->get_text() == "hello");
    assert(edit->get_caret() == 2);

    SpinBox* spin = dynamic_cast<SpinBox*>(built.root->get_node<SpinBox>("Spin").get());
    assert(spin != nullptr);
    assert(spin->get_value() == 2.5);
    assert(spin->get_text() == "2.5");

    NodeTextMatrix* matrix =
        dynamic_cast<NodeTextMatrix*>(built.root->get_node<NodeTextMatrix>("Matrix").get());
    assert(matrix != nullptr);
    assert(matrix->get_text().size() == 2);
}

void test_comments_crlf_bom_and_escapes() {
    const std::string source =
        "\xEF\xBB\xBF# leading comment\r\n"
        "format = 1\r\n"
        "Label \"Root\" {\r\n"
        "    # another comment\r\n"
        "    text = \"caf\\u00E9 \\u20AC\\nnext \\\"quote\\\" \\U0001F600\"\r\n"
        "}\r\n";

    SceneParseResult parsed = parse_scene(source, SceneParseOptions{"unicode.gscene"});
    if (!parsed.ok()) {
        for (const SceneDiagnostic& diagnostic : parsed.diagnostics) {
            std::cerr << diagnostic.format() << "\n";
        }
    }
    assert(parsed.ok());
    SceneBuildResult built = build_scene(parsed.document, nullptr);
    assert(built.ok());
    Label* label = dynamic_cast<Label*>(built.root.get());
    assert(label != nullptr);
    assert(label->get_text() == "café €\nnext \"quote\" 😀");
}

void test_utf8_caret_and_locale_independent_numbers() {
    SceneParseResult parsed = parse_scene(R"(format = 1
LineEdit "Root" {
    text = "café"
    caret = 4
}
)", SceneParseOptions{"caret.gscene"});
    assert(parsed.ok());
    SceneBuildResult built = build_scene(parsed.document, nullptr);
    assert(built.ok());
    LineEdit* edit = dynamic_cast<LineEdit*>(built.root.get());
    assert(edit != nullptr);
    assert(edit->get_text() == "café");
    assert(edit->get_caret() == 5); // byte offset after the two-byte é

    SceneParseResult past_end = parse_scene(R"(format = 1
LineEdit "Root" {
    text = "café"
    caret = 9
}
)", SceneParseOptions{"caret2.gscene"});
    assert(past_end.ok());
    SceneBuildResult bad = build_scene(past_end.document, nullptr);
    assert(!bad.ok());
}

void test_parser_limits() {
    SceneParseOptions bytes;
    bytes.max_bytes = 8;
    SceneParseResult too_large = parse_scene("format = 1\n", bytes);
    assert(!too_large.ok());

    SceneParseOptions nodes;
    nodes.max_nodes = 1;
    SceneParseResult too_many = parse_scene(R"(format = 1
VBox "Root" {
    Label "Child" {}
}
)", nodes);
    assert(!too_many.ok());

    SceneParseOptions depth;
    depth.max_depth = 0;
    SceneParseResult too_deep = parse_scene(R"(format = 1
VBox "Root" {
    Label "Child" {}
}
)", depth);
    assert(!too_deep.ok());
}

void test_invalid_utf8_and_version() {
    std::string invalid = "format = 1\nLabel \"Root\" { text = \"";
    invalid.push_back(static_cast<char>(0xFF));
    invalid += "\" }\n";
    SceneParseResult parsed = parse_scene(invalid, SceneParseOptions{"bad-utf8.gscene"});
    assert(!parsed.ok());
    bool saw_utf8 = false;
    for (const SceneDiagnostic& diagnostic : parsed.diagnostics) {
        if (diagnostic.code == SceneDiagnosticCode::InvalidUtf8) saw_utf8 = true;
    }
    assert(saw_utf8);

    SceneParseResult version = parse_scene("format = 2\nLabel \"Root\" {}\n",
                                           SceneParseOptions{"v2.gscene"});
    assert(!version.ok());
    bool saw_version = false;
    for (const SceneDiagnostic& diagnostic : version.diagnostics) {
        if (diagnostic.code == SceneDiagnosticCode::UnsupportedVersion) saw_version = true;
    }
    assert(saw_version);
}

void test_parse_error_positions() {
    SceneParseResult parsed = parse_scene(R"(format = 1
VBox "Root" {
    Label "A" {}
    Label "A" {}
}
)", SceneParseOptions{"duplicate.gscene"});
    assert(!parsed.ok());
    bool saw_sibling = false;
    for (const SceneDiagnostic& diagnostic : parsed.diagnostics) {
        if (diagnostic.code == SceneDiagnosticCode::DuplicateSibling) {
            saw_sibling = true;
            assert(diagnostic.span.begin.line == 4);
            assert(diagnostic.source == "duplicate.gscene");
        }
    }
    assert(saw_sibling);

    SceneParseResult unknown_property = parse_scene(R"(format = 1
Label "Root" {
    mystery = 1
}
)", SceneParseOptions{"unknown.gscene"});
    assert(unknown_property.ok());
    SceneBuildResult built = build_scene(unknown_property.document, nullptr);
    assert(!built.ok());
    assert(built.diagnostics.front().span.begin.line == 3);
    assert(built.diagnostics.front().source == "<memory>");
}

void test_writer_drops_comments() {
    SceneParseResult parsed = parse_scene(R"(format = 1
# scene comment
Label "Root" {
    # node comment
    text = "hello" # trailing comment
}
)", SceneParseOptions{"comments.gscene"});
    assert(parsed.ok());
    const std::string written = write_scene(parsed.document);
    assert(written.find('#') == std::string::npos);
    assert(written.find("text = \"hello\"") != std::string::npos);
}

void test_named_style_errors() {
    SceneParseResult missing_theme = parse_scene(R"(format = 1
Label "Root" {
    style = "panel"
}
)", SceneParseOptions{"style.gscene"});
    assert(missing_theme.ok());
    SceneBuildResult no_theme = build_scene(missing_theme.document, nullptr);
    assert(!no_theme.ok());

    ThemeParseResult parsed_theme = parse_theme(THEME, SceneParseOptions{"default.gtheme"});
    ThemeBuildResult built_theme = build_theme(parsed_theme.document);
    SceneBuildResult missing_key =
        build_scene(missing_theme.document, built_theme.theme.get());
    assert(missing_key.ok()); // "panel" exists in THEME

    SceneParseResult other = parse_scene(R"(format = 1
Label "Root" {
    style = "nope"
}
)", SceneParseOptions{"style2.gscene"});
    SceneBuildResult missing_other = build_scene(other.document, built_theme.theme.get());
    assert(!missing_other.ok());
}

void test_writer_preserves_explicit_defaults() {
    SceneParseResult parsed = parse_scene(R"(format = 1
Label "Root" {
    text = "x"
    style = none
    visible = true
}
)", SceneParseOptions{"explicit.gscene"});
    assert(parsed.ok());

    ThemeParseResult parsed_theme = parse_theme(THEME, SceneParseOptions{"default.gtheme"});
    ThemeBuildResult built_theme = build_theme(parsed_theme.document);
    assert(built_theme.ok());

    SceneBuildResult first = build_scene(parsed.document, built_theme.theme.get());
    assert(first.ok());
    Label* label = dynamic_cast<Label*>(first.root.get());
    assert(label != nullptr);
    assert(!label->get_stylebox().has_background());

    const std::string written = write_scene(parsed.document);
    assert(written.find("style = none") != std::string::npos);
    assert(written.find("visible = true") != std::string::npos);

    SceneParseResult reparsed = parse_scene(written, SceneParseOptions{"explicit2.gscene"});
    assert(reparsed.ok());
    SceneBuildResult second = build_scene(reparsed.document, built_theme.theme.get());
    assert(second.ok());
    Label* second_label = dynamic_cast<Label*>(second.root.get());
    assert(second_label != nullptr);
    assert(!second_label->get_stylebox().has_background());
}

void test_writer_preserves_unknown_properties() {
    SceneParseResult parsed = parse_scene(R"(format = 1
Label "Root" {
    text = "keep"
    mystery = 42
}
)", SceneParseOptions{"unknown.gscene"});
    assert(parsed.ok());
    const std::string written = write_scene(parsed.document);
    assert(written.find("mystery = 42") != std::string::npos);

    SceneParseResult reparsed = parse_scene(written, SceneParseOptions{"unknown2.gscene"});
    assert(reparsed.ok());
    SceneBuildResult built = build_scene(reparsed.document, nullptr);
    assert(!built.ok());
}

void test_include_defaults() {
    SceneParseResult parsed = parse_scene(R"(format = 1
Label "Root" {
}
)", SceneParseOptions{"defaults.gscene"});
    assert(parsed.ok());

    SceneWriteOptions options;
    options.include_defaults = true;
    const std::string written = write_scene(parsed.document, options);
    assert(written.find("visible = true") != std::string::npos);
    assert(written.find("focus_mode = \"none\"") != std::string::npos);
    assert(written.find("anchors = \"top_left\"") != std::string::npos);
    assert(written.find("text = \"\"") != std::string::npos);

    SceneParseResult reparsed = parse_scene(written, SceneParseOptions{"defaults2.gscene"});
    assert(reparsed.ok());
    SceneBuildResult built = build_scene(reparsed.document, nullptr);
    assert(built.ok());
}

void test_file_loading_and_theme_resolution() {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "guayaba_scene_format_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);

    {
        std::ofstream theme(dir / "theme.gtheme", std::ios::binary);
        theme << "format = 1\nstyle \"label\" {\n    background = \"BLUE\"\n}\n";
    }
    {
        std::ofstream scene(dir / "main.gscene", std::ios::binary);
        scene << "format = 1\ntheme = \"theme.gtheme\"\nLabel \"Root\" {\n"
                 "    text = \"hello\"\n}\n";
    }

    SceneAsset asset = SceneLoader::load_file(dir / "main.gscene");
    assert(asset.root != nullptr);
    assert(asset.theme != nullptr);
    Label* label = dynamic_cast<Label*>(asset.root.get());
    assert(label != nullptr);
    assert(label->get_text() == "hello");
    assert(label->get_stylebox().get_background_color() == 4);

    SceneAsset from_string = SceneLoader::load_string(
        "format = 1\ntheme = \"theme.gtheme\"\nLabel \"Root\" {}\n",
        SceneLoadOptions{"<memory>", dir});
    assert(from_string.root != nullptr);

    SceneParseResult reparsed = parse_scene_file(dir / "main.gscene");
    assert(reparsed.ok());
    write_scene_file(reparsed.document, dir / "saved.gscene");
    SceneAsset saved = SceneLoader::load_file(dir / "saved.gscene");
    assert(saved.root != nullptr);

    ThemeParseResult theme_doc = parse_theme_file(dir / "theme.gtheme");
    assert(theme_doc.ok());
    write_theme_file(theme_doc.document, dir / "saved.gtheme");
    assert(std::filesystem::exists(dir / "saved.gtheme"));

    std::filesystem::remove_all(dir);
}

void test_validation_strictness_and_unknown_type() {
    SceneDocument document;
    document.root = std::make_unique<SceneNodeDesc>();
    document.root->type = "Mystery";
    document.root->name = "Root";
    document.root->properties.push_back(
        SceneProperty{"mystery", SceneValue::integer(1), {}});

    const std::vector<SceneDiagnostic> warnings = validate_scene(document, false, "ast");
    assert(!warnings.empty());
    assert(!has_error(warnings));
    for (const SceneDiagnostic& warning : warnings) {
        assert(warning.severity == SceneDiagnosticSeverity::Warning);
    }

    const std::vector<SceneDiagnostic> errors = validate_scene(document, true, "ast");
    assert(has_error(errors));

    const std::string written = write_scene(document);
    assert(written.find("Mystery") != std::string::npos);
    assert(written.find("mystery = 1") != std::string::npos);
}

void test_theme_apply_order_and_options() {
    const char* scene = R"(format = 1
Label "Root" {
    text = "x"
    style = { background = "RED" }
}
)";
    SceneParseResult parsed = parse_scene(scene, SceneParseOptions{"order.gscene"});
    assert(parsed.ok());
    ThemeParseResult parsed_theme = parse_theme(THEME, SceneParseOptions{"default.gtheme"});
    ThemeBuildResult built_theme = build_theme(parsed_theme.document);

    SceneBuildResult with_theme = build_scene(parsed.document, built_theme.theme.get());
    assert(with_theme.ok());
    Label* label = dynamic_cast<Label*>(with_theme.root.get());
    assert(label != nullptr);
    assert(label->get_stylebox().get_background_color() == 1); // explicit RED wins

    SceneLoadOptions no_apply;
    no_apply.apply_theme = false;
    SceneBuildResult without_theme =
        build_scene(parsed.document, built_theme.theme.get(), no_apply);
    assert(without_theme.ok());
    Label* no_apply_label = dynamic_cast<Label*>(without_theme.root.get());
    assert(no_apply_label != nullptr);
    assert(no_apply_label->get_stylebox().get_background_color() == 1);

    const char* named = R"(format = 1
Label "Root" {
    style = "panel"
}
)";
    SceneParseResult named_parsed = parse_scene(named, SceneParseOptions{"named.gscene"});
    SceneBuildResult named_built =
        build_scene(named_parsed.document, built_theme.theme.get(), no_apply);
    assert(named_built.ok());
}

void test_fallback_theme_and_missing_theme_file() {
    ThemeParseResult parsed_theme = parse_theme(THEME, SceneParseOptions{"default.gtheme"});
    ThemeBuildResult built_theme = build_theme(parsed_theme.document);

    SceneLoadOptions options;
    options.fallback_theme = built_theme.theme.get();
    SceneAsset asset = SceneLoader::load_string(R"(format = 1
Label "Root" { text = "x" }
)", options);
    assert(asset.root != nullptr);
    assert(asset.theme == nullptr); // fallback is not owned
    Label* label = dynamic_cast<Label*>(asset.root.get());
    assert(label != nullptr);
    assert(label->get_stylebox().has_background());

    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "guayaba_scene_missing_theme";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    {
        std::ofstream scene(dir / "main.gscene", std::ios::binary);
        scene << "format = 1\ntheme = \"missing.gtheme\"\nLabel \"Root\" {}\n";
    }

    bool threw = false;
    try {
        (void)SceneLoader::load_file(dir / "main.gscene");
    } catch (const SceneError&) {
        threw = true;
    }
    assert(threw);
    std::filesystem::remove_all(dir);
}

void test_loader_throws_on_error() {
    bool threw = false;
    try {
        (void)SceneLoader::load_string(R"(format = 1
Label "Root" {
    text = 1
}
)");
    } catch (const SceneError& error) {
        threw = true;
        assert(!error.diagnostics().empty());
        assert(error.what() != nullptr);
    }
    assert(threw);
}

void test_headless_layout_and_draw() {
    SceneParseResult parsed = parse_scene(R"(format = 1
VBox "Root" {
    anchors = "full_rect"
    Label "Title" {
        text = "headless"
    }
}
)", SceneParseOptions{"headless.gscene"});
    assert(parsed.ok());
    SceneBuildResult built = build_scene(parsed.document, nullptr);
    assert(built.ok());

    built.root->update_layout(Rect{Vec2{0, 0}, Vec2{40, 12}});
    ConsoleRenderer renderer(Vec2{40, 12}, nullptr);
    built.root->draw(renderer, Rect{Vec2{0, 0}, Vec2{40, 12}});
    renderer.present();
}

} // namespace

int main() {
    test_parse_and_schema();
    test_build_with_theme();
    test_spinbox_and_hidden_properties();
    test_validation_errors();
    test_anchor_rules();
    test_writer_round_trip();
    test_writer_anchors_and_defaults();
    test_theme_writer();
    test_example_round_trip_can_build();
    test_every_type_builds_by_default();
    test_default_build_values();
    test_all_properties_scene();
    test_comments_crlf_bom_and_escapes();
    test_utf8_caret_and_locale_independent_numbers();
    test_parser_limits();
    test_invalid_utf8_and_version();
    test_parse_error_positions();
    test_writer_drops_comments();
    test_named_style_errors();
    test_writer_preserves_explicit_defaults();
    test_writer_preserves_unknown_properties();
    test_include_defaults();
    test_file_loading_and_theme_resolution();
    test_validation_strictness_and_unknown_type();
    test_theme_apply_order_and_options();
    test_fallback_theme_and_missing_theme_file();
    test_loader_throws_on_error();
    test_headless_layout_and_draw();
    std::cout << "all checks passed.\n";
    return 0;
}
