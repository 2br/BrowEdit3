#pragma once

#include <glad/glad.h>
#include <string>
#include <glm/glm.hpp>

namespace gl
{
	class Shader
	{
		std::string file;
		GLuint programId;
		int* uniforms;
	public:
		Shader(const std::string& file, int end);
		~Shader();
		void setUniform(int name, bool value);
		void setUniform(int name, int value);
		void setUniform(int name, float value);
		void setUniform(int name, const glm::mat3& value);
		void setUniform(int name, const glm::mat4& value);
		void setUniform(int name, const glm::vec2& value);
		void setUniform(int name, const glm::vec3& value);
		void setUniform(int name, const glm::vec4& value);

		void use();

	private:
		void addShader(GLuint type, const std::string& fileName);
	protected:
		virtual void bindUniforms() = 0;
		void bindUniform(int enumValue, const std::string& name);
	};

}