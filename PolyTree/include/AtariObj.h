#pragma once

extern "C"
{
	#include "Obj.h"
}

class AtariObj
{
public:
	Obj o;
	
	AtariObj(char* filename);
	void Render();

private:
	unsigned int VAO, VBO, EBO;
	void SetupBuffers();
};
