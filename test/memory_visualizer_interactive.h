#pragma once

#include "memory_visualizer_components.h"
#include <functional>

/**
 * Interactive features for memory visualization
 */
class MemoryVisualizerInteractive {
public:
    struct InteractionConfig {
        bool enableZoom = true;
        bool enablePan = true;
        bool enableTooltips = true;
        bool enableHighlighting = true;
        double zoomFactor = 1.2;
        int tooltipDelay = 200;  // ms
    };

    struct ViewState {
        double viewportX = 0;
        double viewportY = 0;
        double scale = 1.0;
        std::string selectedPattern;
        std::string highlightedBlock;
    };

    static void generateInteractiveElements(std::ofstream& file,
                                         const MemoryVisualizer::VisualizationConfig& config,
                                         const InteractionConfig& interactionConfig) {
        // Add JavaScript for interactivity
        file << "<script type=\"text/javascript\">\n";
        generateJavaScript(file, config, interactionConfig);
        file << "</script>\n";

        // Add interactive controls
        generateControls(file, config);
        
        // Add tooltip container
        file << "<div id=\"tooltip\" class=\"tooltip\" style=\"display: none;\"></div>\n";
        
        // Add event listeners
        file << "<script>\n"
             << "document.addEventListener('DOMContentLoaded', function() {\n"
             << "    initializeInteractivity();\n"
             << "});\n"
             << "</script>\n";
    }

private:
    static void generateJavaScript(std::ofstream& file,
                                const MemoryVisualizer::VisualizationConfig& config,
                                const InteractionConfig& interactionConfig) {
        // ViewState management
        file << "let viewState = {\n"
             << "    viewportX: 0,\n"
             << "    viewportY: 0,\n"
             << "    scale: 1.0,\n"
             << "    selectedPattern: null,\n"
             << "    highlightedBlock: null\n"
             << "};\n\n";

        // Zoom handling
        generateZoomHandlers(file, interactionConfig);

        // Pan handling
        generatePanHandlers(file, interactionConfig);

        // Tooltip handling
        generateTooltipHandlers(file, interactionConfig);

        // Highlight handling
        generateHighlightHandlers(file, interactionConfig);

        // View transformation
        generateViewTransform(file);

        // Event initialization
        generateEventInitialization(file);
    }

    static void generateZoomHandlers(std::ofstream& file,
                                  const InteractionConfig& config) {
        if (!config.enableZoom) return;

        file << "function handleZoom(event) {\n"
             << "    event.preventDefault();\n"
             << "    const zoomFactor = " << config.zoomFactor << ";\n"
             << "    const delta = Math.sign(event.deltaY);\n"
             << "    const scale = delta > 0 ? 1/zoomFactor : zoomFactor;\n"
             << "    \n"
             << "    // Calculate zoom center\n"
             << "    const rect = event.target.getBoundingClientRect();\n"
             << "    const x = event.clientX - rect.left;\n"
             << "    const y = event.clientY - rect.top;\n"
             << "    \n"
             << "    // Update view state\n"
             << "    viewState.viewportX += (x - viewState.viewportX) * (1 - scale);\n"
             << "    viewState.viewportY += (y - viewState.viewportY) * (1 - scale);\n"
             << "    viewState.scale *= scale;\n"
             << "    \n"
             << "    updateView();\n"
             << "}\n\n";
    }

    static void generatePanHandlers(std::ofstream& file,
                                 const InteractionConfig& config) {
        if (!config.enablePan) return;

        file << "let isPanning = false;\n"
             << "let lastX = 0;\n"
             << "let lastY = 0;\n\n"
             << "function startPan(event) {\n"
             << "    isPanning = true;\n"
             << "    lastX = event.clientX;\n"
             << "    lastY = event.clientY;\n"
             << "}\n\n"
             << "function handlePan(event) {\n"
             << "    if (!isPanning) return;\n"
             << "    \n"
             << "    const dx = event.clientX - lastX;\n"
             << "    const dy = event.clientY - lastY;\n"
             << "    \n"
             << "    viewState.viewportX += dx;\n"
             << "    viewState.viewportY += dy;\n"
             << "    \n"
             << "    lastX = event.clientX;\n"
             << "    lastY = event.clientY;\n"
             << "    \n"
             << "    updateView();\n"
             << "}\n\n"
             << "function endPan() {\n"
             << "    isPanning = false;\n"
             << "}\n\n";
    }

