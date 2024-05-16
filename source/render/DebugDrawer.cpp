#include "DebugDrawer.h"
 
#include <OgreSceneManager.h>
#include <OgreRenderQueue.h>
#include <OgreManualObject.h>
#include <OgreAxisAlignedBox.h>
#include <OgreResourceGroupManager.h>

IcoSphere::IcoSphere()
    : index(0)
{
}
 
IcoSphere::~IcoSphere()
{
}
 
void IcoSphere::create(int recursionLevel)
{
    vertices.clear();
    lineIndices.clear();
    triangleIndices.clear();
    faces.clear();
    middlePointIndexCache.clear();
    index = 0;
 
    float t = (1.0f + Ogre::Math::Sqrt(5.0f)) / 2.0f;
 
    addVertex(Ogre::Vector3(-1.0f,  t,  0.0f));
    addVertex(Ogre::Vector3( 1.0f,  t,  0.0f));
    addVertex(Ogre::Vector3(-1.0f, -t,  0.0f));
    addVertex(Ogre::Vector3( 1.0f, -t,  0.0f));
 
    addVertex(Ogre::Vector3( 0.0f, -1.0f,  t));
    addVertex(Ogre::Vector3( 0.0f,  1.0f,  t));
    addVertex(Ogre::Vector3( 0.0f, -1.0f, -t));
    addVertex(Ogre::Vector3( 0.0f,  1.0f, -t));
 
    addVertex(Ogre::Vector3( t,  0.0f, -1.0f));
    addVertex(Ogre::Vector3( t,  0.0f,  1.0f));
    addVertex(Ogre::Vector3(-t,  0.0f, -1.0f));
    addVertex(Ogre::Vector3(-t,  0.0f,  1.0f));
 
    addFace(0, 11, 5);
    addFace(0, 5, 1);
    addFace(0, 1, 7);
    addFace(0, 7, 10);
    addFace(0, 10, 11);
 
    addFace(1, 5, 9);
    addFace(5, 11, 4);
    addFace(11, 10, 2);
    addFace(10, 7, 6);
    addFace(7, 1, 8);
 
    addFace(3, 9, 4);
    addFace(3, 4, 2);
    addFace(3, 2, 6);
    addFace(3, 6, 8);
    addFace(3, 8, 9);
 
    addFace(4, 9, 5);
    addFace(2, 4, 11);
    addFace(6, 2, 10);
    addFace(8, 6, 7);
    addFace(9, 8, 1);
 
    addLineIndices(1, 0);
    addLineIndices(1, 5);
    addLineIndices(1, 7);
    addLineIndices(1, 8);
    addLineIndices(1, 9);
 
    addLineIndices(2, 3);
    addLineIndices(2, 4);
    addLineIndices(2, 6);
    addLineIndices(2, 10);
    addLineIndices(2, 11);
 
    addLineIndices(0, 5);
    addLineIndices(5, 9);
    addLineIndices(9, 8);
    addLineIndices(8, 7);
    addLineIndices(7, 0);
 
    addLineIndices(10, 11);
    addLineIndices(11, 4);
    addLineIndices(4, 3);
    addLineIndices(3, 6);
    addLineIndices(6, 10);
 
    addLineIndices(0, 11);
    addLineIndices(11, 5);
    addLineIndices(5, 4);
    addLineIndices(4, 9);
    addLineIndices(9, 3);
    addLineIndices(3, 8);
    addLineIndices(8, 6);
    addLineIndices(6, 7);
    addLineIndices(7, 10);
    addLineIndices(10, 0);
 
    for (int i = 0; i < recursionLevel; i++)
    {
        std::list<TriangleIndices> faces2;
 
        for (std::list<TriangleIndices>::iterator j = faces.begin(); j != faces.end(); j++)
        {
            TriangleIndices f = *j;
            int a = getMiddlePoint(f.v1, f.v2);
            int b = getMiddlePoint(f.v2, f.v3);
            int c = getMiddlePoint(f.v3, f.v1);
 
            removeLineIndices(f.v1, f.v2);
            removeLineIndices(f.v2, f.v3);
            removeLineIndices(f.v3, f.v1);
 
            faces2.push_back(TriangleIndices(f.v1, a, c));
            faces2.push_back(TriangleIndices(f.v2, b, a));
            faces2.push_back(TriangleIndices(f.v3, c, b));
            faces2.push_back(TriangleIndices(a, b, c));
 
            addTriangleLines(f.v1, a, c);
            addTriangleLines(f.v2, b, a);
            addTriangleLines(f.v3, c, b);
        }
 
        faces = faces2;
    }
}
 
