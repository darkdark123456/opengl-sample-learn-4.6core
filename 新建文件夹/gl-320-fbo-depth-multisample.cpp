#include "test.hpp"

namespace
{
	char const* VERT_SHADER_SOURCE_TEXTURE("gl-320/texture-2d.vert");
	char const* FRAG_SHADER_SOURCE_TEXTURE("gl-320/texture-2d.frag");
	char const* VERT_SHADER_SOURCE_SPLASH("gl-320/fbo-depth-multisample.vert");
	char const* FRAG_SHADER_SOURCE_SPLASH("gl-320/fbo-depth-multisample.frag");
	char const* TEXTURE_DIFFUSE("kueken7_rgb_dxt1_unorm.dds");

	GLsizei const VertexCount(4);
	GLsizeiptr const VertexSize = VertexCount * sizeof(glf::vertex_v2fv2f);
	glf::vertex_v2fv2f const VertexData[VertexCount] =
	{
		glf::vertex_v2fv2f(glm::vec2(-1.0f,-1.0f), glm::vec2(0.0f, 1.0f)),
		glf::vertex_v2fv2f(glm::vec2( 1.0f,-1.0f), glm::vec2(1.0f, 1.0f)),
		glf::vertex_v2fv2f(glm::vec2( 1.0f, 1.0f), glm::vec2(1.0f, 0.0f)),
		glf::vertex_v2fv2f(glm::vec2(-1.0f, 1.0f), glm::vec2(0.0f, 0.0f))
	};

	GLsizei const ElementCount(6);
	GLsizeiptr const ElementSize = ElementCount * sizeof(GLushort);
	GLushort const ElementData[ElementCount] =
	{
		0, 1, 2, 
		2, 3, 0
	};

	namespace buffer
	{
		enum type
		{
			VERTEX,
			ELEMENT,
			TRANSFORM,
			MAX
		};
	}//namespace buffer

	namespace texture
	{
		enum type
		{
			DIFFUSE,
			MULTISAMPLE,
			MAX
		};
	}//namespace texture
	
	namespace program
	{
		enum type
		{
			TEXTURE, // 用来算真实世界
			SPLASH,  // 用于屏幕显示和后处理
			MAX
		};
	}//namespace program

	namespace framebuffer
	{
		enum type
		{
			DEPTH_MULTISAMPLE,
			MAX
		};
	}//namespace framebuffer

	namespace shader
	{
		enum type
		{
			VERT_TEXTURE,
			FRAG_TEXTURE,
			VERT_SPLASH,
			FRAG_SPLASH,
			MAX
		};
	}//namespace shader

	std::vector<GLuint> FramebufferName(framebuffer::MAX);
	std::vector<GLuint> ProgramName(program::MAX);
	std::vector<GLuint> VertexArrayName(program::MAX);
	std::vector<GLuint> BufferName(buffer::MAX);
	std::vector<GLuint> TextureName(texture::MAX);
	GLint UniformTransform(0);
}//namespace

class sample : public framework
{
public:
	sample(int argc, char* argv[]) :
		framework(argc, argv, "gl-320-fbo-depth-multisample", framework::CORE, 3, 2, glm::vec2(0.0f, -glm::pi<float>() * 0.48f))
	{}

private:
	bool initProgram()
	{
		bool Validated(true);
	
		std::vector<GLuint> ShaderName(shader::MAX);
		compiler Compiler;

		// 第一套ProgrameName工艺单用来画真实几何
		if(Validated)
		{
			// 获取顶点着色器
			ShaderName[shader::VERT_TEXTURE] = Compiler.create(GL_VERTEX_SHADER, getDataDirectory() + VERT_SHADER_SOURCE_TEXTURE, "--version 150 --profile core");
			// 获取片段着色器
			ShaderName[shader::FRAG_TEXTURE] = Compiler.create(GL_FRAGMENT_SHADER, getDataDirectory() + FRAG_SHADER_SOURCE_TEXTURE, "--version 150 --profile core");

			// 初始化纹理工艺流程单
			ProgramName[program::TEXTURE] = glCreateProgram();
			// 装配着色器
			glAttachShader(ProgramName[program::TEXTURE], ShaderName[shader::VERT_TEXTURE]);
			glAttachShader(ProgramName[program::TEXTURE], ShaderName[shader::FRAG_TEXTURE]);
			
			// 绑定shader中的顶点属性
			glBindAttribLocation(ProgramName[program::TEXTURE], semantic::attr::POSITION, "Position");
			// 绑定shader中的纹理属性
			glBindAttribLocation(ProgramName[program::TEXTURE], semantic::attr::TEXCOORD, "Texcoord");
			// 绑定片段着色器中的颜色输出
			glBindFragDataLocation(ProgramName[program::TEXTURE], semantic::frag::COLOR, "Color");
			
			//对第一个工艺流程单进行连接
			glLinkProgram(ProgramName[program::TEXTURE]);
		}


		// 第二套工艺单用来将第一套的结果显示在屏幕上 和 后处理,与第一套的区别是不需要Poition 和 MVP
		if(Validated)
		{

			// 绑定顶点着色器
			ShaderName[shader::VERT_SPLASH] = Compiler.create(GL_VERTEX_SHADER, getDataDirectory() + VERT_SHADER_SOURCE_SPLASH, "--version 150 --profile core");
			// 绑定片段着色器
			ShaderName[shader::FRAG_SPLASH] = Compiler.create(GL_FRAGMENT_SHADER, getDataDirectory() + FRAG_SHADER_SOURCE_SPLASH, "--version 150 --profile core");
			// 初始化工艺流程单
			ProgramName[program::SPLASH] = glCreateProgram();
			
			// 装配着色器
			glAttachShader(ProgramName[program::SPLASH], ShaderName[shader::VERT_SPLASH]);
			glAttachShader(ProgramName[program::SPLASH], ShaderName[shader::FRAG_SPLASH]);
			
			// 绑定片段着色器中的输出
			glBindFragDataLocation(ProgramName[program::SPLASH], semantic::frag::COLOR, "Color");
			
			// 对第二个工艺流程单进行连接
			glLinkProgram(ProgramName[program::SPLASH]);
		}

		if(Validated)
		{
			Validated = Validated && Compiler.check();
			Validated = Validated && Compiler.check_program(ProgramName[program::TEXTURE]);
			Validated = Validated && Compiler.check_program(ProgramName[program::SPLASH]);
		}

		if(Validated)
			UniformTransform = glGetUniformBlockIndex(ProgramName[program::TEXTURE], "transform");

		return Validated && this->checkError("initProgram");
	}

