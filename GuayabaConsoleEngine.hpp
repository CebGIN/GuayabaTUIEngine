#ifndef CGINCUIENGINE
#define CGINCUIENGINE

#include <functional>
#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <chrono>
#include <cstdint>

// --- Forward Declarations ---
class Node;
class Node2D;
class NodeUI;
class NodePCT;
class NodeButton;
class SceneManager;

enum class NodeType{
    NODE,
    NODE2D,
    NODEUI,
    NODEPCT,
    NODEBUTTON,
};

// =====================================================================
// 1. Cross-Platform Core Types
// =====================================================================

// Replaces Windows COORD
struct Vec2 {
    int x = 0;
    int y = 0;
};

inline Vec2 const operator+(Vec2 a, Vec2 b){ return {a.x + b.x, a.y + b.y}; }
inline Vec2 const operator-(Vec2 a, Vec2 b){ return {a.x - b.x, a.y - b.y}; }
inline bool operator==(Vec2 a, Vec2 b){ return (a.x == b.x) && (a.y == b.y); }
inline bool operator!=(Vec2 a, Vec2 b){ return !(a == b); }

// Replaces Windows CHAR_INFO
struct TermChar {
    wchar_t character = L' ';
    uint16_t foreground = 7;   // White (default)
    uint16_t background = 0;   // Black (default)
};

// =====================================================================
// 2. Color Namespace (ANSI color codes 0-15)
// =====================================================================
// Color IDs map to standard 4-bit terminal colors:
//   0=Black, 1=Red, 2=Green, 3=Yellow, 4=Blue, 5=Magenta, 6=Cyan, 7=White
//   8-15 = Bright variants of the above

namespace Color {
    const std::string RED = "RED";
    const std::string GREEN = "GREEN";
    const std::string BLUE = "BLUE";
    const std::string YELLOW = "YELLOW";
    const std::string CYAN = "CYAN";
    const std::string MAGENTA = "MAGENTA";
    const std::string WHITE = "WHITE";
    const std::string BLACK = "BLACK";
    const std::string BRIGHT_RED = "BRIGHT_RED";
    const std::string BRIGHT_GREEN = "BRIGHT_GREEN";
    const std::string BRIGHT_BLUE = "BRIGHT_BLUE";
    const std::string BRIGHT_YELLOW = "BRIGHT_YELLOW";
    const std::string BRIGHT_CYAN = "BRIGHT_CYAN";
    const std::string BRIGHT_MAGENTA = "BRIGHT_MAGENTA";
    const std::string BRIGHT_WHITE = "BRIGHT_WHITE";

    // Returns a platform-neutral color ID (0-15)
    inline uint16_t getColorAttribute(const std::string& colorName) {
        if (colorName == "BLACK")          return 0;
        if (colorName == "RED")            return 1;
        if (colorName == "GREEN")          return 2;
        if (colorName == "YELLOW")         return 3;
        if (colorName == "BLUE")           return 4;
        if (colorName == "MAGENTA")        return 5;
        if (colorName == "CYAN")           return 6;
        if (colorName == "WHITE")          return 7;
        if (colorName == "BRIGHT_BLACK")   return 8;
        if (colorName == "BRIGHT_RED")     return 9;
        if (colorName == "BRIGHT_GREEN")   return 10;
        if (colorName == "BRIGHT_BLUE")    return 12;
        if (colorName == "BRIGHT_YELLOW")  return 11;
        if (colorName == "BRIGHT_CYAN")    return 14;
        if (colorName == "BRIGHT_MAGENTA") return 13;
        if (colorName == "BRIGHT_WHITE")   return 15;
        return 7; // Default: white
    }

    inline uint16_t getBackgroundColorAttribute(const std::string& colorName) {
        return getColorAttribute(colorName);
    }
};

// =====================================================================
// 3. Abstract Terminal Interface (IOSTerminal)
// =====================================================================

class IOSTerminal {
public:
    virtual ~IOSTerminal() = default;

    // Lifecycle
    virtual void init() = 0;
    virtual void restore() = 0;

    // Output
    virtual void setCursorVisible(bool visible) = 0;
    virtual void setCursorPosition(Vec2 pos) = 0;
    virtual void presentBuffer(const std::vector<TermChar>& buffer, Vec2 bufferSize) = 0;

