// Builds the three dungeon heart meshes and skeletons: the heart on the dungeon temple's pedestal.
//
// Input (one folder):
//  - DungeonHeartObject{Healthy,Damaged,Critical}.skeleton as exported from assets-src/DungeonHeartObject.blend
//    (the animation "Pulse" is kept as it is, only the two bones are moved);
//  - DungeonTempleObject.mesh, the old temple;
//  - DungeonHeart{Healthy,Damaged,Critical}.geo written by generate_heart_geometry.py (the heart itself).
// Output (another folder): DungeonHeartObject{Healthy,Damaged,Critical}.{mesh,skeleton}, each mesh with
//  - a "Heart" submesh (material DungeonHeart<Tier>), skinned to the bones "Root" and "Pulse";
//  - a "Pedestal" submesh with the material "Obsidian": the steps and claws of the temple's metal part (without the
//    old spherical lattice that stood for the heart), bound to "Root", and the metal parts of the heart (band, collar
//    and struts) that come with the .geo file. The claws are tilted outwards and stretched up to stand around the bigger heart.
//
// Build (Visual Studio developer prompt, Ogre installed in %D%):
//   cl /EHsc /MD /I%D%\include /I%D%\include\OGRE build_heart_on_temple.cpp /link /LIBPATH:%D%\lib OgreMain.lib
// Run: build_heart_on_temple <input folder> <output folder>

#include <Ogre.h>
#include <OgreMeshSerializer.h>
#include <OgreSkeletonSerializer.h>
#include <OgreDataStream.h>
#include <OgreDefaultHardwareBufferManager.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
// Triangles that lie completely within this distance of the old lattice centre are the old heart
const float LATTICE_RADIUS = 0.68f;
const Ogre::Vector3 LATTICE_CENTRE(0.0f, 0.0f, 1.2f);
// The claws (the connected parts of the pedestal that reach higher than CLAW_MIN_HEIGHT) are tilted outwards by
// CLAW_TILT (radians) about the point (CLAW_PIVOT_RADIUS, CLAW_PIVOT_HEIGHT) of their foot and stretched in height by
// CLAW_STRETCH_Z, so that they stand around the bigger heart
const float CLAW_MIN_HEIGHT = 2.0f;
const float CLAW_TILT = 0.1f;
const float CLAW_PIVOT_RADIUS = 1.45f;
const float CLAW_PIVOT_HEIGHT = 0.03f;
const float CLAW_STRETCH_Z = 1.05f;

struct Vertex
{
    Ogre::Vector3 position;
    Ogre::Vector3 normal;
    Ogre::Vector2 uv;
    // Weight of the bone "Pulse", the rest belongs to the bone "Root"
    float pulse;
};

struct Part
{
    std::vector<Vertex> vertices;
    std::vector<unsigned short> indices;
};

Ogre::MeshPtr loadMesh(const std::string& path, const std::string& name)
{
    Ogre::MeshPtr mesh = Ogre::MeshManager::getSingleton().create(name, Ogre::RGN_DEFAULT, true);
    Ogre::MeshSerializer serializer;
    Ogre::DataStreamPtr stream(new Ogre::FileStreamDataStream(new std::ifstream(path.c_str(), std::ios::binary), true));
    serializer.importMesh(stream, mesh.get());
    return mesh;
}

void includePoint(Ogre::AxisAlignedBox& box, Ogre::Real& radius, const Ogre::Vector3& point)
{
    box.merge(point);
    radius = std::max(radius, point.length());
}

