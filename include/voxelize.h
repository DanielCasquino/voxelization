#include <cstdint>
#include <vector>

struct Triangle
{
    float v[3][3]; // each row is a vertex, cols is x,y,z
};

std::vector<uint64_t> Voxelize();