    // Input
    virtual void pollEvents() = 0;
    virtual Vec2 getMousePosition() const = 0;
    virtual bool isLeftClickPressed() const = 0;
    virtual bool isRightClickPressed() const = 0;
    virtual bool isLeftClickJustPressed() const = 0;
    virtual bool isRightClickJustPressed() const = 0;

    // Blocking text input
    virtual std::string getLineInput(Vec2 position) = 0;

    template<typename T>
    T getTypedInput(Vec2 position) {
        T input;
        setCursorPosition(position);
        setCursorVisible(true);
        std::cin >> input;
        setCursorVisible(false);
        return input;
    }
};

// Factory function — implemented by including the correct platform header
std::unique_ptr<IOSTerminal> CreateSystemTerminal();

// =====================================================================
// 4. Input Namespace (Delegates to active IOSTerminal)
// =====================================================================

namespace Input {
    inline IOSTerminal* g_Terminal = nullptr;

    inline Vec2 MousePos = {0, 0};
    inline bool LClick = false;
    inline bool RClick = false;
    inline bool LClickPrev = false;
    inline bool RClickPrev = false;
    inline bool LClickJustPressed = false;
    inline bool RClickJustPressed = false;

    inline void iniciateInput() {
        if (g_Terminal) g_Terminal->init();
    }

    inline void refresh_input() {
        if (!g_Terminal) return;
        g_Terminal->pollEvents();

        LClickPrev = LClick;
        RClickPrev = RClick;

        LClick = g_Terminal->isLeftClickPressed();
        RClick = g_Terminal->isRightClickPressed();

        LClickJustPressed = g_Terminal->isLeftClickJustPressed();
        RClickJustPressed = g_Terminal->isRightClickJustPressed();

        MousePos = g_Terminal->getMousePosition();
    }

    template<typename T>
    T getTypedInput(Vec2 position = {0, 0}){
        if (g_Terminal) return g_Terminal->getTypedInput<T>(position);
        T input;
        std::cin >> input;
        return input;
    }

    inline std::string getLineInput(Vec2 position = {0, 0}){
        if (g_Terminal) return g_Terminal->getLineInput(position);
        std::string input;
        std::getline(std::cin, input);
        return input;
    }
};

// =====================================================================
// 5. ConsoleRenderer (Platform-Agnostic Buffer)
// =====================================================================

class ConsoleRenderer {
private:
    Vec2 consoleBufferSize;
    std::vector<TermChar> screenBuffer;
    IOSTerminal* terminalInstance;

public:
    ConsoleRenderer(Vec2 bufferSize, IOSTerminal* terminal)
        : consoleBufferSize(bufferSize), terminalInstance(terminal)
    {
        screenBuffer.resize(bufferSize.x * bufferSize.y);
        clearBuffer();
    }

    void clearBuffer() {
        TermChar emptyChar;
        emptyChar.character = L' ';
        emptyChar.foreground = 15;  // BRIGHT_WHITE
        emptyChar.background = 15;  // BRIGHT_WHITE (white background default, matching original)

        for (size_t i = 0; i < screenBuffer.size(); ++i) {
            screenBuffer[i] = emptyChar;
        }
    }

    void putChar(Vec2 pos, wchar_t character, uint16_t fg, uint16_t bg) {
        if (pos.x >= 0 && pos.x < consoleBufferSize.x &&
            pos.y >= 0 && pos.y < consoleBufferSize.y) {
            int index = pos.y * consoleBufferSize.x + pos.x;
            screenBuffer[index].character = character;
            screenBuffer[index].foreground = fg;
            screenBuffer[index].background = bg;
        }
    }

    // Convenience overload matching old putChar(pos, char, WORD) pattern
    // The combined WORD is split: low nibble = foreground, high nibble = background
    void putChar(Vec2 pos, wchar_t character, uint16_t combinedAttrs) {
        uint16_t fg = combinedAttrs & 0x0F;
        uint16_t bg = (combinedAttrs >> 4) & 0x0F;
        putChar(pos, character, fg, bg);
    }

    void putString(Vec2 pos, const std::string& text, uint16_t fg, uint16_t bg) {
        for (size_t i = 0; i < text.length(); ++i) {
            putChar(pos + Vec2{static_cast<int>(i), 0}, static_cast<wchar_t>(text[i]), fg, bg);
        }
    }