// Reads a .geo file: the pivot and the two submeshes "flesh" and "iron"
void readGeo(const std::string& path, Ogre::Vector3& pivot, Part& flesh, Part& iron)
{
    std::ifstream file(path.c_str());
    if(!file)
        OGRE_EXCEPT(Ogre::Exception::ERR_FILE_NOT_FOUND, "Cannot open " + path, "readGeo");
    Part* current = nullptr;
    std::string line;
    while(std::getline(file, line))
    {
        std::istringstream in(line);
        std::string key;
        in >> key;
        if(key == "pivot")
        {
            in >> pivot.x >> pivot.y >> pivot.z;
        }
        else if(key == "sub")
        {
            std::string name;
            in >> name;
            current = (name == "flesh") ? &flesh : &iron;
        }
        else if(key == "v" && current != nullptr)
        {
            Vertex vertex;
            in >> vertex.position.x >> vertex.position.y >> vertex.position.z >> vertex.normal.x >> vertex.normal.y
                >> vertex.normal.z >> vertex.uv.x >> vertex.uv.y >> vertex.pulse;
            current->vertices.push_back(vertex);
        }
        else if(key == "f" && current != nullptr)
        {
            unsigned int a;
            unsigned int b;
            unsigned int c;
            in >> a >> b >> c;
            current->indices.push_back(static_cast<unsigned short>(a));
            current->indices.push_back(static_cast<unsigned short>(b));
            current->indices.push_back(static_cast<unsigned short>(c));
        }
    }
}

unsigned int findRoot(std::vector<unsigned int>& parent, unsigned int i)
{
    while(parent[i] != i)
    {
        parent[i] = parent[parent[i]];
        i = parent[i];
    }
    return i;
}

