/*
### Mesh data format ###
vertices[] = {Vector3, ...}
normals[] = {Vector3, ...}
attributes[] = {{Vector3, ...}, ...}
faces[] = {Face, ...}

MapCoord { id mapid, co1, co2, co3; }

Face {
    id v1, v2, v3;
    id n1, n2, n3;
    attributes[] = {MapCoord, ...}
    Material *m
}
*/

#ifndef R1H_MESH_H
#define R1H_MESH_H

#include <vector>

#include "geometry.h"
#include <materials/material.h>
#include <bvhnode.h>

namespace r1h {

class Mesh : public Geometry, public BVHLeaf {
public:
	
    static const int kTypeID;
    
    struct AttrCoord {
        int attrid;         // index of Geometry's attributes
        int co0, co1, co2;
    };
    
    struct Face {
        int v0, v1, v2;
        int n0, n1, n2;
        int t0, t1, t2;
        std::vector<AttrCoord> attrs;
        int matid;
		
        R1hFPType area;
        R1hFPType areaBorder;
		Vector3 normal;
		Vector4 tangent;
		
        Face();
        Face(const int a, const int b, const int c, const int m);
        
        void setV(const int a, const int b, const int c);
        void setN(const int a, const int b, const int c);
        void setT(const int a, const int b, const int c);
        void addAttr(const int attrid, const int a, const int b, const int c);
    };
    
public:
    // empty mesh
    Mesh(const int vreserve=0, const int freserv=0);
    ~Mesh();
    
    size_t addVertexWithAttrs(const Vector3 &p, const Vector3 &n, const Vector4 &t, const Vector3 &uv=0, const int uvid=-1);
    size_t addVertex(const Vector3 &v);
    size_t getVertexCount() const;
	
	size_t addNormal(const Vector3 &v);
    size_t getNormalCount() const;
    
    size_t addTangent(const Vector4 &v);
    size_t getTangentCount() const;
    
	size_t newAttributeContainer();
    size_t addAttribute(const int attrid, const Vector3 &v);
	size_t getAttributeCount(const int attrid) const;
    
    size_t addFace(const Mesh::Face &f);
    size_t addFace(const int a, const int b, const int c, const int matid=0);
    size_t getFaceCount() const;
	
    Vector3 getVaryingAttr(const int faceid, const int attrid, const Vector3 weights) const;
	
	void postProcess();
    void calcSmoothNormals();
    void buildBVH();
	
	//--- override ---
	// Geometry
	virtual AABB getAABB(const Matrix4& tm) const;
    virtual bool isIntersect(const Ray &ray, Intersection *intersect) const;
    virtual SamplePoint getSamplePoint(Random *rng) const;
    virtual void prepareRendering();
	
    virtual Vector4 computeTangent(Intersection *intersect) const;    
    
    // BVHLeaf
	virtual bool isIntersectLeaf(int dataid, const Ray &ray, Intersection *intersect) const;
	//bool triangleIntersect(const int faceid, const Ray &ray, Intersection *intersect) const;
	
	/// for debug
	void dumpFaces() const;
	
private:
	std::vector<Vector3> vertices;
	std::vector<Vector3> normals;
	std::vector<Vector4> tangents;
    std::vector<std::vector<Vector3> > attributes;
    std::vector<Face> faces;
	R1hFPType surfArea;
	
    BVHNode *bvhRoot;
    
    int vertex_reserved;
    int face_reserved;
};

}
#endif