    // Convenience overload matching old putString(pos, text, WORD) pattern
    void putString(Vec2 pos, const std::string& text, uint16_t combinedAttrs) {
        uint16_t fg = combinedAttrs & 0x0F;
        uint16_t bg = (combinedAttrs >> 4) & 0x0F;
        putString(pos, text, fg, bg);
    }

    void present() {
        if (terminalInstance) {
            terminalInstance->presentBuffer(screenBuffer, consoleBufferSize);
        }
    }
};

// =====================================================================
// 6. Node Hierarchy (Identical logic, platform-neutral types)
// =====================================================================

class Node : public std::enable_shared_from_this<Node>{
    friend class SceneManager;
protected:
    std::vector<std::shared_ptr<Node>> Children;
    std::weak_ptr<Node> Parent;
    std::string name;
    std::function<void(double)> process;
    std::function<void()> atEnterTree;
    std::function<void()> atExitTree;

    mutable bool its_in_the_tree = false;

    virtual void enterTree() {
        its_in_the_tree = true;
        atEnterTree();
        for (const auto& child : Children) {
            child->enterTree(); 
        }
    }

    virtual void exitTree() {
        for (const auto& child : Children) {
            child->exitTree(); 
        }
        atExitTree();
        its_in_the_tree = false;
    }

public:
    Node(const std::string& nodeName = "Unnamed Node") : name(nodeName) {
        process = [](double){}; atEnterTree = [](){}; atExitTree = [](){};
    }
    
    virtual ~Node() {}

    void setParent(std::shared_ptr<Node> parentNode) {
        Parent = parentNode;
    }

    std::shared_ptr<Node> getParent() const {
        return Parent.lock();
    }

    template<typename T>
    std::shared_ptr<T> getParentOfType() const {
        static_assert(std::is_base_of<Node, T>::value, "T must be a Node type.");
        std::shared_ptr<Node> currentParent = getParent();
        while (currentParent) {
            std::shared_ptr<T> parentOfType = std::dynamic_pointer_cast<T>(currentParent);
            if (parentOfType) {
                return parentOfType;
            }
            currentParent = currentParent->getParent();
        }
        return nullptr;
    }

    bool isRoot() const {
        return Parent.expired() || !Parent.lock();
    }

    void addChild(std::shared_ptr<Node> childNode) {
        if (childNode) {
            Children.push_back(childNode);
            childNode->setParent(shared_from_this());
            if (its_in_the_tree) childNode->enterTree();
        } 
    }

    size_t getChildCount() const {
        return Children.size();
    }

    std::shared_ptr<Node> getChild(size_t index) {
        if (index < Children.size()) {
            return Children[index];
        } else {
            throw std::out_of_range("Node::getChild(index): Index out of bounds. en: " + name);
        }
    }

    std::shared_ptr<const Node> getChild(size_t index) const {
        if (index < Children.size()) {
            return Children[index];
        } else {
            throw std::out_of_range("Node::getChild(index): Index out of bounds.");
        }
    }

    std::vector<std::shared_ptr<Node>>::iterator begin() { return Children.begin();}
    std::vector<std::shared_ptr<Node>>::iterator end() { return Children.end();}

    std::vector<std::shared_ptr<Node>>::const_iterator begin() const { return Children.cbegin();}
    std::vector<std::shared_ptr<Node>>::const_iterator end() const { return Children.cend();}

    const std::string& getName() const {
        return name;
    }

    virtual NodeType getType() const {
        return NodeType::NODE;
    }

    virtual void update(double deltaTime) {
        process(deltaTime); 
        for (const auto& child : Children) {
            child->update(deltaTime); 
        }
    }

    void setProcessFunction(std::function<void(double)> func) {
        if (func) { process = func; }
        else { process = [](double){}; }
    }
    void setAtEnterFunction(std::function<void()> func) {
        if (func) { atEnterTree = func; }
        else { atEnterTree = [](){}; }
    }
    void setAtExitFunction(std::function<void()> func) {
        if (func) { atExitTree = func; }
        else { atExitTree = [](){}; }
    }