    static void generateTooltipHandlers(std::ofstream& file,
                                     const InteractionConfig& config) {
        if (!config.enableTooltips) return;

        file << "let tooltipTimeout = null;\n\n"
             << "function showTooltip(event, content) {\n"
             << "    clearTimeout(tooltipTimeout);\n"
             << "    tooltipTimeout = setTimeout(() => {\n"
             << "        const tooltip = document.getElementById('tooltip');\n"
             << "        tooltip.innerHTML = content;\n"
             << "        tooltip.style.display = 'block';\n"
             << "        tooltip.style.left = (event.pageX + 10) + 'px';\n"
             << "        tooltip.style.top = (event.pageY + 10) + 'px';\n"
             << "    }, " << config.tooltipDelay << ");\n"
             << "}\n\n"
             << "function hideTooltip() {\n"
             << "    clearTimeout(tooltipTimeout);\n"
             << "    document.getElementById('tooltip').style.display = 'none';\n"
             << "}\n\n";
    }

    static void generateHighlightHandlers(std::ofstream& file,
                                      const InteractionConfig& config) {
        if (!config.enableHighlighting) return;

        file << "function highlightPattern(patternId) {\n"
             << "    if (viewState.selectedPattern === patternId) return;\n"
             << "    \n"
             << "    // Remove previous highlight\n"
             << "    if (viewState.selectedPattern) {\n"
             << "        document.querySelectorAll('.pattern-' + viewState.selectedPattern)\n"
             << "            .forEach(el => el.classList.remove('highlighted'));\n"
             << "    }\n"
             << "    \n"
             << "    // Add new highlight\n"
             << "    viewState.selectedPattern = patternId;\n"
             << "    if (patternId) {\n"
             << "        document.querySelectorAll('.pattern-' + patternId)\n"
             << "            .forEach(el => el.classList.add('highlighted'));\n"
             << "    }\n"
             << "}\n\n";
    }

    static void generateViewTransform(std::ofstream& file) {
        file << "function updateView() {\n"
             << "    const svg = document.querySelector('svg');\n"
             << "    const content = svg.getElementById('content');\n"
             << "    \n"
             << "    const transform = `translate(${viewState.viewportX}px, "
             << "${viewState.viewportY}px) scale(${viewState.scale})`;\n"
             << "    \n"
             << "    content.style.transform = transform;\n"
             << "}\n\n";
    }

    static void generateEventInitialization(std::ofstream& file) {
        file << "function initializeInteractivity() {\n"
             << "    const svg = document.querySelector('svg');\n"
             << "    \n"
             << "    svg.addEventListener('wheel', handleZoom);\n"
             << "    svg.addEventListener('mousedown', startPan);\n"
             << "    svg.addEventListener('mousemove', handlePan);\n"
             << "    svg.addEventListener('mouseup', endPan);\n"
             << "    svg.addEventListener('mouseleave', endPan);\n"
             << "    \n"
             << "    // Initialize tooltip handlers\n"
             << "    document.querySelectorAll('[data-tooltip]').forEach(el => {\n"
             << "        el.addEventListener('mouseenter', e => {\n"
             << "            showTooltip(e, el.getAttribute('data-tooltip'));\n"
             << "        });\n"
             << "        el.addEventListener('mouseleave', hideTooltip);\n"
             << "    });\n"
             << "}\n\n";
    }

    static void generateControls(std::ofstream& file,
                              const MemoryVisualizer::VisualizationConfig& config) {
        file << "<div class=\"controls\">\n"
             << "    <button onclick=\"resetView()\">Reset View</button>\n"
             << "    <button onclick=\"zoomIn()\">Zoom In</button>\n"
             << "    <button onclick=\"zoomOut()\">Zoom Out</button>\n"
             << "    <select onchange=\"highlightPattern(this.value)\">\n"
             << "        <option value=\"\">Select Pattern...</option>\n"
             << "        <option value=\"cyclic\">Cyclic Allocations</option>\n"
             << "        <option value=\"growing\">Growing Memory</option>\n"
             << "        <option value=\"fragmented\">Fragmentation</option>\n"
             << "        <option value=\"leak\">Potential Leaks</option>\n"
             << "    </select>\n"
             << "</div>\n";
    }
};

// Macro for adding interactivity
#define ADD_VISUALIZATION_INTERACTIVITY(file, config) \
    MemoryVisualizerInteractive::generateInteractiveElements(file, config, \
        MemoryVisualizerInteractive::InteractionConfig())