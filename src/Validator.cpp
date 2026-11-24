#include "Validator.hpp"

struct AbsoluteBox {
    int x;
    int y;
    int z;
    int width;
    int depth;
    int height;
};

static bool Overlaps(const AbsoluteBox& first, const AbsoluteBox& second) {
    return first.x < second.x + second.width && second.x < first.x + first.width &&
           first.y < second.y + second.depth && second.y < first.y + first.depth &&
           first.z < second.z + second.height && second.z < first.z + first.height;
}

unsigned Validate(const Solution& solution, const std::vector<Box>& boxes,
                  const Container& container) {
    unsigned violations = 0;
    std::vector<int> counts(boxes.size(), 0);

    for (const PalletState& pallet : solution.pallets) {
        std::vector<AbsoluteBox> placed;
        int z = 0;

        for (const Layer& layer : pallet.layers) {
            for (const Placement& placement : layer.placements) {
                if (placement.boxId < 0 || placement.boxId >= static_cast<int>(boxes.size())) {
                    violations |= VIOLATION_COVERAGE;
                    continue;
                }
                const Box& box = boxes[placement.boxId];
                ++counts[placement.boxId];

                if (box.height != layer.height) {
                    violations |= VIOLATION_LAYER_HEIGHT;
                }

                bool upright = !placement.rotated && placement.width == box.width &&
                               placement.depth == box.depth;
                bool turned = placement.rotated && placement.width == box.depth &&
                              placement.depth == box.width;
                if (!upright && !turned) {
                    violations |= VIOLATION_ORIENTATION;
                }

                if (placement.x < 0 || placement.y < 0 ||
                    placement.x + placement.width > container.width ||
                    placement.y + placement.depth > container.depth ||
                    z + box.height > container.height) {
                    violations |= VIOLATION_BOUNDS;
                }

                placed.push_back(AbsoluteBox{placement.x, placement.y, z, placement.width,
                                             placement.depth, box.height});
            }
            z += layer.height;
        }

        if (z > container.height) {
            violations |= VIOLATION_PALLET_HEIGHT;
        }

        for (size_t i = 0; i < placed.size(); ++i) {
            for (size_t j = i + 1; j < placed.size(); ++j) {
                if (Overlaps(placed[i], placed[j])) {
                    violations |= VIOLATION_OVERLAP;
                }
            }
        }
    }

    for (int count : counts) {
        if (count != 1) {
            violations |= VIOLATION_COVERAGE;
        }
    }
    return violations;
}