    virtual void draw(ConsoleRenderer& renderer) {
        for (const auto& child : Children) {
            child->draw(renderer);
        }
    }

    virtual void invalidateCacheRecursively(){
        for (const auto& child : Children) {
            child->invalidateCacheRecursively();  
        }
    }
};

class Node2D : public Node{
protected:
    Vec2 position;
    mutable Vec2 cached_global_position;
    mutable bool is_cached_position_valid = false;
    
public:
    
    Node2D(const std::string& nodeName = "Unnamed Node", Vec2 nodePosition = {0, 0}) : Node(nodeName), position(nodePosition){}

    void invalidateCacheRecursively() override {
        if (!this->is_cached_position_valid) return;
        this->is_cached_position_valid = false;
        Node::invalidateCacheRecursively();
    }

    Vec2 getLocalPosition() const {
        return position;
    }
    void setLocalPosition(Vec2 new_position){
        if(new_position == position) return;
        position = new_position;
        invalidateCacheRecursively();
    }
    NodeType getType() const override {
        return NodeType::NODE2D;
    }

    Vec2 getGlobalPosition() const {
        if (is_cached_position_valid) return cached_global_position;
        Vec2 globalPos = position;

        std::shared_ptr<Node2D> parent2D = getParentOfType<Node2D>();
        if(parent2D) globalPos = globalPos + parent2D->getGlobalPosition();

        cached_global_position = globalPos;
        is_cached_position_valid = true;
        return globalPos;
    }
    
    void setGlobalPosition(Vec2 newGlobalPosition) {
        setLocalPosition(((newGlobalPosition + getLocalPosition()) - getGlobalPosition()));
        this->cached_global_position = newGlobalPosition;
        this->is_cached_position_valid = true;
    }
};

class NodeUI : public Node2D{
protected:
    Vec2 size;
    std::vector<std::string> text;
    
public: 

    NodeUI(const std::string& nodeName, Vec2 nodePosition, std::vector<std::string> nodeText) :
        Node2D(nodeName, nodePosition), text(nodeText)
    {
        if (!text.empty()) {
            size.x = static_cast<int>(text[0].size());
            size.y = static_cast<int>(text.size());
        } else {
            size.x = 0;
            size.y = 0;
        }
    }

    void set_text(std::vector<std::string> NewText){
        text = NewText;
        if (!text.empty()) {
            size.x = static_cast<int>(text[0].size());
            size.y = static_cast<int>(text.size());
        } else {
            size.x = 0;
            size.y = 0;
        }
    }

    std::vector<std::string> getText(){
        return text;
    }

    NodeType getType() const override {
        return NodeType::NODEUI;
    }

    bool is_inside(const Vec2 point){
        Vec2 globalPos = getGlobalPosition(); 
        return (point.x >= globalPos.x && point.x < globalPos.x + size.x &&
                point.y >= globalPos.y && point.y < globalPos.y + size.y);
    }

    void draw(ConsoleRenderer& renderer) override {
        Vec2 globalPos = getGlobalPosition();
        // Default: black text on white background (matching original BACKGROUND_R|G|B)
        uint16_t defaultFg = 0;  // BLACK
        uint16_t defaultBg = 7;  // WHITE

        for (int y_offset = 0; y_offset < static_cast<int>(text.size()); ++y_offset) {
            Vec2 currentLineGlobalPos = (globalPos + Vec2{0, y_offset});
            std::string line = text[y_offset];
            renderer.putString(currentLineGlobalPos, line, defaultFg, defaultBg);
        }
        Node::draw(renderer);
    }
    
    Vec2 getSize() const { return size; }
};

class NodeSQ : public Node2D{
protected:
    Vec2 size;
    uint16_t text_color;
    uint16_t background_color;
public:
    NodeSQ(const std::string& nodeName, Vec2 nodePosition, Vec2 nodeSize,
        const std::string& textColorName, const std::string& backgroundColorName) : Node2D(nodeName, nodePosition), 
        size(nodeSize), text_color(Color::getColorAttribute(textColorName)), background_color(Color::getBackgroundColorAttribute(backgroundColorName)) {}
    
    Vec2 getSize() const{
        return size;
    }

    void setSize(Vec2 newSize = {1, 1}) {
        size = newSize;
    }

