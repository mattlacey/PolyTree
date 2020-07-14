#pragma once
class Shader
{
public:
	unsigned int programId;
	char errorLog[1024];

	Shader(const char* vPath, const char* fPath);

	void Use();
};

