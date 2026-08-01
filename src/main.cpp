#include<iostream>
#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<cmath>
#include<fstream>
#include<sstream>
#include<string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <map>
#include <cfloat>
#include <algorithm>

// ==================== STRUCTS ====================

// AABB - Axis Aligned Bounding Box
struct AABB {
    glm::vec3 min;
    glm::vec3 max;
    
    AABB() : min(FLT_MAX), max(-FLT_MAX) {}
    AABB(const glm::vec3& min, const glm::vec3& max) : min(min), max(max) {}
    
    glm::vec3 getCenter() const { return (min + max) * 0.5f; }
    glm::vec3 getSize() const { return max - min; }
    
    void expand(const glm::vec3& point) {
        min = glm::min(min, point);
        max = glm::max(max, point);
    }
    
    void expand(const AABB& other) {
        min = glm::min(min, other.min);
        max = glm::max(max, other.max);
    }
    
    bool intersects(const AABB& other) const {
        return (min.x <= other.max.x && max.x >= other.min.x) &&
               (min.y <= other.max.y && max.y >= other.min.y) &&
               (min.z <= other.max.z && max.z >= other.min.z);
    }
    
    bool containsPoint(const glm::vec3& point) const {
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }
};

// Physics Body
struct RigidBody {
    glm::vec3 position;
    glm::vec3 velocity;
    float mass;
    float restitution;  // Bounciness (0-1)
    AABB aabb;
    AABB aabbWorld;
    bool isStatic;
    bool isGrounded;
    bool isTrigger;
    
    RigidBody() : 
        position(0.0f),
        velocity(0.0f),
        mass(1.0f),
        restitution(0.5f),
        isStatic(false),
        isGrounded(false),
        isTrigger(false) {}
    
    void updateAABB() {
        aabbWorld.min = aabb.min + position;
        aabbWorld.max = aabb.max + position;
    }
};

// Collision Info
struct Collision {
    RigidBody* a;
    RigidBody* b;
    glm::vec3 normal;
    float depth;
};

// OBJ Face
struct OBJFace {
    std::vector<int> vIndices;
    std::vector<int> vtIndices;
    std::vector<int> vnIndices;
};

// Material
struct Material {
    std::string name;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    float shininess;
    std::string diffuseMap;
    std::string specularMap;
    std::string normalMap;
    float transparency;
    float refraction;
    int illumModel;
    
    Material() : 
        ambient(0.2f, 0.2f, 0.2f),
        diffuse(0.8f, 0.8f, 0.8f),
        specular(0.0f, 0.0f, 0.0f),
        shininess(32.0f),
        transparency(1.0f),
        refraction(1.0f),
        illumModel(1) {}
};

// OBJ Group with AABB
struct OBJGroup {
    std::string name;
    std::string materialName;
    std::vector<GLuint> indices;
    AABB aabb;
    glm::vec3 center;
    float radius;
};

// ==================== PHYSICS WORLD ====================

class PhysicsWorld {
private:
    std::vector<RigidBody*> bodies;
    glm::vec3 gravity;
    float fixedDeltaTime;
    
public:
    PhysicsWorld() : gravity(0.0f, -9.81f, 0.0f), fixedDeltaTime(1.0f / 60.0f) {}
    
    void addBody(RigidBody* body) {
        bodies.push_back(body);
    }
    
    void removeBody(RigidBody* body) {
        auto it = std::find(bodies.begin(), bodies.end(), body);
        if (it != bodies.end()) {
            bodies.erase(it);
        }
    }
    
    void setGravity(const glm::vec3& g) { gravity = g; }
    glm::vec3 getGravity() const { return gravity; }
    
    void update(float deltaTime) {
        float accumulator = 0.0f;
        accumulator += deltaTime;
        
        while (accumulator >= fixedDeltaTime) {
            step();
            accumulator -= fixedDeltaTime;
        }
    }
    