    void changeTextColor(const std::string& textColorName){
        text_color = Color::getColorAttribute(textColorName);
    }

    void changeBackgroundColor(const std::string& backgroundColorName){
        background_color = Color::getBackgroundColorAttribute(backgroundColorName);
    }

    void draw(ConsoleRenderer& renderer) override {
        Vec2 globalPos = getGlobalPosition();

        for (int j = 1; j < static_cast<int>(size.y - 1); ++j){
            renderer.putChar(globalPos + Vec2{0, j}, L'|', text_color, background_color);
        }
        for (int j = 1; j < static_cast<int>(size.y - 1); ++j){
            renderer.putChar(globalPos + Vec2{static_cast<int>(size.x - 1), j}, L'|', text_color, background_color);
        }

        for(int i = 0; i < size.x; ++i){
            renderer.putChar(globalPos + Vec2{i, 0}, L'-', text_color, background_color);
        }
        for(int i = 0; i < size.x; ++i){
            renderer.putChar(globalPos + Vec2{i, static_cast<int>(size.y - 1)}, L'-', text_color, background_color);
        }

        Node::draw(renderer);
    }
};

class NodeBox : public Node2D{
    protected:
        Vec2 size;
        uint16_t background_color;
    public:
        NodeBox(const std::string& nodeName, Vec2 nodePosition, Vec2 nodeSize, const std::string& backgroundColorName) : Node2D(nodeName, nodePosition), 
            size(nodeSize), background_color(Color::getBackgroundColorAttribute(backgroundColorName)) {}
        
        Vec2 getSize() const{
            return size;
        }
    
        void setSize(Vec2 newSize = {1, 1}) {
            size = newSize;
        }
    
        void changeBackgroundColor(const std::string& backgroundColorName){
            background_color = Color::getBackgroundColorAttribute(backgroundColorName);
        }
    
        void draw(ConsoleRenderer& renderer) override {
            Vec2 globalPos = getGlobalPosition();
    
            for (int j = 0; j < static_cast<int>(size.y); ++j){
                for(int i = 0; i < size.x; ++i){
                    renderer.putChar(globalPos + Vec2{i, j}, L' ', 0, background_color);
                }
            }
    
            Node::draw(renderer);
        }
    };

class NodePCT : public NodeUI{
protected:
    uint16_t text_color;
    uint16_t background_color;

public: 
    NodePCT(const std::string& nodeName, Vec2 nodePosition, 
            const std::string& textColorName, const std::string& backgroundColorName, 
            std::vector<std::string> nodeText) : NodeUI(nodeName, nodePosition, nodeText), 
            text_color(Color::getColorAttribute(textColorName)), background_color(Color::getBackgroundColorAttribute(backgroundColorName)) {}

    NodeType getType() const override {
        return NodeType::NODEPCT;
    }

   void draw(ConsoleRenderer& renderer) override {
        Vec2 globalPos = getGlobalPosition();

        for (int y_offset = 0; y_offset < static_cast<int>(text.size()); ++y_offset) {
            Vec2 currentLineGlobalPos = (globalPos + Vec2{0, y_offset});
            std::string line = text[y_offset];
            renderer.putString(currentLineGlobalPos, line, text_color, background_color);
        }

        Node::draw(renderer);
    }

    void changeTextColor(const std::string& textColorName){
        text_color = Color::getColorAttribute(textColorName);
    }

    void changeBackgroundColor(const std::string& backgroundColorName){
        background_color = Color::getBackgroundColorAttribute(backgroundColorName);
    }
};

class NodeButton : public NodePCT{
protected:
    bool hovered = false;
    std::function<void()> onClick;

public:
    void update(double deltaTime) override {
        updateHover(Input::MousePos);
        handleClick();
        Node::update(deltaTime); 
    }
    NodeButton(const std::string& nodeName, Vec2 nodePosition, std::string textColor, std::string backgroundColor, std::vector<std::string> nodeText) : 
        NodePCT(nodeName, nodePosition, textColor, backgroundColor, nodeText) {}

    NodeType getType() const override {
        return NodeType::NODEBUTTON;
    }
    bool is_hovered(){
        return hovered;
    }
    void updateHover(const Vec2 mouseGlobalPos) {
        hovered = is_inside(mouseGlobalPos);
    }
    void handleClick() { if (Input::LClickJustPressed && hovered && onClick) onClick(); }