void IcoSphere::addLineIndices(int index0, int index1)
{
    lineIndices.push_back(LineIndices(index0, index1));
}
 
void IcoSphere::removeLineIndices(int index0, int index1)
{
    std::list<LineIndices>::iterator result = std::find(lineIndices.begin(), lineIndices.end(), LineIndices(index0, index1));
 
    if (result != lineIndices.end())
        lineIndices.erase(result);
}
 
void IcoSphere::addTriangleLines(int index0, int index1, int index2)
{
    addLineIndices(index0, index1);
    addLineIndices(index1, index2);
    addLineIndices(index2, index0);
}
 
int IcoSphere::addVertex(const Ogre::Vector3 &vertex)
{
    Ogre::Real length = vertex.length();
    vertices.push_back(Ogre::Vector3(vertex.x / length, vertex.y / length, vertex.z / length));
    return index++;
}
 
int IcoSphere::getMiddlePoint(int index0, int index1)
{
    bool isFirstSmaller = index0 < index1;
    int64_t smallerIndex = isFirstSmaller ? index0 : index1;
    int64_t largerIndex = isFirstSmaller ? index1 : index0;
    int64_t key = (smallerIndex << 32) | largerIndex;
 
    if (middlePointIndexCache.find(key) != middlePointIndexCache.end())
        return middlePointIndexCache[key];
 
    Ogre::Vector3 point1 = vertices[index0];
    Ogre::Vector3 point2 = vertices[index1];
    Ogre::Vector3 middle = point1.midPoint(point2);
 
    int index = addVertex(middle);
    middlePointIndexCache[key] = index;
    return index;
}
 
void IcoSphere::addFace(int index0, int index1, int index2)
{
    faces.push_back(TriangleIndices(index0, index1, index2));
}
 
void IcoSphere::addToLineIndices(int baseIndex, std::list<int> *target)
{
    for (std::list<LineIndices>::iterator i = lineIndices.begin(); i != lineIndices.end(); i++)
    {
        target->push_back(baseIndex + (*i).v1);
        target->push_back(baseIndex + (*i).v2);
    }
}
 
void IcoSphere::addToTriangleIndices(int baseIndex, std::list<int> *target)
{
    for (std::list<TriangleIndices>::iterator i = faces.begin(); i != faces.end(); i++)
    {
        target->push_back(baseIndex + (*i).v1);
        target->push_back(baseIndex + (*i).v2);
        target->push_back(baseIndex + (*i).v3);
    }
}
 
int IcoSphere::addToVertices(std::list<VertexPair> *target, const Ogre::Vector3 &position, const Ogre::ColourValue &colour, float scale)
{
    Ogre::Matrix4 transform = Ogre::Matrix4::IDENTITY;
    transform.setTrans(position);
    transform.setScale(Ogre::Vector3(scale, scale, scale));
 
    for (int i = 0; i < static_cast<int64_t>(vertices.size()); i++)
        target->push_back(VertexPair(transform * vertices[i], colour));

    return vertices.size();
}
 
// ===============================================================================================

template<> DebugDrawer* Ogre::Singleton<DebugDrawer>::msSingleton = 0;
DebugDrawer* DebugDrawer::getSingletonPtr(void)
{
    return msSingleton;
}
 
DebugDrawer& DebugDrawer::getSingleton(void)
{  
    assert( msSingleton );  return ( *msSingleton );  
}


DebugDrawer::DebugDrawer(Ogre::SceneManager *_sceneManager, float _fillAlpha)
    : sceneManager(_sceneManager), fillAlpha(_fillAlpha), manualObject(0), isEnabled(true),linesIndex(0),  trianglesIndex(0)
{
    initialise();
}
 
DebugDrawer::~DebugDrawer()
{
    shutdown();
}
 