    // Get all bodies (for debug rendering)
    const std::vector<RigidBody*>& getBodies() const { return bodies; }
    
private:
    void step() {
        // 1. Apply forces (gravity)
        for (auto* body : bodies) {
            if (!body->isStatic) {
                body->velocity += gravity * fixedDeltaTime;
            }
        }
        
        // 2. Integrate positions
        for (auto* body : bodies) {
            if (!body->isStatic) {
                body->position += body->velocity * fixedDeltaTime;
            }
            body->updateAABB();
        }
        
        // 3. Detect and resolve collisions
        for (size_t i = 0; i < bodies.size(); i++) {
            for (size_t j = i + 1; j < bodies.size(); j++) {
                RigidBody* a = bodies[i];
                RigidBody* b = bodies[j];
                
                if (a->isStatic && b->isStatic) continue;
                if (a->isTrigger || b->isTrigger) continue;
                
                if (a->aabbWorld.intersects(b->aabbWorld)) {
                    resolveCollision(a, b);
                }
            }
        }
    }
    
    void resolveCollision(RigidBody* a, RigidBody* b) {
        // Calculate overlap on each axis
        glm::vec3 overlap;
        overlap.x = std::min(a->aabbWorld.max.x - b->aabbWorld.min.x,
                            b->aabbWorld.max.x - a->aabbWorld.min.x);
        overlap.y = std::min(a->aabbWorld.max.y - b->aabbWorld.min.y,
                            b->aabbWorld.max.y - a->aabbWorld.min.y);
        overlap.z = std::min(a->aabbWorld.max.z - b->aabbWorld.min.z,
                            b->aabbWorld.max.z - a->aabbWorld.min.z);
        
        // Find smallest overlap axis (collision normal)
        glm::vec3 normal;
        float depth;
        
        if (overlap.x < overlap.y && overlap.x < overlap.z) {
            normal = glm::vec3(1.0f, 0.0f, 0.0f);
            depth = overlap.x;
        } else if (overlap.y < overlap.z) {
            normal = glm::vec3(0.0f, 1.0f, 0.0f);
            depth = overlap.y;
        } else {
            normal = glm::vec3(0.0f, 0.0f, 1.0f);
            depth = overlap.z;
        }
        
        // Push bodies apart
        float totalMass = a->mass + b->mass;
        
        if (!a->isStatic) {
            a->position -= normal * depth * (b->mass / totalMass);
        }
        if (!b->isStatic) {
            b->position += normal * depth * (a->mass / totalMass);
        }
        
        // Update AABBs
        a->updateAABB();
        b->updateAABB();
        
        // Velocity resolution (bounce)
        glm::vec3 relativeVelocity = a->velocity - b->velocity;
        float velAlongNormal = glm::dot(relativeVelocity, normal);
        
        if (velAlongNormal < 0.0f) {
            float e = (a->restitution + b->restitution) * 0.5f;
            float impulseMagnitude = -(1.0f + e) * velAlongNormal / (1.0f/a->mass + 1.0f/b->mass);
            glm::vec3 impulse = impulseMagnitude * normal;
            
            if (!a->isStatic) a->velocity += impulse / a->mass;
            if (!b->isStatic) b->velocity -= impulse / b->mass;
            
            // Ground check
            if (normal.y > 0.5f) {
                a->isGrounded = true;
                b->isGrounded = true;
            }
        }
    }
};

// ==================== FUNCTION DECLARATIONS ====================

std::string readShaderFile(const char* filepath);
GLuint compileShader(GLenum type, const char* source);
void loadMTL(const char* filepath, std::map<std::string, Material>& materials);
void loadOBJ(const char* filepath, 
             std::vector<glm::vec3>& outVertices, 
             std::vector<GLuint>& outIndices,
             std::vector<glm::vec2>* outUVs = nullptr,
             std::vector<glm::vec3>* outNormals = nullptr,
             std::map<std::string, Material>* outMaterials = nullptr,
             std::vector<OBJGroup>* outGroups = nullptr,
             AABB* outOverallAABB = nullptr);

void generateNormals(const std::vector<glm::vec3>& vertices, 
                     const std::vector<GLuint>& indices, 
                     std::vector<glm::vec3>& outNormals);

