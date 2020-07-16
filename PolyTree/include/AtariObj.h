#pragma once

#include <vector>

extern "C"
{
	#include "Obj.h"
}

class AtariObj
{
public:
	Obj o;
	float* fpVerts;
	
	AtariObj(char* filename);
	void Render();
	void GenerateTree();

private:
	unsigned int VAO, VBO, EBO;
	void GenerateNode(ObjNode* node, std::vector<ObjFace> faces);
	void SetupBuffers();
	bool FacesIntersect(ObjFace* face1, ObjFace* face2);
};