// Reads the temple's first submesh ("Stacheln", the metal part) and returns its vertices and the triangles that are not
// the lattice. The claws are stretched (the normals turn with them).
void readPedestal(Ogre::Mesh* temple, Part& part)
{
    Ogre::SubMesh* sub = temple->getSubMesh(0);
    Ogre::VertexData* data = sub->vertexData;
    const Ogre::VertexElement* position = data->vertexDeclaration->findElementBySemantic(Ogre::VES_POSITION);
    const Ogre::VertexElement* normal = data->vertexDeclaration->findElementBySemantic(Ogre::VES_NORMAL);
    const Ogre::VertexElement* uv = data->vertexDeclaration->findElementBySemantic(Ogre::VES_TEXTURE_COORDINATES);
    Ogre::HardwareVertexBufferSharedPtr buffer = data->vertexBufferBinding->getBuffer(position->getSource());
    unsigned char* base = static_cast<unsigned char*>(buffer->lock(Ogre::HardwareBuffer::HBL_READ_ONLY));
    std::vector<Vertex> all(data->vertexCount);
    for(size_t i = 0; i < data->vertexCount; ++i)
    {
        unsigned char* vertex = base + i * buffer->getVertexSize();
        float* p;
        float* n;
        float* t;
        position->baseVertexPointerToElement(vertex, &p);
        normal->baseVertexPointerToElement(vertex, &n);
        uv->baseVertexPointerToElement(vertex, &t);
        all[i].position = Ogre::Vector3(p[0], p[1], p[2]);
        all[i].normal = Ogre::Vector3(n[0], n[1], n[2]);
        all[i].uv = Ogre::Vector2(t[0], t[1]);
        all[i].pulse = 0.0f;
    }
    buffer->unlock();

    Ogre::HardwareIndexBufferSharedPtr index = sub->indexData->indexBuffer;
    std::vector<unsigned int> triangles(sub->indexData->indexCount);
    const bool wide = index->getType() == Ogre::HardwareIndexBuffer::IT_32BIT;
    void* raw = index->lock(Ogre::HardwareBuffer::HBL_READ_ONLY);
    for(size_t i = 0; i < triangles.size(); ++i)
        triangles[i] = wide ? static_cast<unsigned int*>(raw)[i] : static_cast<unsigned short*>(raw)[i];
    index->unlock();

    // The claws are the connected parts that reach high
    std::vector<unsigned int> parent(all.size());
    for(size_t i = 0; i < parent.size(); ++i)
        parent[i] = static_cast<unsigned int>(i);
    for(size_t i = 0; i + 2 < triangles.size(); i += 3)
    {
        parent[findRoot(parent, triangles[i + 1])] = findRoot(parent, triangles[i]);
        parent[findRoot(parent, triangles[i + 2])] = findRoot(parent, triangles[i]);
    }
    std::map<unsigned int, float> height;
    for(size_t i = 0; i < all.size(); ++i)
    {
        const unsigned int root = findRoot(parent, static_cast<unsigned int>(i));
        std::map<unsigned int, float>::iterator it = height.find(root);
        if(it == height.end())
            height.insert(std::make_pair(root, all[i].position.z));
        else
            it->second = std::max(it->second, all[i].position.z);
    }
    const float PI = 3.14159265f;
    const float cosTilt = std::cos(CLAW_TILT);
    const float sinTilt = std::sin(CLAW_TILT);
    for(size_t i = 0; i < all.size(); ++i)
    {
        if(height[findRoot(parent, static_cast<unsigned int>(i))] < CLAW_MIN_HEIGHT)
            continue;
        // The claws stand on the diagonals: work in the plane of the nearest one (radius along it, tangent across it)
        Ogre::Vector3& p = all[i].position;
        const float diagonal = PI / 4.0f + std::floor((std::atan2(p.y, p.x) - PI / 4.0f) / (PI / 2.0f) + 0.5f) * PI / 2.0f;
        const float dx = std::cos(diagonal);
        const float dy = std::sin(diagonal);
        const float radius = p.x * dx + p.y * dy;
        const float across = -p.x * dy + p.y * dx;
        const float up = p.z - CLAW_PIVOT_HEIGHT;
        const float outward = radius - CLAW_PIVOT_RADIUS;
        const float newRadius = CLAW_PIVOT_RADIUS + outward * cosTilt + up * sinTilt;
        const float newUp = -outward * sinTilt + up * cosTilt;
        p = Ogre::Vector3(newRadius * dx - across * dy, newRadius * dy + across * dx,
            CLAW_PIVOT_HEIGHT + newUp * CLAW_STRETCH_Z);
        Ogre::Vector3& n = all[i].normal;
        const float normalRadius = n.x * dx + n.y * dy;
        const float normalAcross = -n.x * dy + n.y * dx;
        const float newNormalRadius = normalRadius * cosTilt + n.z * sinTilt;
        const float newNormalUp = -normalRadius * sinTilt + n.z * cosTilt;
        n = Ogre::Vector3(newNormalRadius * dx - normalAcross * dy, newNormalRadius * dy + normalAcross * dx, newNormalUp);
    }

    std::map<unsigned int, unsigned short> remap;
    for(size_t i = 0; i + 2 < triangles.size(); i += 3)
    {
        bool lattice = true;
        for(size_t k = 0; k < 3; ++k)
        {
            if((all[triangles[i + k]].position - LATTICE_CENTRE).length() >= LATTICE_RADIUS)
                lattice = false;
        }
        if(lattice)
            continue;
        for(size_t k = 0; k < 3; ++k)
        {
            std::map<unsigned int, unsigned short>::iterator it = remap.find(triangles[i + k]);
            if(it == remap.end())
            {
                it = remap.insert(std::make_pair(triangles[i + k], static_cast<unsigned short>(part.vertices.size()))).first;
                part.vertices.push_back(all[triangles[i + k]]);
            }
            part.indices.push_back(it->second);
        }
    }
}