    void setOnClick(std::function<void()> callback) {
        onClick = callback;
    }

    void draw(ConsoleRenderer& renderer) override {
        Vec2 globalPos = getGlobalPosition();
        uint16_t currentFg = text_color;
        uint16_t currentBg = background_color;

        if (hovered) {
            // Swap foreground and background on hover
            currentFg = background_color;
            currentBg = text_color;
        }

        for (int y_offset = 0; y_offset < static_cast<int>(text.size()); ++y_offset) {
            Vec2 currentLineGlobalPos = (globalPos + Vec2{0, y_offset});
            std::string line = text[y_offset];
            renderer.putString(currentLineGlobalPos, line, currentFg, currentBg);
        }
        Node::draw(renderer);
    }
};

// =====================================================================
// 7. SceneManager (std::chrono for cross-platform timing)
// =====================================================================

class SceneManager {
    private:
        std::shared_ptr<Node> root_node = nullptr; 
    
        bool is_running = false; 
        SceneManager() = default;
        SceneManager(const SceneManager&) = delete;
        SceneManager& operator=(const SceneManager&) = delete;
    
        std::chrono::high_resolution_clock::time_point m_lastTime;
        double m_deltaTime = 0.0;

        unsigned long int frameCount = 0;

        std::shared_ptr<Node> next_scene = nullptr; 

        std::unique_ptr<IOSTerminal> osTerminal;

    public:
        static SceneManager& getInstance(){
            static SceneManager instance;
            return instance;
        }
    
        void processScene() {
            auto currentTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = currentTime - m_lastTime;
            m_deltaTime = elapsed.count();
            m_lastTime = currentTime;

            if (!root_node) return;
            root_node->update(m_deltaTime);
        }
    
        std::shared_ptr<Node> getCurrrentScene() {
            return root_node;
        }

        void changeScene(std::shared_ptr<Node> newRoot) {
            if (newRoot == root_node) return;
            next_scene = newRoot;
        }
    
        void startRunning() {
            this->is_running = true;

            // Create the platform-correct terminal
            osTerminal = CreateSystemTerminal();
            osTerminal->init();

            // Wire up the global Input system
            Input::g_Terminal = osTerminal.get();

            m_lastTime = std::chrono::high_resolution_clock::now();

            ConsoleRenderer renderer({100, 60}, osTerminal.get());
            while (this->is_running) {
        
                if (next_scene){
                    if (root_node) {
                        root_node->exitTree();
                    }
                    root_node = next_scene;
                    next_scene = nullptr;
                    if (root_node) { 
                        root_node->enterTree();
                    }
                }

                if (this->is_running && this->root_node) {

                    renderer.clearBuffer();
                    Input::refresh_input();

                    this->processScene();
                    this->root_node->draw(renderer);
                    renderer.present();
                    increaseFrameCount();
                }
            }

            // Restore terminal on exit
            if (osTerminal) osTerminal->restore();
        }
    
        void stopRunning() {
            this->is_running = false;
        }

        unsigned long int getFrameCount() const{
            return frameCount;
        }

        void increaseFrameCount(){
            frameCount++;
        }
};

// =====================================================================
// 8. Platform Backend Inclusion (auto-selects based on OS)
// =====================================================================

#ifdef _WIN32
    #include "GuayabaTerminalWindows.hpp"
#elif defined(__linux__) || defined(__APPLE__)
    #include "GuayabaTerminalLinux.hpp"
#else
    #error "Unsupported platform. GuayabaConsoleEngine supports Windows and Linux."
#endif

#endif

//FIN DEL CODIGO DEL MOTOR
/*
    Uso recomendado: 
    - Crear funciones que se encargan de construir las escenas
    - Las funciones de la forma: std::shared_ptr crearEscena(){... return root;};
    En el main():
    - Crear la root como root = crearEscena();
    - Establecer la root como la escena activa con SceneManager::getInstance().changeScene(root);
    - Iniciar el bucle de ejecución con SceneManager::getInstance().startRunning();
    Al iniciar el bucle la ejecución del main se mantendra en ese punto, cualquier lógica adicional debe ser manejada por el contenido de la escena.
*/