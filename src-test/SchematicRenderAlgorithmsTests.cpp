


#include "levischematic/render/BoxFaces.h"
#include "levischematic/render/SchematicRenderAlgorithms.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace algorithms = levischematic::render::detail;

using Origins = std::vector<algorithms::GridOrigin>;

namespace {

int failures = 0;

void check(bool value, std::string const& what) {
    if (!value) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}

void checkOrigins(Origins const& actual, Origins const& expected, std::string const& what) {
    if (actual == expected) return;
    std::cerr << "FAIL: " << what << "\n  expected " << expected.size() << " origin(s), got " << actual.size() << "\n";
    for (auto const& o : actual) {
        std::cerr << "    (" << o[0] << ", " << o[1] << ", " << o[2] << ")\n";
    }
    ++failures;
}

void testChunkOrigins() {

    checkOrigins(
        algorithms::generateChunkOrigins(
            {
                0,
                0,
                0
    },
            {1, 1, 1}
        ),
        {{0, 0, 0}},
        "one block"
    );
    checkOrigins(
        algorithms::generateChunkOrigins(
            {
                0,
                0,
                0
    },
            {16, 16, 16}
        ),
        {{0, 0, 0}},
        "exclusive max"
    );
    checkOrigins(
        algorithms::generateChunkOrigins(
            {
                16,
                16,
                16
    },
            {32, 32, 32}
        ),
        {{16, 16, 16}},
        "aligned origin"
    );


    checkOrigins(
        algorithms::generateChunkOrigins(
            {
                0,
                0,
                0
    },
            {17, 17, 17}
        ),
        {{0, 0, 0}, {0, 0, 16}, {0, 16, 0}, {0, 16, 16}, {16, 0, 0}, {16, 0, 16}, {16, 16, 0}, {16, 16, 16}},
        "three-axis boundary order"
    );


    checkOrigins(
        algorithms::generateChunkOrigins(
            {
                -17,
                -1,
                -16
    },
            {0, 1, 0}
        ),
        {{-32, -16, -16}, {-32, 0, -16}, {-16, -16, -16}, {-16, 0, -16}},
        "negative floor division"
    );
    checkOrigins(
        algorithms::generateChunkOrigins(
            {
                -16,
                -16,
                -16
    },
            {-15, -15, -15}
        ),
        {{-16, -16, -16}},
        "exact negative multiple"
    );
    checkOrigins(
        algorithms::generateChunkOrigins(
            {
                -1,
                -1,
                -1
    },
            {1, 1, 1}
        ),
        {{-16, -16, -16},
         {-16, -16, 0},
         {-16, 0, -16},
         {-16, 0, 0},
         {0, -16, -16},
         {0, -16, 0},
         {0, 0, -16},
         {0, 0, 0}},
        "cross-chunk boundary from -1"
    );


    checkOrigins(
        algorithms::generateChunkOrigins(
            {
                -.5f,
                0,
                0
    },
            {.5f, 1, 1}
        ),
        {{-16, 0, 0}, {0, 0, 0}},
        "fractional boundary"
    );


    check(algorithms::generateChunkOrigins({1, 1, 1}, {1, 2, 2}).empty(), "zero width is empty");
    check(algorithms::generateChunkOrigins({2, 0, 0}, {1, 2, 2}).empty(), "inverted bounds");
    check(algorithms::generateChunkOrigins({0, 0, 0}, {INFINITY, 1, 1}).empty(), "infinite bounds");
    check(algorithms::generateChunkOrigins({NAN, 0, 0}, {1, 1, 1}).empty(), "NaN bounds");
    check(algorithms::generateChunkOrigins({0, 0, 0}, {3e10f, 1, 1}).empty(), "integer overflow guard");
}

void testRotatedSizes() {

    checkOrigins(
        std::vector<algorithms::GridOrigin>{
            algorithms::rotatedStructureSize({5, 3, 20},
            false)
    },
        {{5, 3, 20}},
        "no rotation keeps size"
    );
    checkOrigins(
        std::vector<algorithms::GridOrigin>{
            algorithms::rotatedStructureSize({5, 3, 20},
            true)
    },
        {{20, 3, 5}},
        "90 degree swaps x/z"
    );


    auto const straight = algorithms::generateChunkOrigins({0, 0, 0}, {20, 5, 5});
    auto const swapped  = algorithms::generateChunkOrigins({0, 0, 0}, {5, 5, 20});
    check(straight.size() == 2 && swapped.size() == 2, "rotated size enumerates same chunk count");
}

void testBlockEntityBounds() {
    check(algorithms::withinBlockEntityBounds({-.009f, 0, 0}, {0, 0, 0}, {1, 1, 1}), "entity min tolerance");
    check(!algorithms::withinBlockEntityBounds({-.011f, 0, 0}, {0, 0, 0}, {1, 1, 1}), "entity outside min");
    check(!algorithms::withinBlockEntityBounds({1.004f, 0, 0}, {0, 0, 0}, {1, 1, 1}), "entity max x unchanged");
    check(!algorithms::withinBlockEntityBounds({0, 1.001f, 0}, {0, 0, 0}, {1, 1, 1}), "entity max y unchanged");
    check(!algorithms::withinBlockEntityBounds({0, 0, 1.001f}, {0, 0, 0}, {1, 1, 1}), "entity max z unchanged");
    check(algorithms::withinBlockEntityBounds({0, 0, 0}, {0, 0, 0}, {1, 1, 1}), "entity on lower face");
    check(
        algorithms::withinBlockEntityBounds({16, 16, 16}, {0, 0, 0}, {17, 33, 17}),
        "entity crosses chunk boundaries"
    );
    check(
        algorithms::withinBlockEntityBounds({-16, -1, -17}, {-17, -2, -18}, {0, 1, 0}),
        "entity negative coordinates"
    );
    check(!algorithms::withinBlockEntityBounds({NAN, 0, 0}, {0, 0, 0}, {1, 1, 1}), "entity NaN rejected");
}

} 