	bool initBuffer()
	{
		// 生成三个缓冲区 VAO VBO EBO
		glGenBuffers(buffer::MAX, &BufferName[0]);
		// 接下来指示的操作是说给EBO听的
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, BufferName[buffer::ELEMENT]);
		
		// 将EBO的数据放到显存的合适位置 这块数据经常被cpu更改
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, ElementSize, ElementData, GL_STATIC_DRAW);

		// 解绑
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);


		// 接下来的操作是说给顶点缓冲区听的 VAO
		glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::VERTEX]);
		// 将VAO的数据放在显存的合适位置 这块内存经常被cpu进行访问和修改
		glBufferData(GL_ARRAY_BUFFER, VertexSize, VertexData, GL_STATIC_DRAW);
		// 解绑
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		// UBO的对齐字节在GPU中至少需要多少
		GLint UniformBufferOffset(0);
		// 系统目前提供的GPU最低的对齐字节
		glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &UniformBufferOffset);
		
		// UBO在GPU中的最低对齐字节至少为GPU要求的最低大小 但是如果你的mat4很大 我就以你为单位对齐
		GLint UniformBlockSize = glm::max(GLint(sizeof(glm::mat4)), UniformBufferOffset);

		// 接下来的操作是说给UBO听的
		glBindBuffer(GL_UNIFORM_BUFFER, BufferName[buffer::TRANSFORM]);
		
		// 给我至少一块能够存放MVP矩阵大小的GPU内存
		glBufferData(GL_UNIFORM_BUFFER, UniformBlockSize, NULL, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		return this->checkError("initBuffer");
	}

	bool initTexture()
	{
		bool Validated(true);

		gli::texture2d Texture(gli::load_dds((getDataDirectory() + TEXTURE_DIFFUSE).c_str()));
		assert(!Texture.empty());

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		glGenTextures(texture::MAX, &TextureName[0]);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, TextureName[texture::DIFFUSE]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, GLint(Texture.levels() - 1));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		
		for(std::size_t Level = 0; Level < Texture.levels(); ++Level)
		{
			glCompressedTexImage2D(GL_TEXTURE_2D,
				GLint(Level),
				GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
				GLsizei(Texture[Level].extent().x),
				GLsizei(Texture[Level].extent().y),
				0, 
				GLsizei(Texture[Level].size()),
				Texture[Level].data());
		}
		
		glm::ivec2 WindowSize(this->getWindowSize());

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, TextureName[texture::MULTISAMPLE]);
		
		this->checkError("initTexture 1");
		
		glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAX_LEVEL, 0);
		
		this->checkError("initTexture 2");
		
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_DEPTH_COMPONENT24, GLsizei(WindowSize.x), GLsizei(WindowSize.y), GL_TRUE);
		
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

		return Validated && this->checkError("initTexture");
	}

	bool initVertexArray()
	{
		// 在GPU生成VAO标志
		glGenVertexArrays(program::MAX, &VertexArrayName[0]);
		// 接下在的操作是说给TEXTTURE这个VAO说的 接下来的顶点读取规则都会被记录进当前这个VAO
		glBindVertexArray(VertexArrayName[program::TEXTURE]);
		
		//接下来的顶点描述来自这个VBO，VAO你要记住我的规则
		glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::VERTEX]);
		
		// 对于Poition顶点 它在vertex_v2fv2f 这个 struct中 从偏移量0开始读2个
		glVertexAttribPointer(semantic::attr::POSITION, 2, GL_FLOAT, GL_FALSE, sizeof(glf::vertex_v2fv2f), BUFFER_OFFSET(0));
		// 对于Textcoord顶点 它在vertex_v2fv2f 这个 struct中 从偏移量vec2的大小初开始读 都两个
		glVertexAttribPointer(semantic::attr::TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(glf::vertex_v2fv2f), BUFFER_OFFSET(sizeof(glm::vec2)));		
		//  VAO已经记住读取的规则了 VBO你可以空闲了 我对你解绑了
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		
		
		// 打开顶点着色器中的顶点position属性 让VAO可以进行从当前GL_ARRAY_BUFFER按照规则往着色器中写入
		glEnableVertexAttribArray(semantic::attr::POSITION);
		
		// 打开顶点着色器中的顶点textcoord属性 让VAO可以进行从当前GL_ARRAY_BUFFER按照规则往着色器中写入
		glEnableVertexAttribArray(semantic::attr::TEXCOORD);


		// 让VAO记住使用哪个索引缓冲区
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, BufferName[buffer::ELEMENT]);
		
		// 至此 VAO所有的规则已经全部学会 处于空闲状态
		glBindVertexArray(0);



		glBindVertexArray(VertexArrayName[program::SPLASH]);
		glBindVertexArray(0);

		return this->checkError("initVertexArray");
	}

	bool initFramebuffer()
	{
		bool Validated(true);

		glGenFramebuffers(GLsizei(framebuffer::MAX), &FramebufferName[0]);
		glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName[framebuffer::DEPTH_MULTISAMPLE]);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, TextureName[texture::MULTISAMPLE], 0);
		glDrawBuffer(GL_NONE);

		if(!this->checkFramebuffer(FramebufferName[framebuffer::DEPTH_MULTISAMPLE]))
			return false;

		glBindFramebuffer(GL_FRAMEBUFFER, 0);	
		return this->checkError("initFramebuffer");
	}

	bool begin()
	{
		bool Validated(true);

		

		if(Validated)
			Validated = initProgram();
		if(Validated)
			Validated = initBuffer();
		if(Validated)
			Validated = initVertexArray();
		if(Validated)
			Validated = initTexture();
		if(Validated)
			Validated = initFramebuffer();

		return Validated && this->checkError("begin");
	}

	bool end()
	{
		glDeleteFramebuffers(GLsizei(FramebufferName.size()), &FramebufferName[0]);
		glDeleteProgram(ProgramName[program::SPLASH]);
		glDeleteProgram(ProgramName[program::TEXTURE]);
		glDeleteBuffers(buffer::MAX, &BufferName[0]);
		glDeleteTextures(texture::MAX, &TextureName[0]);
		glDeleteVertexArrays(program::MAX, &VertexArrayName[0]);

		return this->checkError("end");
	}

	bool render()
	{
		glm::ivec2 WindowSize(this->getWindowSize());

		{
			glBindBuffer(GL_UNIFORM_BUFFER, BufferName[buffer::TRANSFORM]);
			glm::mat4* Pointer = (glm::mat4*)glMapBufferRange(
				GL_UNIFORM_BUFFER, 0,	sizeof(glm::mat4),
				GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

			//glm::mat4 Projection = glm::perspectiveFov(glm::pi<float>() * 0.25f, 640.f, 480.f, 0.1f, 100.0f);
			glm::mat4 Projection = glm::perspective(glm::pi<float>() * 0.25f, 4.0f / 3.0f, 0.1f, 8.0f);
			glm::mat4 Model = glm::scale(glm::mat4(1.0f), glm::vec3(5.0f));
		
			*Pointer = Projection * this->view() * Model;

			// Make sure the uniform buffer is uploaded
			glUnmapBuffer(GL_UNIFORM_BUFFER);
		}

		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);

		glViewport(0, 0, WindowSize.x, WindowSize.y);

		glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName[framebuffer::DEPTH_MULTISAMPLE]);
		float Depth(1.0f);
		glClearBufferfv(GL_DEPTH , 0, &Depth);

		// Bind rendering objects
		glUseProgram(ProgramName[program::TEXTURE]);
		glUniformBlockBinding(ProgramName[program::TEXTURE], UniformTransform, semantic::uniform::TRANSFORM0);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, TextureName[texture::DIFFUSE]);
		glBindVertexArray(VertexArrayName[program::TEXTURE]);
		glBindBufferBase(GL_UNIFORM_BUFFER, semantic::uniform::TRANSFORM0, BufferName[buffer::TRANSFORM]);

		glDrawElementsInstancedBaseVertex(GL_TRIANGLES, ElementCount, GL_UNSIGNED_SHORT, 0, 2, 0);

		// Pass 2
		glDisable(GL_DEPTH_TEST);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glUseProgram(ProgramName[program::SPLASH]);

		glActiveTexture(GL_TEXTURE0);
		glBindVertexArray(VertexArrayName[program::SPLASH]);
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, TextureName[texture::MULTISAMPLE]);

		glDrawArraysInstanced(GL_TRIANGLES, 0, 3, 1);

		return true;
	}
};

int main(int argc, char* argv[])
{
	int Error = 0;

	sample Sample(argc, argv);
	Error += Sample();

	return Error;
}