void DebugDrawer::initialise()
{
    manualObject = sceneManager->createManualObject("debug_object");
    sceneManager->getRootSceneNode()->createChildSceneNode("debug_object")->attachObject(manualObject);
    manualObject->setDynamic(true);
 
    icoSphere.create(DEFAULT_ICOSPHERE_RECURSION_LEVEL);
 
    manualObject->begin("debug_draw", Ogre::RenderOperation::OT_LINE_LIST, "Graphics");// Ogre::ResourceGroupManager::getSingletonPtr()->getResourceGroups()[2]);
    manualObject->position(Ogre::Vector3::ZERO);
    manualObject->colour(Ogre::ColourValue::ZERO);
    manualObject->index(0);
    manualObject->end();
    manualObject->begin("debug_draw", Ogre::RenderOperation::OT_TRIANGLE_LIST,  "Graphics");  // Ogre::ResourceGroupManager::getSingletonPtr()->getResourceGroups()[2]);
    manualObject->position(Ogre::Vector3::ZERO);
    manualObject->colour(Ogre::ColourValue::ZERO);
    manualObject->index(0);
    manualObject->end();
 
    linesIndex = trianglesIndex = 0;
}
 
void DebugDrawer::setIcoSphereRecursionLevel(int recursionLevel)
{
    icoSphere.create(recursionLevel);
}
 
void DebugDrawer::shutdown()
{
    sceneManager->destroySceneNode("debug_object");
    sceneManager->destroyManualObject(manualObject);
}
 
void DebugDrawer::buildLine(const Ogre::Vector3& start,
                            const Ogre::Vector3& end,
                            const Ogre::ColourValue& colour,
                            float alpha)
{
    int i = addLineVertex(start, Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));
    addLineVertex(end, Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));
 
    addLineIndices(i, i + 1);
}
 
void DebugDrawer::buildQuad(const Ogre::Vector3 *vertices,
                            const Ogre::ColourValue& colour,
                            float alpha)
{
    int index = addLineVertex(vertices[0], Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));
    addLineVertex(vertices[1], Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));
    addLineVertex(vertices[2], Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));
    addLineVertex(vertices[3], Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));
 
    for (int i = 0; i < 4; ++i) addLineIndices(index + i, index + ((i + 1) % 4));
}
 
void DebugDrawer::buildCircle(const Ogre::Vector3 &centre,
                              float radius,
                              int segmentsCount,
                              const Ogre::ColourValue& colour,
                              float alpha)
{
    int index = linesIndex;
    float increment = 2 * Ogre::Math::PI / segmentsCount;
    float angle = 0.0f;
 
    for (int i = 0; i < segmentsCount; i++)
    {
        addLineVertex(Ogre::Vector3(centre.x + radius * Ogre::Math::Cos(angle), centre.y, centre.z + radius * Ogre::Math::Sin(angle)),
                      Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));
        angle += increment;
    }
 
    for (int i = 0; i < segmentsCount; i++)
        addLineIndices(index + i, i + 1 < segmentsCount ? index + i + 1 : index);
}
 
void DebugDrawer::buildFilledCircle(const Ogre::Vector3 &centre,
                                    float radius,
                                    int segmentsCount,
                                    const Ogre::ColourValue& colour,
                                    float alpha)
{
    int index = trianglesIndex;
    float increment = 2 * Ogre::Math::PI / segmentsCount;
    float angle = 0.0f;
 
    for (int i = 0; i < segmentsCount; i++)
    {
        addTriangleVertex(Ogre::Vector3(centre.x + radius * Ogre::Math::Cos(angle), centre.y, centre.z + radius * Ogre::Math::Sin(angle)),
                          Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));
        angle += increment;
    }
 
    addTriangleVertex(centre, Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));
 
    for (int i = 0; i < segmentsCount; i++)
        addTriangleIndices(i + 1 < segmentsCount ? index + i + 1 : index, index + i, index + segmentsCount);
}