// Fills a submesh: vertex data (position, normal, texture coordinates), indices and bone assignments
void fillSubMesh(Ogre::SubMesh* sub, const std::vector<Vertex>& vertices, const std::vector<unsigned short>& indices,
    unsigned short rootBone, unsigned short pulseBone, Ogre::AxisAlignedBox& box, Ogre::Real& radius)
{
    sub->useSharedVertices = false;
    sub->vertexData = new Ogre::VertexData();
    sub->vertexData->vertexCount = vertices.size();
    Ogre::VertexDeclaration* decl = sub->vertexData->vertexDeclaration;
    size_t offset = 0;
    offset += decl->addElement(0, offset, Ogre::VET_FLOAT3, Ogre::VES_POSITION).getSize();
    offset += decl->addElement(0, offset, Ogre::VET_FLOAT3, Ogre::VES_NORMAL).getSize();
    offset += decl->addElement(0, offset, Ogre::VET_FLOAT2, Ogre::VES_TEXTURE_COORDINATES, 0).getSize();
    Ogre::HardwareVertexBufferSharedPtr buffer = Ogre::HardwareBufferManager::getSingleton().createVertexBuffer(
        offset, vertices.size(), Ogre::HBU_CPU_ONLY);
    float* out = static_cast<float*>(buffer->lock(Ogre::HardwareBuffer::HBL_DISCARD));
    for(size_t i = 0; i < vertices.size(); ++i)
    {
        *out++ = vertices[i].position.x;
        *out++ = vertices[i].position.y;
        *out++ = vertices[i].position.z;
        *out++ = vertices[i].normal.x;
        *out++ = vertices[i].normal.y;
        *out++ = vertices[i].normal.z;
        *out++ = vertices[i].uv.x;
        *out++ = vertices[i].uv.y;
        includePoint(box, radius, vertices[i].position);
    }
    buffer->unlock();
    sub->vertexData->vertexBufferBinding->setBinding(0, buffer);

    sub->indexData->indexCount = indices.size();
    sub->indexData->indexStart = 0;
    sub->indexData->indexBuffer = Ogre::HardwareBufferManager::getSingleton().createIndexBuffer(
        Ogre::HardwareIndexBuffer::IT_16BIT, indices.size(), Ogre::HBU_CPU_ONLY);
    sub->indexData->indexBuffer->writeData(0, indices.size() * sizeof(unsigned short), &indices[0], true);

    for(size_t i = 0; i < vertices.size(); ++i)
    {
        const float pulse = std::min(1.0f, std::max(0.0f, vertices[i].pulse));
        if(pulse < 0.999f)
        {
            Ogre::VertexBoneAssignment assignment;
            assignment.vertexIndex = static_cast<unsigned int>(i);
            assignment.boneIndex = rootBone;
            assignment.weight = 1.0f - pulse;
            sub->addBoneAssignment(assignment);
        }
        if(pulse > 0.001f)
        {
            Ogre::VertexBoneAssignment assignment;
            assignment.vertexIndex = static_cast<unsigned int>(i);
            assignment.boneIndex = pulseBone;
            assignment.weight = pulse;
            sub->addBoneAssignment(assignment);
        }
    }
}

// Keeps the animation and moves the bones: "Root" to the origin, "Pulse" (the pivot of the beat) to the heart's centre
void convertSkeleton(const std::string& inPath, const std::string& outPath, const std::string& name,
    const Ogre::Vector3& pivot, unsigned short& rootBone, unsigned short& pulseBone)
{
    Ogre::SkeletonPtr skeleton = Ogre::SkeletonManager::getSingleton().create(name, Ogre::RGN_DEFAULT, true);
    Ogre::SkeletonSerializer serializer;
    Ogre::DataStreamPtr stream(new Ogre::FileStreamDataStream(new std::ifstream(inPath.c_str(), std::ios::binary), true));
    serializer.importSkeleton(stream, skeleton.get());
    Ogre::Bone* root = skeleton->getBone("Root");
    Ogre::Bone* pulse = skeleton->getBone("Pulse");
    root->setPosition(Ogre::Vector3::ZERO);
    pulse->setPosition(pivot);
    skeleton->setBindingPose();
    rootBone = root->getHandle();
    pulseBone = pulse->getHandle();
    serializer.exportSkeleton(skeleton.get(), outPath, Ogre::SKELETON_VERSION_1_8);
}
}