void calculateGroupAABB(const std::vector<glm::vec3>& vertices, OBJGroup& group);

void setMaterialUniforms(GLuint shaderProgram, const Material& material);
void setLightingUniforms(GLuint shaderProgram, const glm::vec3& lightPos, 
                         const glm::vec3& lightColor, const glm::vec3& viewPos);

// ==================== FUNCTION IMPLEMENTATIONS ====================

std::string readShaderFile(const char* filepath) {
    std::ifstream file(filepath);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
        return 0;
    }
    return shader;
}

void generateNormals(const std::vector<glm::vec3>& vertices, 
                     const std::vector<GLuint>& indices, 
                     std::vector<glm::vec3>& outNormals) {
    outNormals.clear();
    outNormals.resize(vertices.size(), glm::vec3(0.0f));
    
    for (size_t i = 0; i < indices.size(); i += 3) {
        glm::vec3 v0 = vertices[indices[i]];
        glm::vec3 v1 = vertices[indices[i + 1]];
        glm::vec3 v2 = vertices[indices[i + 2]];
        
        glm::vec3 edge1 = v1 - v0;
        glm::vec3 edge2 = v2 - v0;
        glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));
        
        outNormals[indices[i]] += normal;
        outNormals[indices[i + 1]] += normal;
        outNormals[indices[i + 2]] += normal;
    }
    
    for (auto& n : outNormals) {
        if (glm::length(n) > 0.0f) {
            n = glm::normalize(n);
        } else {
            n = glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }
    
    std::cout << "Generated " << outNormals.size() << " normals" << std::endl;
}

void calculateGroupAABB(const std::vector<glm::vec3>& vertices, OBJGroup& group) {
    group.aabb = AABB();
    if (group.indices.empty()) return;
    
    for (GLuint idx : group.indices) {
        if (idx < vertices.size()) {
            group.aabb.expand(vertices[idx]);
        }
    }
    
    group.center = group.aabb.getCenter();
    float maxDist = 0.0f;
    for (GLuint idx : group.indices) {
        if (idx < vertices.size()) {
            float dist = glm::distance(vertices[idx], group.center);
            maxDist = std::max(maxDist, dist);
        }
    }
    group.radius = maxDist;
}

void setMaterialUniforms(GLuint shaderProgram, const Material& material) {
    glUniform3fv(glGetUniformLocation(shaderProgram, "materialAmbient"), 1, glm::value_ptr(material.ambient));
    glUniform3fv(glGetUniformLocation(shaderProgram, "materialDiffuse"), 1, glm::value_ptr(material.diffuse));
    glUniform3fv(glGetUniformLocation(shaderProgram, "materialSpecular"), 1, glm::value_ptr(material.specular));
    glUniform1f(glGetUniformLocation(shaderProgram, "materialShininess"), material.shininess);
    glUniform1f(glGetUniformLocation(shaderProgram, "materialTransparency"), material.transparency);
}

void setLightingUniforms(GLuint shaderProgram, const glm::vec3& lightPos, 
                         const glm::vec3& lightColor, const glm::vec3& viewPos) {
    glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, glm::value_ptr(lightPos));
    glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, glm::value_ptr(lightColor));
    glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(viewPos));
}

