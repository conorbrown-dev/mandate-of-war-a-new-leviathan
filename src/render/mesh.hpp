#ifndef RTS_RENDER_MESH_HPP
#define RTS_RENDER_MESH_HPP

#include <vector>
#include <glm/glm.hpp>

namespace rts {

struct MeshVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

struct Mesh {
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
};

inline Mesh create_cube(float size = 1.0f) {
    Mesh mesh;
    float half = size / 2.0f;
    
    // Cube vertices (position, normal, uv)
    mesh.vertices.resize(24);
    mesh.vertices[0]  = {{-half, -half, half}, {0, 0, 1}, {0, 0}};
    mesh.vertices[1]  = {{ half, -half, half}, {0, 0, 1}, {1, 0}};
    mesh.vertices[2]  = {{ half,  half, half}, {0, 0, 1}, {1, 1}};
    mesh.vertices[3]  = {{-half,  half, half}, {0, 0, 1}, {0, 1}};
    mesh.vertices[4]  = {{ half, -half, -half}, {0, 0, -1}, {0, 0}};
    mesh.vertices[5]  = {{-half, -half, -half}, {0, 0, -1}, {1, 0}};
    mesh.vertices[6]  = {{-half,  half, -half}, {0, 0, -1}, {1, 1}};
    mesh.vertices[7]  = {{ half,  half, -half}, {0, 0, -1}, {0, 1}};
    mesh.vertices[8]  = {{-half, half, -half}, {0, 1, 0}, {0, 0}};
    mesh.vertices[9]  = {{ half, half, -half}, {0, 1, 0}, {1, 0}};
    mesh.vertices[10] = {{ half, half,  half}, {0, 1, 0}, {1, 1}};
    mesh.vertices[11] = {{-half, half,  half}, {0, 1, 0}, {0, 1}};
    mesh.vertices[12] = {{-half, -half, half}, {0, -1, 0}, {0, 0}};
    mesh.vertices[13] = {{ half, -half, half}, {0, -1, 0}, {1, 0}};
    mesh.vertices[14] = {{ half, -half, -half}, {0, -1, 0}, {1, 1}};
    mesh.vertices[15] = {{-half, -half, -half}, {0, -1, 0}, {0, 1}};
    mesh.vertices[16] = {{half, -half, -half}, {1, 0, 0}, {0, 0}};
    mesh.vertices[17] = {{half, -half, half}, {1, 0, 0}, {1, 0}};
    mesh.vertices[18] = {{half, half, half}, {1, 0, 0}, {1, 1}};
    mesh.vertices[19] = {{half, half, -half}, {1, 0, 0}, {0, 1}};
    mesh.vertices[20] = {{-half, -half, half}, {-1, 0, 0}, {0, 0}};
    mesh.vertices[21] = {{-half, -half, -half}, {-1, 0, 0}, {1, 0}};
    mesh.vertices[22] = {{-half, half, -half}, {-1, 0, 0}, {1, 1}};
    mesh.vertices[23] = {{-half, half, half}, {-1, 0, 0}, {0, 1}};
    
    mesh.indices.resize(36);
    mesh.indices[0]  = 0;  mesh.indices[1]  = 1;  mesh.indices[2]  = 2;
    mesh.indices[3]  = 0;  mesh.indices[4]  = 2;  mesh.indices[5]  = 3;
    mesh.indices[6]  = 4;  mesh.indices[7]  = 5;  mesh.indices[8]  = 6;
    mesh.indices[9]  = 4;  mesh.indices[10] = 6;  mesh.indices[11] = 7;
    mesh.indices[12] = 8;  mesh.indices[13] = 9;  mesh.indices[14] = 10;
    mesh.indices[15] = 8;  mesh.indices[16] = 10; mesh.indices[17] = 11;
    mesh.indices[18] = 12; mesh.indices[19] = 13; mesh.indices[20] = 14;
    mesh.indices[21] = 12; mesh.indices[22] = 14; mesh.indices[23] = 15;
    mesh.indices[24] = 16; mesh.indices[25] = 17; mesh.indices[26] = 18;
    mesh.indices[27] = 16; mesh.indices[28] = 18; mesh.indices[29] = 19;
    mesh.indices[30] = 20; mesh.indices[31] = 21; mesh.indices[32] = 22;
    mesh.indices[33] = 20; mesh.indices[34] = 22; mesh.indices[35] = 23;
    
    return mesh;
}

} // namespace rts

#endif // RTS_RENDER_MESH_HPP