int run(int argc, char** argv)
{
    if(argc != 3)
    {
        std::cerr << "usage: build_heart_on_temple <input folder> <output folder>\n";
        return 2;
    }
    const std::string in = std::string(argv[1]) + "/";
    const std::string out = std::string(argv[2]) + "/";
    Ogre::LogManager* logManager = new Ogre::LogManager();
    logManager->createLog("build_heart_on_temple.log", true, false, false);
    Ogre::Root* root = new Ogre::Root("", "", "");
    Ogre::DefaultHardwareBufferManager* bufferManager = new Ogre::DefaultHardwareBufferManager();
    Ogre::ResourceGroupManager::getSingleton().addResourceLocation(in, "FileSystem");
    // Ogre keeps a material name in a mesh only while a material of that name exists; empty ones are enough here
    const char* materials[4] = {"Obsidian", "DungeonHeartHealthy", "DungeonHeartDamaged", "DungeonHeartCritical"};
    for(int i = 0; i < 4; ++i)
        Ogre::MaterialManager::getSingleton().create(materials[i], Ogre::RGN_DEFAULT);

    Ogre::MeshPtr temple = loadMesh(in + "DungeonTempleObject.mesh", "temple");
    Part pedestal;
    readPedestal(temple.get(), pedestal);
    std::cout << "pedestal: " << pedestal.vertices.size() << " vertices, " << pedestal.indices.size() / 3 << " triangles\n";

    const char* tiers[3] = {"Healthy", "Damaged", "Critical"};
    for(int t = 0; t < 3; ++t)
    {
        const std::string tier = tiers[t];
        const std::string base = "DungeonHeartObject" + tier;
        Ogre::Vector3 pivot = Ogre::Vector3::ZERO;
        Part flesh;
        Part iron;
        readGeo(in + "DungeonHeart" + tier + ".geo", pivot, flesh, iron);

        Part metal = pedestal;
        const unsigned short shift = static_cast<unsigned short>(metal.vertices.size());
        metal.vertices.insert(metal.vertices.end(), iron.vertices.begin(), iron.vertices.end());
        for(size_t i = 0; i < iron.indices.size(); ++i)
            metal.indices.push_back(static_cast<unsigned short>(iron.indices[i] + shift));

        unsigned short rootBone = 0;
        unsigned short pulseBone = 0;
        convertSkeleton(in + base + ".skeleton", out + base + ".skeleton", base + "_converted.skeleton", pivot, rootBone,
            pulseBone);

        Ogre::MeshPtr heart = Ogre::MeshManager::getSingleton().create(base, Ogre::RGN_DEFAULT, true);
        heart->setSkeletonName(base + ".skeleton");
        Ogre::AxisAlignedBox box;
        Ogre::Real radius = 0.0f;
        Ogre::SubMesh* fleshSub = heart->createSubMesh("Heart");
        fleshSub->setMaterialName("DungeonHeart" + tier);
        fillSubMesh(fleshSub, flesh.vertices, flesh.indices, rootBone, pulseBone, box, radius);
        Ogre::SubMesh* metalSub = heart->createSubMesh("Pedestal");
        metalSub->setMaterialName("Obsidian");
        fillSubMesh(metalSub, metal.vertices, metal.indices, rootBone, pulseBone, box, radius);
        heart->_setBounds(box, false);
        heart->_setBoundingSphereRadius(radius);
        Ogre::MeshSerializer serializer;
        serializer.exportMesh(heart.get(), out + base + ".mesh");
        std::cout << base << ": " << flesh.vertices.size() << " + " << metal.vertices.size() << " vertices, "
            << (flesh.indices.size() + metal.indices.size()) / 3 << " triangles, bounds " << box.getMinimum() << " .. "
            << box.getMaximum() << "\n";
    }
    temple.reset();
    delete root;
    delete bufferManager;
    delete logManager;
    return 0;
}

int main(int argc, char** argv)
{
    try
    {
        return run(argc, argv);
    }
    catch(const Ogre::Exception& exception)
    {
        std::cerr << exception.getFullDescription() << "\n";
        return 1;
    }
}