void loadMTL(const char* filepath, std::map<std::string, Material>& materials) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cout << "WARNING: Cannot open MTL file: " << filepath << std::endl;
        return;
    }
    std::cout << "Loading MTL: " << filepath << std::endl;
    
    std::string line;
    Material currentMaterial;
    bool hasMaterial = false;
    
    while (std::getline(file, line)) {
        if (!line.empty() && line[line.length() - 1] == '\r') {
            line.erase(line.length() - 1);
        }
        if (line.empty() || line[0] == '#') continue;
        
        if (line.substr(0, 6) == "newmtl") {
            if (hasMaterial) {
                materials[currentMaterial.name] = currentMaterial;
            }
            currentMaterial = Material();
            currentMaterial.name = line.substr(7);
            currentMaterial.name.erase(0, currentMaterial.name.find_first_not_of(" \t"));
            currentMaterial.name.erase(currentMaterial.name.find_last_not_of(" \t") + 1);
            hasMaterial = true;
        }
        else if (line.substr(0, 2) == "Ka") {
            float r, g, b;
            if (sscanf(line.c_str(), "Ka %f %f %f", &r, &g, &b) == 3) {
                currentMaterial.ambient = glm::vec3(r, g, b);
            }
        }
        else if (line.substr(0, 2) == "Kd") {
            float r, g, b;
            if (sscanf(line.c_str(), "Kd %f %f %f", &r, &g, &b) == 3) {
                currentMaterial.diffuse = glm::vec3(r, g, b);
            }
        }
        else if (line.substr(0, 2) == "Ks") {
            float r, g, b;
            if (sscanf(line.c_str(), "Ks %f %f %f", &r, &g, &b) == 3) {
                currentMaterial.specular = glm::vec3(r, g, b);
            }
        }
        else if (line.substr(0, 2) == "Ns") {
            float ns;
            if (sscanf(line.c_str(), "Ns %f", &ns) == 1) {
                currentMaterial.shininess = ns;
            }
        }
        else if (line[0] == 'd' && line[1] == ' ') {
            float d;
            if (sscanf(line.c_str(), "d %f", &d) == 1) {
                currentMaterial.transparency = d;
            }
        }
        else if (line.substr(0, 6) == "map_Kd") {
            currentMaterial.diffuseMap = line.substr(7);
            currentMaterial.diffuseMap.erase(0, currentMaterial.diffuseMap.find_first_not_of(" \t"));
            currentMaterial.diffuseMap.erase(currentMaterial.diffuseMap.find_last_not_of(" \t") + 1);
        }
    }
    
    if (hasMaterial) {
        materials[currentMaterial.name] = currentMaterial;
    }
    
    std::cout << "Loaded " << materials.size() << " materials" << std::endl;
}

