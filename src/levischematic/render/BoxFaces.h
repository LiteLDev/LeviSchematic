#pragma once

#include <array>

namespace levischematic::render::detail {

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