void DebugDrawer::buildCylinder(const Ogre::Vector3 &centre,
                                float radius,
                                int segmentsCount,
                                float height,
                                const Ogre::ColourValue& colour,
                                float alpha)
{
    int index = linesIndex;
    float increment = 2 * Ogre::Math::PI / segmentsCount;
    float angle = 0.0f;
 
    // Top circle
    for (int i = 0; i < segmentsCount; i++)
    {
        addLineVertex(Ogre::Vector3(centre.x + radius * Ogre::Math::Cos(angle), centre.y + height / 2, centre.z + radius * Ogre::Math::Sin(angle)),
                      Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));
        angle += increment;
    }

    angle = 0.0f;

    // Bottom circle
    for (int i = 0; i < segmentsCount; i++)
    {
        addLineVertex(Ogre::Vector3(centre.x + radius * Ogre::Math::Cos(angle), centre.y - height / 2, centre.z + radius * Ogre::Math::Sin(angle)),
                      Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));
        angle += increment;
    }
 
    for (int i = 0; i < segmentsCount; i++)
    {
        addLineIndices(index + i, i + 1 < segmentsCount ? index + i + 1 : index);
        addLineIndices(segmentsCount + index + i, i + 1 < segmentsCount ? segmentsCount + index + i + 1 : segmentsCount + index);
        addLineIndices(index + i, segmentsCount + index + i);
    }
}
 
void DebugDrawer::buildFilledCylinder(const Ogre::Vector3 &centre,
                                      float radius,
                                      int segmentsCount,
                                      float height,
                                      const Ogre::ColourValue& colour,
                                      float alpha)
{
    int index = trianglesIndex;
    float increment = 2 * Ogre::Math::PI / segmentsCount;
    float angle = 0.0f;
 
    // Top circle
    for (int i = 0; i < segmentsCount; i++)
    {
        addTriangleVertex(Ogre::Vector3(centre.x + radius * Ogre::Math::Cos(angle), centre.y + height / 2, centre.z + radius * Ogre::Math::Sin(angle)),
                          Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));
        angle += increment;
    }

    addTriangleVertex(Ogre::Vector3(centre.x, centre.y + height / 2, centre.z), Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));

    angle = 0.0f;

    // Bottom circle
    for (int i = 0; i < segmentsCount; i++)
    {
        addTriangleVertex(Ogre::Vector3(centre.x + radius * Ogre::Math::Cos(angle), centre.y - height / 2, centre.z + radius * Ogre::Math::Sin(angle)),
                          Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));
        angle += increment;
    }
 
    addTriangleVertex(Ogre::Vector3(centre.x, centre.y - height / 2, centre.z), Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));
 
    for (int i = 0; i < segmentsCount; i++)
    {
        addTriangleIndices(i + 1 < segmentsCount ? index + i + 1 : index,
                           index + i,
                           index + segmentsCount);

        addTriangleIndices(i + 1 < segmentsCount ? (segmentsCount + 1) + index + i + 1 : (segmentsCount + 1) + index,
                           (segmentsCount + 1) + index + segmentsCount,
                           (segmentsCount + 1) + index + i);
		
        addQuadIndices(
            index + i,
            i + 1 < segmentsCount ? index + i + 1 : index,
            i + 1 < segmentsCount ? (segmentsCount + 1) + index + i + 1 : (segmentsCount + 1) + index,
            (segmentsCount + 1) + index + i);
    }
}

void DebugDrawer::buildCuboid(const Ogre::Vector3 *vertices,
                              const Ogre::ColourValue& colour,
                              float alpha)
{
    int index = addLineVertex(vertices[0], Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));
    for (int i = 1; i < 8; ++i) addLineVertex(vertices[i], Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));
 
    for (int i = 0; i < 4; ++i) addLineIndices(index + i, index + ((i + 1) % 4));
    for (int i = 4; i < 8; ++i) addLineIndices(index + i, i == 7 ? index + 4 : index + i + 1);
    addLineIndices(index + 1, index + 5);
    addLineIndices(index + 2, index + 4);
    addLineIndices(index,     index + 6);
    addLineIndices(index + 3, index + 7);
}
 
void DebugDrawer::buildFilledCuboid(const Ogre::Vector3 *vertices,
                                    const Ogre::ColourValue& colour,
                                    float alpha)
{
    int index = addTriangleVertex(vertices[0], Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));
    for (int i = 1; i < 8; ++i) addTriangleVertex(vertices[i], Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));
 
    addQuadIndices(index,     index + 1, index + 2, index + 3);
    addQuadIndices(index + 4, index + 5, index + 6, index + 7);
 
    addQuadIndices(index + 1, index + 5, index + 4, index + 2);
    addQuadIndices(index,     index + 3, index + 7, index + 6);
 
    addQuadIndices(index + 1, index    , index + 6, index + 5);
    addQuadIndices(index + 4, index + 7, index + 3, index + 2);
}
 
