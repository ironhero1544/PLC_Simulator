#pragma once

#include <vector>
#include <string>
#include <map>

struct ImVec2;

namespace plc {

    struct Position {
        float x = 0.0f;
        float y = 0.0f;
        Position() = default;
        Position(float px, float py) : x(px), y(py) {}
        Position(const ImVec2& vec);
        operator ImVec2() const;
    };

    struct Size {
        float width = 0.0f;
        float height = 0.0f;
    };

    struct Color {
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
        float a = 1.0f;
    };

    enum class Mode { WIRING, PROGRAMMING };

    enum class ComponentType {
        PLC, FRL, MANIFOLD, LIMIT_SWITCH, SENSOR, CYLINDER,
        VALVE_SINGLE, VALVE_DOUBLE, BUTTON_UNIT, POWER_SUPPLY
    };

    enum class ToolType { SELECT, PNEUMATIC, ELECTRIC };
    enum class PortType { ELECTRIC, PNEUMATIC };
    enum class WireEditMode { NONE, MOVING_POINT };

    struct PlacedComponent {
        int instanceId = 0;
        ComponentType type;
        Position position;
        Size size;
        bool selected = false;
        float rotation = 0.0f;
        std::map<std::string, float> internalStates;
    };

    struct Port {
        int id = 0;
        Position relativePos;
        Color color;
        bool isInput = true;
        PortType type;
        std::string role;
    };

    struct Wire {
        int id = 0;
        int fromComponentId = 0;
        int fromPortId = 0;
        int toComponentId = 0;
        int toPortId = 0;
        std::vector<Position> wayPoints;
        bool isElectric;
        Color color;
        float thickness;
    };

    struct SnapSettings {
        bool enableGridSnap = true;
        bool enablePortSnap = true;
        bool enableVerticalSnap = true;
        bool enableHorizontalSnap = true;
        float snapDistance = 15.0f;
        float gridSize = 25.0f;
    };

} // namespace plc