void testRenderDistance() {
    using algorithms::distanceSquaredToBox;
    using algorithms::withinRenderDistance;
    check(distanceSquaredToBox({5, 5, 5}, {0, 0, 0}, {16, 16, 16}) == 0, "camera inside box");
    check(withinRenderDistance({0, 0, 0}, {128, 0, 0}, {144, 16, 16}, 128), "inclusive distance boundary");
    check(!withinRenderDistance({0, 0, 0}, {129, 0, 0}, {145, 16, 16}, 128), "outside distance boundary");
    check(!withinRenderDistance({0, 0, 0}, {0, 129, 0}, {16, 145, 16}, 128), "vertical distance is included");
    check(withinRenderDistance({0, 0, 0}, {-144, -16, -16}, {-128, 0, 0}, 128), "negative boundary");
    check(!withinRenderDistance({0, 0, 0}, {-145, -16, -16}, {-129, 0, 0}, 128), "negative outside boundary");
    check(withinRenderDistance({0, 0, 0}, {-2048, 0, 0}, {2048, 16, 16}, 128), "large placement containing camera");
    check(
        withinRenderDistance({0, 0, 0}, {100000, 100000, 100000}, {100016, 100016, 100016}, 0),
        "zero disables distance limit"
    );
    check(distanceSquaredToBox({-17, -1, 16}, {-16, 0, 0}, {0, 16, 16}) == 2, "cross-chunk diagonal distance");
    check(distanceSquaredToBox({NAN, 0, 0}, {0, 0, 0}, {1, 1, 1}) == 0, "invalid camera fails open");
    check(distanceSquaredToBox({0, 0, 0}, {2, 0, 0}, {1, 1, 1}) == 0, "invalid bounds fail open");
    auto size = algorithms::rotatedStructureSize({5, 3, 20}, true);
    check(
        withinRenderDistance({-1, 0, 0}, {-21, 0, 0}, {-21.0f + size[0], 3, static_cast<float>(size[2])}, 1),
        "rotated mirrored placement uses transformed extent, not origin distance"
    );
    check(
        withinRenderDistance({0, 0, 0}, {0, 0, 0}, {1000, 16, 16}, 128),
        "outside-asset override extends coarse bounds"
    );
    check(!withinRenderDistance({-1000, -64, 32}, {-500, -64, 32}, {-484, -48, 48}, 128), "world coordinates");
    check(!withinRenderDistance({0, 0, 0}, {500, 0, 0}, {516, 16, 16}, 128), "equivalent local coordinates");
}