void DebugDrawer::buildFilledQuad(const Ogre::Vector3 *vertices,
                                  const Ogre::ColourValue& colour,
                                  float alpha)
{
    int index = addTriangleVertex(vertices[0], Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));
    addTriangleVertex(vertices[1], Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));
    addTriangleVertex(vertices[2], Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));
    addTriangleVertex(vertices[3], Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));
 
    addQuadIndices(index, index + 1, index + 2, index + 3);
}
 
void DebugDrawer::buildFilledTriangle(const Ogre::Vector3 *vertices,
                                      const Ogre::ColourValue& colour,
                                      float alpha)
{
    int index = addTriangleVertex(vertices[0], Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));
    addTriangleVertex(vertices[1], Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));
    addTriangleVertex(vertices[2], Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));
 
    addTriangleIndices(index, index + 1, index + 2);
}

void DebugDrawer::buildTetrahedron(const Ogre::Vector3 &centre,
                                   float scale,
                                   const Ogre::ColourValue &colour,
                                   float alpha)
{
    int index = linesIndex;

    // Distance from the centre
    float bottomDistance = scale * 0.2f;
    float topDistance = scale * 0.62f;
    float frontDistance = scale * 0.289f;
    float backDistance = scale * 0.577f;
    float leftRightDistance = scale * 0.5f;

    addLineVertex(Ogre::Vector3(centre.x, centre.y + topDistance, centre.z),
                  Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));
    addLineVertex(Ogre::Vector3(centre.x, centre.y - bottomDistance, centre.z + frontDistance),
                  Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));
    addLineVertex(Ogre::Vector3(centre.x + leftRightDistance, centre.y - bottomDistance, centre.z - backDistance),
                  Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));
    addLineVertex(Ogre::Vector3(centre.x - leftRightDistance, centre.y - bottomDistance, centre.z - backDistance),
                  Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));

    addLineIndices(index, index + 1);
    addLineIndices(index, index + 2);
    addLineIndices(index, index + 3);

    addLineIndices(index + 1, index + 2);
    addLineIndices(index + 2, index + 3);
    addLineIndices(index + 3, index + 1);
}

void DebugDrawer::buildFilledTetrahedron(const Ogre::Vector3 &centre,
                                         float scale,
                                         const Ogre::ColourValue &colour,
                                         float alpha)
{
    int index = trianglesIndex;

    // Distance from the centre
    float bottomDistance = scale * 0.2f;
    float topDistance = scale * 0.62f;
    float frontDistance = scale * 0.289f;
    float backDistance = scale * 0.577f;
    float leftRightDistance = scale * 0.5f;

    addTriangleVertex(Ogre::Vector3(centre.x, centre.y + topDistance, centre.z),
                      Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));
    addTriangleVertex(Ogre::Vector3(centre.x, centre.y - bottomDistance, centre.z + frontDistance),
                      Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));
    addTriangleVertex(Ogre::Vector3(centre.x + leftRightDistance, centre.y - bottomDistance, centre.z - backDistance),
                      Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));
    addTriangleVertex(Ogre::Vector3(centre.x - leftRightDistance, centre.y - bottomDistance, centre.z - backDistance),
                      Ogre::ColourValue(colour.r, colour.g, colour.b, alpha));

    addTriangleIndices(index, index + 1, index + 2);
    addTriangleIndices(index, index + 2, index + 3);
    addTriangleIndices(index, index + 3, index + 1);

    addTriangleIndices(index + 1, index + 3, index + 2);
}
 
void DebugDrawer::drawLine(const Ogre::Vector3& start,
                           const Ogre::Vector3& end,
                           const Ogre::ColourValue& colour)
{
    buildLine(start, end, colour);
}
 
void DebugDrawer::drawCircle(const Ogre::Vector3 &centre,
                             float radius,
                             int segmentsCount,
                             const Ogre::ColourValue& colour,
                             bool isFilled)
{
    buildCircle(centre, radius, segmentsCount, colour);
    if (isFilled) buildFilledCircle(centre, radius, segmentsCount, colour, fillAlpha);
}

void DebugDrawer::drawCylinder(const Ogre::Vector3 &centre,
                               float radius,
                               int segmentsCount,
                               float height,
                               const Ogre::ColourValue& colour,
                               bool isFilled)
{
    buildCylinder(centre, radius, segmentsCount, height, colour);
    if (isFilled) buildFilledCylinder(centre, radius, segmentsCount, height, colour, fillAlpha);
}
 