void loadOBJ(const char* filepath, 
             std::vector<glm::vec3>& outVertices, 
             std::vector<GLuint>& outIndices,
             std::vector<glm::vec2>* outUVs,
             std::vector<glm::vec3>* outNormals,
             std::map<std::string, Material>* outMaterials,
             std::vector<OBJGroup>* outGroups,
             AABB* outOverallAABB) {
    
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cout << "ERROR: Cannot open file: " << filepath << std::endl;
        return;
    }
    std::cout << "Successfully opened: " << filepath << std::endl;
    
    std::vector<glm::vec3> tempPositions;
    std::vector<glm::vec2> tempUVs;
    std::vector<glm::vec3> tempNormals;
    std::vector<OBJFace> allFaces;
    
    std::string currentGroup = "default";
    std::string currentMaterial = "";
    OBJGroup group;
    group.name = "default";
    
    AABB overallAABB;
    bool hasGeometry = false;
    
    std::string mtlPath;
    std::string baseDir = filepath;
    size_t lastSlash = baseDir.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        baseDir = baseDir.substr(0, lastSlash + 1);
    } else {
        baseDir = "";
    }
    
    std::string line;
    
    while (std::getline(file, line)) {
        if (!line.empty() && line[line.length() - 1] == '\r') {
            line.erase(line.length() - 1);
        }
        if (line.empty() || line[0] == '#') continue;
        
        if (line[0] == 'v' && line[1] == ' ') {
            float x, y, z;
            if (sscanf(line.c_str(), "v %f %f %f", &x, &y, &z) == 3) {
                tempPositions.push_back(glm::vec3(x, y, z));
                overallAABB.expand(glm::vec3(x, y, z));
                hasGeometry = true;
            }
        }
        else if (line[0] == 'v' && line[1] == 't') {
            float u, v;
            if (sscanf(line.c_str(), "vt %f %f", &u, &v) == 2) {
                tempUVs.push_back(glm::vec2(u, v));
            }
        }
        else if (line[0] == 'v' && line[1] == 'n') {
            float x, y, z;
            if (sscanf(line.c_str(), "vn %f %f %f", &x, &y, &z) == 3) {
                tempNormals.push_back(glm::vec3(x, y, z));
            }
        }
        else if (line.substr(0, 6) == "mtllib") {
            mtlPath = line.substr(7);
            mtlPath.erase(0, mtlPath.find_first_not_of(" \t"));
            mtlPath.erase(mtlPath.find_last_not_of(" \t") + 1);
            std::cout << "Material library: " << mtlPath << std::endl;
            
            if (outMaterials) {
                std::string fullMtlPath = baseDir + mtlPath;
                loadMTL(fullMtlPath.c_str(), *outMaterials);
            }
        }
        else if (line[0] == 'o' && line[1] == ' ') {
            if (outGroups && !group.indices.empty()) {
                calculateGroupAABB(outVertices, group);
                outGroups->push_back(group);
                group.indices.clear();
            }
            
            currentGroup = line.substr(2);
            currentGroup.erase(0, currentGroup.find_first_not_of(" \t"));
            currentGroup.erase(currentGroup.find_last_not_of(" \t") + 1);
            
            if (outGroups) {
                group.name = currentGroup;
                group.materialName = currentMaterial;
                group.aabb = AABB();
            }
        }
        else if (line.substr(0, 6) == "usemtl") {
            currentMaterial = line.substr(7);
            currentMaterial.erase(0, currentMaterial.find_first_not_of(" \t"));
            currentMaterial.erase(currentMaterial.find_last_not_of(" \t") + 1);
            
            if (outGroups && !group.indices.empty()) {
                calculateGroupAABB(outVertices, group);
                outGroups->push_back(group);
                group.indices.clear();
            }
            if (outGroups) {
                group.materialName = currentMaterial;
                group.aabb = AABB();
            }
        }
        else if (line[0] == 'f' && line[1] == ' ') {
            OBJFace face;
            
            std::string faceData = line.substr(2);
            size_t pos = 0;
            
            while (pos < faceData.length()) {
                while (pos < faceData.length() && faceData[pos] == ' ') pos++;
                if (pos >= faceData.length()) break;
                
                size_t end = faceData.find(' ', pos);
                if (end == std::string::npos) end = faceData.length();
                
                std::string groupStr = faceData.substr(pos, end - pos);
                
                int vIdx = -1, vtIdx = -1, vnIdx = -1;
                
                size_t slash1 = groupStr.find('/');
                if (slash1 == std::string::npos) {
                    vIdx = std::stoi(groupStr);
                } else {
                    std::string vPart = groupStr.substr(0, slash1);
                    if (!vPart.empty()) vIdx = std::stoi(vPart);
                    
                    size_t slash2 = groupStr.find('/', slash1 + 1);
                    if (slash2 == std::string::npos) {
                        std::string vtPart = groupStr.substr(slash1 + 1);
                        if (!vtPart.empty()) vtIdx = std::stoi(vtPart);
                    } else {
                        std::string vtPart = groupStr.substr(slash1 + 1, slash2 - slash1 - 1);
                        if (!vtPart.empty()) vtIdx = std::stoi(vtPart);
                        
                        std::string vnPart = groupStr.substr(slash2 + 1);
                        if (!vnPart.empty()) vnIdx = std::stoi(vnPart);
                    }
                }
                
                if (vIdx > 0) face.vIndices.push_back(vIdx - 1);
                if (vtIdx > 0) face.vtIndices.push_back(vtIdx - 1);
                if (vnIdx > 0) face.vnIndices.push_back(vnIdx - 1);
                
                pos = end;
            }
            
            if (face.vIndices.size() >= 3) {
                allFaces.push_back(face);
                
                if (outGroups) {
                    for (size_t i = 1; i < face.vIndices.size() - 1; i++) {
                        group.indices.push_back(face.vIndices[0]);
                        group.indices.push_back(face.vIndices[i]);
                        group.indices.push_back(face.vIndices[i + 1]);
                    }
                }
            }
        }
    }
    
    file.close();
    
    if (outGroups && !group.indices.empty()) {
        calculateGroupAABB(outVertices, group);
        outGroups->push_back(group);
    }
    
    if (outOverallAABB && hasGeometry) {
        *outOverallAABB = overallAABB;
        std::cout << "Overall AABB:" << std::endl;
        std::cout << "  Min: " << overallAABB.min.x << ", " 
                  << overallAABB.min.y << ", " << overallAABB.min.z << std::endl;
        std::cout << "  Max: " << overallAABB.max.x << ", " 
                  << overallAABB.max.y << ", " << overallAABB.max.z << std::endl;
        std::cout << "  Size: " << overallAABB.getSize().x << " x " 
                  << overallAABB.getSize().y << " x " << overallAABB.getSize().z << std::endl;
    }
    
    std::cout << "Parsed " << tempPositions.size() << " positions, "
              << tempUVs.size() << " UVs, "
              << tempNormals.size() << " normals, "
              << allFaces.size() << " faces" << std::endl;
    
    if (allFaces.empty()) {
        std::cout << "No faces found in OBJ file" << std::endl;
        return;
    }
    
    for (const auto& face : allFaces) {
        for (size_t i = 1; i < face.vIndices.size() - 1; i++) {
            int idx0 = face.vIndices[0];
            int idx1 = face.vIndices[i];
            int idx2 = face.vIndices[i + 1];
            
            outVertices.push_back(tempPositions[idx0]);
            outVertices.push_back(tempPositions[idx1]);
            outVertices.push_back(tempPositions[idx2]);
            
            size_t baseIndex = outVertices.size() - 3;
            outIndices.push_back(baseIndex);
            outIndices.push_back(baseIndex + 1);
            outIndices.push_back(baseIndex + 2);
            
            if (outNormals && !tempNormals.empty() && !face.vnIndices.empty()) {
                int vn0 = face.vnIndices[0];
                int vn1 = face.vnIndices[i];
                int vn2 = face.vnIndices[i + 1];
                outNormals->push_back(tempNormals[vn0]);
                outNormals->push_back(tempNormals[vn1]);
                outNormals->push_back(tempNormals[vn2]);
            }
            
            if (outUVs && !tempUVs.empty() && !face.vtIndices.empty()) {
                int vt0 = face.vtIndices[0];
                int vt1 = face.vtIndices[i];
                int vt2 = face.vtIndices[i + 1];
                outUVs->push_back(tempUVs[vt0]);
                outUVs->push_back(tempUVs[vt1]);
                outUVs->push_back(tempUVs[vt2]);
            }
        }
    }
    
    std::cout << "Generated " << outVertices.size() << " vertices and " 
              << outIndices.size() << " indices" << std::endl;
    
    if (outVertices.size() > 0) {
        std::cout << "First vertex: " << outVertices[0].x << ", " 
                  << outVertices[0].y << ", " << outVertices[0].z << std::endl;
    }
}