void testVisibleBuildPriority() {
    struct Candidate {
        int    id;
        double priority;
    };
    std::vector<Candidate> candidates{
        {0, 0},
        {1, 0},
        {2, 0},
        {3, 1}
    };
    algorithms::prioritizeVisibleBuilds(candidates, 1);
    check(
        candidates[0].id == 1 && candidates[1].id == 2 && candidates[2].id == 0,
        "equal-distance candidates rotate fairly"
    );
    check(candidates.back().id == 3, "near candidates precede far candidates");
    std::vector<Candidate> empty;
    algorithms::prioritizeVisibleBuilds(empty, 100);
    check(empty.empty(), "empty candidate queue");
    std::vector<Candidate> single{
        {5, 3}
    };
    algorithms::prioritizeVisibleBuilds(single, 100);
    check(single[0].id == 5, "rotation wraps safely");
}

void testBoxFaces() {
    using Point = std::array<float, 3>;
    std::array<std::pair<Point, Point>, 4> const bounds{
        {
         {{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
         {{-17.0f, -64.0f, -33.0f}, {18.0f, 4.0f, 16.0f}},
         {{-0.002f, -0.002f, -0.002f}, {33.002f, 1.002f, 17.002f}},
         {{-0.002f, -0.002f, -0.002f}, {17.002f, 1.002f, 33.002f}},
         }
    };
    for (auto const& [minimum, maximum] : bounds) {
        auto const         faces = algorithms::makeBoxFaces(minimum, maximum);
        std::array<int, 6> planeCounts{};
        std::array<int, 8> cornerCounts{};
        for (auto const& face : faces) {
            for (std::size_t axis = 0; axis < 3; ++axis) {
                if (face.normal[axis] == 0.0f) continue;
                bool const positive = face.normal[axis] > 0.0f;
                ++planeCounts[axis * 2 + static_cast<std::size_t>(positive)];
                for (auto const& vertex : face.vertices) {
                    check(vertex[axis] == (positive ? maximum[axis] : minimum[axis]), "box face lies on its plane");
                }
            }
            for (auto const& vertex : face.vertices) {
                std::size_t corner = 0;
                for (std::size_t axis = 0; axis < 3; ++axis) {
                    check(
                        vertex[axis] == minimum[axis] || vertex[axis] == maximum[axis],
                        "box vertex lies at a corner"
                    );
                    if (vertex[axis] == maximum[axis]) corner |= std::size_t{1} << axis;
                }
                ++cornerCounts[corner];
            }
            Point firstEdge{}, secondEdge{};
            for (std::size_t axis = 0; axis < 3; ++axis) {
                firstEdge[axis]  = face.vertices[1][axis] - face.vertices[0][axis];
                secondEdge[axis] = face.vertices[2][axis] - face.vertices[0][axis];
            }
            Point const cross{
                firstEdge[1] * secondEdge[2] - firstEdge[2] * secondEdge[1],
                firstEdge[2] * secondEdge[0] - firstEdge[0] * secondEdge[2],
                firstEdge[0] * secondEdge[1] - firstEdge[1] * secondEdge[0]
            };
            float const facing = cross[0] * face.normal[0] + cross[1] * face.normal[1] + cross[2] * face.normal[2];
            check(facing > 0.0f, "box winding faces outward");
        }
        for (auto count : planeCounts) check(count == 1, "box contains each of the six faces once");
        for (auto count : cornerCounts) check(count == 3, "box corners join exactly three faces");
    }
}

int main() {
    testBoxFaces();
    testRenderDistance();
    testVisibleBuildPriority();
    testChunkOrigins();
    testRotatedSizes();
    testBlockEntityBounds();

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "Schematic render algorithm tests passed\n";
    return EXIT_SUCCESS;
}
