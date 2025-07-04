#include <gtest/gtest.h>
#include "memory_visualizer.h"
#include "memory_visualizer_components.h"
#include "memory_visualizer_interactive.h"
#include <sstream>
#include <regex>

class MemoryVisualizationTest : public ::testing::Test {
protected:
    std::stringstream output;
    MemoryVisualizer::VisualizationConfig config;
    
    void SetUp() override {
        config.width = 800;
        config.height = 600;
        config.margin = 50;
        config.showGrid = true;
        config.showTooltips = true;
    }

    bool containsSVGElement(const std::string& svg, const std::string& element) {
        std::regex pattern("<" + element + "[^>]*>");
        return std::regex_search(svg, pattern);
    }

    int countElements(const std::string& svg, const std::string& element) {
        std::regex pattern("<" + element + "[^>]*>");
        auto begin = std::sregex_iterator(svg.begin(), svg.end(), pattern);
        auto end = std::sregex_iterator();
        return std::distance(begin, end);
    }

    bool validateSVGStructure(const std::string& svg) {
        // Check SVG header
        if (svg.find("<?xml version=\"1.0\" encoding=\"UTF-8\"?>") == std::string::npos) {
            return false;
        }
        
        // Check SVG root element
        if (svg.find("<svg") == std::string::npos || 
            svg.find("</svg>") == std::string::npos) {
            return false;
        }

        // Check width and height attributes
        std::regex dimensions("width=\"" + std::to_string(config.width) + 
                            "\".*height=\"" + std::to_string(config.height) + "\"");
        if (!std::regex_search(svg, dimensions)) {
            return false;
        }

        return true;
    }
};

TEST_F(MemoryVisualizationTest, BasicVisualizationGeneration) {
    std::ofstream file("test_basic.svg");
    MemoryVisualizer::instance().generateVisualization("test_basic.svg", config);
    
    std::ifstream input("test_basic.svg");
    std::string svg((std::istreambuf_iterator<char>(input)),
                   std::istreambuf_iterator<char>());
    
    EXPECT_TRUE(validateSVGStructure(svg));
    EXPECT_TRUE(containsSVGElement(svg, "g"));
    EXPECT_TRUE(containsSVGElement(svg, "rect"));
    EXPECT_TRUE(containsSVGElement(svg, "path"));
}

TEST_F(MemoryVisualizationTest, TimelineGeneration) {
    // Setup test data
    auto& analyzer = AllocationPatternAnalyzer::instance();
    // Add some test allocations
    analyzer.recordAllocation(nullptr, 1024, "test.cpp", 42);
    analyzer.recordAllocation(nullptr, 2048, "test.cpp", 43);
    analyzer.recordDeallocation(nullptr);
    
    std::ofstream file("test_timeline.svg");
    MemoryVisualizer::instance().generateVisualization("test_timeline.svg", config);
    
    std::ifstream input("test_timeline.svg");
    std::string svg((std::istreambuf_iterator<char>(input)),
                   std::istreambuf_iterator<char>());
    
    EXPECT_TRUE(containsSVGElement(svg, "path"));
    
    // Verify timeline points
    std::regex pointPattern("L\\s+\\d+(\\.\\d+)?\\s+\\d+(\\.\\d+)?");
    auto begin = std::sregex_iterator(svg.begin(), svg.end(), pointPattern);
    auto end = std::sregex_iterator();
    EXPECT_GE(std::distance(begin, end), 3); // At least 3 points for our test data
}

TEST_F(MemoryVisualizationTest, FragmentationVisualization) {
    // Setup test fragmentation data
    auto& analyzer = HeapFragmentationAnalyzer::instance();
    // Add some test blocks
    analyzer.trackAllocation(nullptr, 1024);
    analyzer.trackAllocation(nullptr, 512);
    analyzer.trackDeallocation(nullptr);
    
    std::ofstream file("test_fragmentation.svg");
    MemoryVisualizer::instance().generateVisualization("test_fragmentation.svg", config);
    
    std::ifstream input("test_fragmentation.svg");
    std::string svg((std::istreambuf_iterator<char>(input)),
                   std::istreambuf_iterator<char>());
    
    EXPECT_TRUE(containsSVGElement(svg, "rect"));
    int blockCount = countElements(svg, "rect");
    EXPECT_GE(blockCount, 2); // At least 2 blocks visualized
}

TEST_F(MemoryVisualizationTest, InteractiveFeatures) {
    std::ofstream file("test_interactive.svg");
    MemoryVisualizerInteractive::InteractionConfig interactiveConfig;
    interactiveConfig.enableZoom = true;
    interactiveConfig.enablePan = true;
    
    MemoryVisualizerInteractive::generateInteractiveElements(file, config, interactiveConfig);
    
    std::ifstream input("test_interactive.svg");
    std::string svg((std::istreambuf_iterator<char>(input)),
                   std::istreambuf_iterator<char>());
    
    EXPECT_TRUE(svg.find("script") != std::string::npos);
    EXPECT_TRUE(svg.find("function handleZoom") != std::string::npos);
    EXPECT_TRUE(svg.find("function handlePan") != std::string::npos);
    EXPECT_TRUE(svg.find("class=\"controls\"") != std::string::npos);
}

TEST_F(MemoryVisualizationTest, TooltipGeneration) {
    std::ofstream file("test_tooltips.svg");
    config.showTooltips = true;
    
    MemoryVisualizer::instance().generateVisualization("test_tooltips.svg", config);
    
    std::ifstream input("test_tooltips.svg");
    std::string svg((std::istreambuf_iterator<char>(input)),
                   std::istreambuf_iterator<char>());
    
    EXPECT_TRUE(svg.find("data-tooltip") != std::string::npos);
    EXPECT_TRUE(svg.find("showTooltip") != std::string::npos);
    EXPECT_TRUE(svg.find("hideTooltip") != std::string::npos);
}

TEST_F(MemoryVisualizationTest, PatternHighlighting) {
    std::ofstream file("test_highlighting.svg");
    
    // Add some test patterns
    auto& analyzer = AllocationPatternAnalyzer::instance();
    // Create cyclic pattern
    for (int i = 0; i < 5; i++) {
        analyzer.recordAllocation(nullptr, 1024, "test.cpp", 42);
        analyzer.recordDeallocation(nullptr);
    }
    
    MemoryVisualizer::instance().generateVisualization("test_highlighting.svg", config);
    
    std::ifstream input("test_highlighting.svg");
    std::string svg((std::istreambuf_iterator<char>(input)),
                   std::istreambuf_iterator<char>());
    
    EXPECT_TRUE(svg.find("pattern-cyclic") != std::string::npos);
    EXPECT_TRUE(svg.find("highlightPattern") != std::string::npos);
}

TEST_F(MemoryVisualizationTest, AnimationGeneration) {
    std::ofstream file("test_animation.svg");
    
    MemoryVisualizer::instance().generateAnimatedView("test_animation.svg");
    
    std::ifstream input("test_animation.svg");
    std::string svg((std::istreambuf_iterator<char>(input)),
                   std::istreambuf_iterator<char>());
    
    EXPECT_TRUE(svg.find("@keyframes") != std::string::npos);
    EXPECT_TRUE(svg.find("animation") != std::string::npos);
    EXPECT_TRUE(svg.find("animate") != std::string::npos);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}