void DebugDrawer::drawQuad(const Ogre::Vector3 *vertices,
                           const Ogre::ColourValue& colour,
                           bool isFilled)
{
    buildQuad(vertices, colour);
    if (isFilled) buildFilledQuad(vertices, colour, fillAlpha);
}
 
void DebugDrawer::drawCuboid(const Ogre::Vector3 *vertices,
                             const Ogre::ColourValue& colour,
                             bool isFilled)
{
    buildCuboid(vertices, colour);
    if (isFilled) buildFilledCuboid(vertices, colour, fillAlpha);
}
 
void DebugDrawer::drawSphere(const Ogre::Vector3 &centre,
                             float radius,
                             const Ogre::ColourValue& colour,
                             bool isFilled)
{
    int baseIndex = linesIndex;
    linesIndex += icoSphere.addToVertices(&lineVertices, centre, colour, radius);
    icoSphere.addToLineIndices(baseIndex, &lineIndices);
 
    if (isFilled)
    {
        baseIndex = trianglesIndex;
        trianglesIndex += icoSphere.addToVertices(&triangleVertices, centre, Ogre::ColourValue(colour.r, colour.g, colour.b, fillAlpha), radius);
        icoSphere.addToTriangleIndices(baseIndex, &triangleIndices);
    }
}

void DebugDrawer::drawTetrahedron(const Ogre::Vector3 &centre,
                                  float scale,
                                  const Ogre::ColourValue& colour,
                                  bool isFilled)
{
    buildTetrahedron(centre, scale, colour);
    if (isFilled) buildFilledTetrahedron(centre, scale, colour, fillAlpha);
}
 
void DebugDrawer::build()
{
    manualObject->beginUpdate(0);
    if (lineVertices.size() > 0 && isEnabled)
    {
        manualObject->estimateVertexCount(lineVertices.size());
        manualObject->estimateIndexCount(lineIndices.size());
        for (std::list<VertexPair>::iterator i = lineVertices.begin(); i != lineVertices.end(); i++)
        {
            manualObject->position(i->first);
            manualObject->colour(i->second);
        }
        for (std::list<int>::iterator i = lineIndices.begin(); i != lineIndices.end(); i++)
            manualObject->index(*i);
    }
    manualObject->end();
 
    manualObject->beginUpdate(1);
    if (triangleVertices.size() > 0 && isEnabled)
    {
        manualObject->estimateVertexCount(triangleVertices.size());
        manualObject->estimateIndexCount(triangleIndices.size());
        for (std::list<VertexPair>::iterator i = triangleVertices.begin(); i != triangleVertices.end(); i++)
        {
            manualObject->position(i->first);
            manualObject->colour(i->second.r, i->second.g, i->second.b, fillAlpha);
        }
        for (std::list<int>::iterator i = triangleIndices.begin(); i != triangleIndices.end(); i++)
            manualObject->index(*i);
    }
    manualObject->end();
}
 
void DebugDrawer::clear()
{
    lineVertices.clear();
    triangleVertices.clear();
    lineIndices.clear();
    triangleIndices.clear();
    linesIndex = trianglesIndex = 0;
}
 
int DebugDrawer::addLineVertex(const Ogre::Vector3 &vertex, const Ogre::ColourValue &colour)
{
    lineVertices.push_back(VertexPair(vertex, colour));
    return linesIndex++;
}
 
void DebugDrawer::addLineIndices(int index1, int index2)
{
    lineIndices.push_back(index1);
    lineIndices.push_back(index2);
}
 
int DebugDrawer::addTriangleVertex(const Ogre::Vector3 &vertex, const Ogre::ColourValue &colour)
{
    triangleVertices.push_back(VertexPair(vertex, colour));
    return trianglesIndex++;
}
 
void DebugDrawer::addTriangleIndices(int index1, int index2, int index3)
{
    triangleIndices.push_back(index1);
    triangleIndices.push_back(index2);
    triangleIndices.push_back(index3);
}
 
void DebugDrawer::addQuadIndices(int index1, int index2, int index3, int index4)
{
    triangleIndices.push_back(index1);
    triangleIndices.push_back(index2);
    triangleIndices.push_back(index3);
 
    triangleIndices.push_back(index1);
    triangleIndices.push_back(index3);
    triangleIndices.push_back(index4);
}
