#pragma once

#include <array>

namespace levischematic::render::detail {

struct BoxEdge {
    std::array<float, 3> minimum, maximum;
};
inline std::array<BoxEdge, 12>
makeBoxEdges(std::array<float, 3> const& lo, std::array<float, 3> const& hi, float halfWidth) {
    std::array<BoxEdge, 12> edges{};
    int                     index = 0;
    for (int axis = 0; axis < 3; ++axis) {
        int u = (axis + 1) % 3, v = (axis + 2) % 3;
        for (int corner = 0; corner < 4; ++corner) {
            auto& edge      = edges[index++];
            edge.minimum    = lo;
            edge.maximum    = hi;
            float cu        = (corner & 1) ? hi[u] : lo[u];
            float cv        = (corner & 2) ? hi[v] : lo[v];
            edge.minimum[u] = cu - halfWidth;
            edge.maximum[u] = cu + halfWidth;
            edge.minimum[v] = cv - halfWidth;
            edge.maximum[v] = cv + halfWidth;
        }
    }
    return edges;
}

struct BoxFace {
    std::array<std::array<float, 3>, 4> vertices;
    std::array<float, 3>                normal;
};

inline std::array<BoxFace, 6> makeBoxFaces(std::array<float, 3> const& minimum, std::array<float, 3> const& maximum) {
    auto const [x0, y0, z0] = minimum;
    auto const [x1, y1, z1] = maximum;
    return {
        {
         {{{{x1, y0, z0}, {x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0}}}, {0.0f, 0.0f, -1.0f}},
         {{{{x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}}}, {0.0f, 0.0f, 1.0f}},
         {{{{x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1}}}, {0.0f, -1.0f, 0.0f}},
         {{{{x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}, {x0, y1, z0}}}, {0.0f, 1.0f, 0.0f}},
         {{{{x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0}}}, {-1.0f, 0.0f, 0.0f}},
         {{{{x1, y0, z1}, {x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1}}}, {1.0f, 0.0f, 0.0f}},
         }
    };
}

}