// ==================== CAMERA CONTROLS ====================

glm::vec3 cameraPos = glm::vec3(0.0f, 60.0f, 300.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
float cameraYaw = -90.0f;
float cameraPitch = 0.0f;
float cameraSpeed = 50.0f;
float mouseSensitivity = 0.1f;
bool firstMouse = true;
float lastX = 400.0f, lastY = 400.0f;

void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    xoffset *= mouseSensitivity;
    yoffset *= mouseSensitivity;

    cameraYaw += xoffset;
    cameraPitch += yoffset;

    if (cameraPitch > 89.0f) cameraPitch = 89.0f;
    if (cameraPitch < -89.0f) cameraPitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
    front.y = sin(glm::radians(cameraPitch));
    front.z = sin(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
    cameraFront = glm::normalize(front);
}

void processInput(GLFWwindow* window, float deltaTime) {
    float speed = cameraSpeed * deltaTime;
    
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += speed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= speed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * speed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * speed;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        cameraPos += speed * cameraUp;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        cameraPos -= speed * cameraUp;
    
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        cameraPos = glm::vec3(0.0f, 60.0f, 300.0f);
        cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
        cameraYaw = -90.0f;
        cameraPitch = 0.0f;
    }
}

// ==================== MAIN ====================

int main()
{
    // Initialize GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 800, "Physics Engine", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    gladLoadGL();
    glViewport(0, 0, 800, 800);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // Load shaders
    std::string vertexShaderSource = readShaderFile("shaders/vertshader.vert");
    std::string fragmentShaderSource = readShaderFile("shaders/fragshader.frag");

    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource.c_str());
    if (vertexShader == 0) return -1;

    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource.c_str());
    if (fragmentShader == 0) return -1;

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    GLint success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "ERROR::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        return -1;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // ====== LOAD OBJ WITH AABB ======
    std::vector<glm::vec3> vertices;
    std::vector<GLuint> indices;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec3> normals;
    std::map<std::string, Material> materials;
    std::vector<OBJGroup> groups;
    AABB overallAABB;
    
    loadOBJ("assets/car.obj", 
            vertices, 
            indices,
            &uvs,
            &normals,
            &materials,
            &groups,
            &overallAABB);

    // Generate normals if missing
    if (normals.empty() && !vertices.empty()) {
        std::cout << "Generating normals..." << std::endl;
        generateNormals(vertices, indices, normals);
    }

    // Calculate car properties
    glm::vec3 carCenter = overallAABB.getCenter();
    float carSize = glm::length(overallAABB.getSize());

    std::cout << "\n=== Car Info ===" << std::endl;
    std::cout << "Center: " << carCenter.x << ", " << carCenter.y << ", " << carCenter.z << std::endl;
    std::cout << "Size: " << carSize << std::endl;

    // ====== SETUP CAMERA ======
    cameraPos = carCenter + glm::vec3(0.0f, 60.0f, 300.0f);
    cameraFront = glm::normalize(carCenter - cameraPos);
    
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f/800.0f, 0.1f, 5000.0f);

    // ====== CONVERT VERTEX DATA ======
    std::vector<GLfloat> vertexData;
    std::vector<GLfloat> normalData;
    std::vector<GLfloat> uvData;

    for (size_t i = 0; i < vertices.size(); i++) {
        vertexData.push_back(vertices[i].x);
        vertexData.push_back(vertices[i].y);
        vertexData.push_back(vertices[i].z);
        
        if (!normals.empty() && i < normals.size()) {
            normalData.push_back(normals[i].x);
            normalData.push_back(normals[i].y);
            normalData.push_back(normals[i].z);
        }
        
        if (!uvs.empty() && i < uvs.size()) {
            uvData.push_back(uvs[i].x);
            uvData.push_back(uvs[i].y);
        }
    }

    // ====== SETUP VAO/VBO/EBO ======
    GLuint VAO, VBO, EBO, NBO, UVBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glGenBuffers(1, &NBO);
    glGenBuffers(1, &UVBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(GLfloat), vertexData.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(0);

    if (!normalData.empty()) {
        glBindBuffer(GL_ARRAY_BUFFER, NBO);
        glBufferData(GL_ARRAY_BUFFER, normalData.size() * sizeof(GLfloat), normalData.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
        glEnableVertexAttribArray(1);
    }

    if (!uvData.empty()) {
        glBindBuffer(GL_ARRAY_BUFFER, UVBO);
        glBufferData(GL_ARRAY_BUFFER, uvData.size() * sizeof(GLfloat), uvData.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
        glEnableVertexAttribArray(2);
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);

    // ====== CREATE PHYSICS WORLD ======
    PhysicsWorld physics;
    physics.setGravity(glm::vec3(0.0f, -9.81f, 0.0f));

    // Create car physics body
    RigidBody* carBody = new RigidBody();
    carBody->aabb = overallAABB;
    carBody->position = carCenter;
    carBody->mass = 1000.0f;
    carBody->restitution = 0.1f;
    carBody->updateAABB();
    physics.addBody(carBody);

    // Create ground
    RigidBody* ground = new RigidBody();
    ground->aabb = AABB(glm::vec3(-1000.0f, -10.0f, -1000.0f), 
                         glm::vec3(1000.0f, 0.0f, 1000.0f));
    ground->position = glm::vec3(0.0f, 0.0f, 0.0f);
    ground->isStatic = true;
    ground->updateAABB();
    physics.addBody(ground);

    // ====== LIGHTING ======
    glm::vec3 lightPos = glm::vec3(200.0f, 300.0f, 400.0f);
    glm::vec3 lightColor = glm::vec3(1.5f, 1.5f, 1.5f);

    // ====== MAIN LOOP ======
    float lastFrame = 0.0f;
    bool showPhysicsDebug = false;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Input
        processInput(window, deltaTime);

        // Toggle physics debug
        if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS) {
            showPhysicsDebug = !showPhysicsDebug;
            std::cout << "Physics Debug: " << (showPhysicsDebug ? "ON" : "OFF") << std::endl;
            glfwWaitEventsTimeout(0.2);
        }

        // Car controls
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
            carBody->velocity += glm::vec3(0.0f, 0.0f, -10.0f) * deltaTime;
        }
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
            carBody->velocity += glm::vec3(0.0f, 0.0f, 10.0f) * deltaTime;
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
            carBody->velocity += glm::vec3(-10.0f, 0.0f, 0.0f) * deltaTime;
        }
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
            carBody->velocity += glm::vec3(10.0f, 0.0f, 0.0f) * deltaTime;
        }
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            carBody->velocity.y = 15.0f;
        }

        // Physics
        physics.update(deltaTime);

        // Render
        glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float time = glfwGetTime();
        float angle = time * 0.3f;

        // Model matrix using physics position
        float scaleFactor = 0.02f;
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, carBody->position);
        model = glm::scale(model, glm::vec3(scaleFactor));
        model = glm::rotate(model, angle, glm::vec3(0.0f, 1.0f, 0.0f));

        // View matrix
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

        glUseProgram(shaderProgram);

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        setLightingUniforms(shaderProgram, lightPos, lightColor, cameraPos);

        // Draw car
        glBindVertexArray(VAO);
        
        if (!groups.empty()) {
            size_t indexOffset = 0;
            for (const auto& group : groups) {
                auto it = materials.find(group.materialName);
                if (it != materials.end()) {
                    setMaterialUniforms(shaderProgram, it->second);
                } else {
                    Material defaultMat;
                    defaultMat.diffuse = glm::vec3(1.0f, 0.5f, 0.0f);
                    defaultMat.ambient = glm::vec3(0.3f, 0.2f, 0.0f);
                    defaultMat.specular = glm::vec3(0.5f, 0.5f, 0.5f);
                    defaultMat.shininess = 32.0f;
                    setMaterialUniforms(shaderProgram, defaultMat);
                }
                
                glDrawElements(GL_TRIANGLES, group.indices.size(), GL_UNSIGNED_INT, 
                              (void*)(indexOffset * sizeof(GLuint)));
                indexOffset += group.indices.size();
            }
        } else {
            glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
        }

        // Physics debug rendering (AABB)
        if (showPhysicsDebug) {
            // Draw car AABB (green)
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            
            // You would draw AABB boxes here
            // For now, just print position
            static int frameCount = 0;
            if (frameCount++ % 60 == 0) {
                std::cout << "Car pos: " << carBody->position.x << ", " 
                          << carBody->position.y << ", " << carBody->position.z << std::endl;
                std::cout << "Car velocity: " << carBody->velocity.x << ", " 
                          << carBody->velocity.y << ", " << carBody->velocity.z << std::endl;
                std::cout << "Grounded: " << (carBody->isGrounded ? "YES" : "NO") << std::endl;
            }
            
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    if (!normalData.empty()) glDeleteBuffers(1, &NBO);
    if (!uvData.empty()) glDeleteBuffers(1, &UVBO);
    glDeleteProgram(shaderProgram);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
