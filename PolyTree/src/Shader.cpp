#include "Shader.h"

#include <glad/glad.h>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>


Shader::Shader(const char* vPath, const char* fPath)
{
	std::ifstream file;
	std::stringstream stream;
	std::string codeString;

	unsigned int vertexId, fragmentId;
	int success;

	programId = -1;

	{
		file.open(vPath);
		stream << file.rdbuf();
		file.close();
		codeString = stream.str();
		const char * vCode = (const char*)codeString.c_str();

		file.open(fPath);;
		stream << file.rdbuf();
		file.close();
		codeString = stream.str();
		const char* fCode = (const char*)codeString.c_str();

		vertexId = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertexId, 1, &vCode, NULL);
		glCompileShader(vertexId);
		
		fragmentId = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragmentId, 1, &fCode, NULL);
		glCompileShader(fragmentId);

		programId = glCreateProgram();
		glAttachShader(programId, vertexId);
		glAttachShader(programId, fragmentId);
		glLinkProgram(programId);

		glDeleteShader(vertexId);
		glDeleteShader(fragmentId);
	}
}

void Shader::Use()
{
	glUseProgram(programId);
}

