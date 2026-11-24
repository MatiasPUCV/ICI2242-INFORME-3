#include "Greedy.hpp"

#include "Packing.hpp"

std::optional<Solution> FirstFit(const std::vector<Box>& boxes, const Container& container)
{
    if (!FitsAll(boxes, container))
        return std::nullopt;
        
    Solution solution;
    for (const Box& box : boxes)
    {
        std::optional<Candidate> candidate = FindFirstPlacement(solution, box, container);
        if (!candidate)
            return std::nullopt;

        ApplyPlacement(solution, box, container, *candidate);
    }
    return solution;
}

std::optional<Solution> BestFitDecreasing(const std::vector<Box>& boxes, const Container& container)
{
    if (!FitsAll(boxes, container))
        return std::nullopt;
        
    Solution solution;
    for (int boxId : SortedBoxOrder(boxes))
    {
        const Box& box = boxes[boxId];
        std::optional<Candidate> candidate = FindBestPlacement(solution, box, container, UNRESTRICTED);
        if (!candidate)
            return std::nullopt;
            
        ApplyPlacement(solution, box, container, *candidate);
    }
    return solution;
}
