#include "test.hpp"

namespace
{
	char const* VERT_SHADER_SOURCE("gl-320/draw-range-elements.vert");
	char const* FRAG_SHADER_SOURCE("gl-320/draw-range-elements.frag");

	GLsizei const VertexCount(8);
	GLsizeiptr const VertexSize = VertexCount * sizeof(glm::vec2);
	glm::vec2 const VertexData[VertexCount] =
	{
		glm::vec2(-0.4f,-0.6f),
		glm::vec2( 0.5f,-0.4f),
		glm::vec2( 0.6f, 0.4f),
		glm::vec2(-0.5f, 0.5f),
		glm::vec2(-0.5f,-0.5f),
		glm::vec2( 0.5f,-0.5f),
		glm::vec2( 0.5f, 0.5f),
		glm::vec2(-0.5f, 0.5f)
	};

	GLsizei const ElementCount(12);
	GLsizeiptr const ElementSize = ElementCount * sizeof(GLushort);
	GLushort const ElementData[ElementCount] =
	{
		0, 1, 2,
		2, 3, 0,
		4, 5, 6,
		6, 7, 4
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

	GLuint ProgramName(0);
	GLuint VertexArrayName(0);
	std::vector<GLuint> BufferName(buffer::MAX);
}//namespace

class sample : public framework
{
public:
	sample(int argc, char* argv[]) :
		framework(argc, argv, "gl-320-draw-range-elements", framework::CORE, 3, 2),
		VertexArrayName(0),
		ProgramName(0),
		UniformTransform(-1)
	{}

private:
	std::array<GLuint, buffer::MAX> BufferName;
	GLuint ProgramName;
	GLuint VertexArrayName;
	GLint UniformTransform;

	bool initTest()
	{
		bool Validated = true;
		glEnable(GL_DEPTH_TEST);

		return Validated && this->checkError("initTest");
	}

	bool initProgram()
	{
		bool Validated = true;
	
		// Create program
		if(Validated)
		{	
			//创建一个编译器
			compiler Compiler;
			//获取顶点着色器
			GLuint VertShaderName = Compiler.create(GL_VERTEX_SHADER, getDataDirectory() + VERT_SHADER_SOURCE, "--version 150 --profile core");
			//获取片段着色器
			GLuint FragShaderName = Compiler.create(GL_FRAGMENT_SHADER, getDataDirectory() + FRAG_SHADER_SOURCE, "--version 150 --profile core");
			//创建一个GPU工艺流程单
			ProgramName = glCreateProgram();
			// 把shader装进GPU工艺流程单
			glAttachShader(ProgramName, VertShaderName);
			glAttachShader(ProgramName, FragShaderName);
			
			// 绑定顶点输入变量
			// 把vertex shader里的Postion变量绑定到semantic::attr::POSITION编号的GPU内存里
			glBindAttribLocation(ProgramName, semantic::attr::POSITION, "Position");

			// 绑定片段着色器的输出变量
			// 把fragment shader里Color写到GPU的第COLOR个GPU颜色缓冲区内
			glBindFragDataLocation(ProgramName, semantic::frag::COLOR, "Color");
			glLinkProgram(ProgramName);

			//检查编译错误与连接错误
			Validated = Validated && Compiler.check();
			Validated = Validated && Compiler.check_program(ProgramName);
		}

		// Get variables locations
		if(Validated)
		{
			// 从.vert中读取transform的索引，并绑定到 TRANSFORM0 上，告诉GPU 以后读取transform块数据
			// 只需用插槽TRANSFORM0来获取
			// 用哪一个UBO提供数据
			glUniformBlockBinding(ProgramName, glGetUniformBlockIndex(ProgramName, "transform"), semantic::uniform::TRANSFORM0);
		}

		// 返回OpenGL初始化是否正确
		return Validated && this->checkError("initProgram");
	}

	bool initBuffer()
	{
		// 生成缓冲区对象名
		// BUfferName中的每个Buffer从BufferName[0]中
		// 执行完后会得到 BufferName[VERTEX] BufferName[ELEMENT] BufferName[TRANSPTION] 这三个数组
		glGenBuffers(buffer::MAX, &BufferName[0]);

		// 将对应的坐标存入BufferName[ELEMENT]中的GPU缓存中
		// 初始化GPU索引缓冲区
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, BufferName[buffer::ELEMENT]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, ElementSize, ElementData, GL_STATIC_DRAW);
		// 释放绑定
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		// 初始化GPU顶点缓冲区VBO
		glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::VERTEX]);
		glBufferData(GL_ARRAY_BUFFER, VertexSize, VertexData, GL_STATIC_DRAW);
		// 释放绑定
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		
		// uniform buffer中的GPU数据最少要按多少字节对齐，这是由GPU厂商硬件固定
		GLint UniformBufferOffset(0);
		glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &UniformBufferOffset);

		  // 满足分配的内存的最低要求
	     //  为什么不能直接用sizeof(mat4) 呢？？所有在GPU中的数据都要以GPU对齐为准
		//   如果直接使用sizeof(mat4) 那么获取到的block size为64 ，GPU对齐要求为256字节，那么在工艺    单执行的时候就会出错。
		GLint UniformBlockSize = glm::max(GLint(sizeof(glm::mat4)), UniformBufferOffset);

		// 绑定UBO缓冲区
		glBindBuffer(GL_UNIFORM_BUFFER, BufferName[buffer::TRANSFORM]);
		
		// 为该UBO在GPU上占用一块内存 不提供初始数据 之后CPU通过BufferName[buffer::TRANSFORM]
		// 寻找这块内存 GPU通过semantic::uniform::TRANSFORM
		glBufferData(GL_UNIFORM_BUFFER, UniformBlockSize, NULL, GL_DYNAMIC_DRAW);
		// 释放绑定
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
		return this->checkError("initBuffer");
	}

	bool initVertexArray()
	{
		// 生成一个VAO
		glGenVertexArrays(1, &VertexArrayName);
		//opengl 绑定这个VAO 
		glBindVertexArray(VertexArrayName);
		// 绑定VBO VB0中的数据存取到对应的buffer里
		// 这段代码并不会被VAO记住 只是提供上下文
		glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::VERTEX]);
		
		//将GPU如何读取数据的规则写进VAO
		//1对于顶点属性 2每个顶点取两个分量 3每个分量是FLOAT 数据来源是刚刚的绑定的GL_ARRAY_BUFFER 4不进行归一化 5从buffer的第0个字节开始 6数据是连续排列的
		glVertexAttribPointer(semantic::attr::POSITION, 2, GL_FLOAT, GL_FALSE, 0, 0);
		//允许顶点着色器使用POITION属性 这个启用状态也会被记录到VAO
		glEnableVertexAttribArray(semantic::attr::POSITION);

		//告诉VAO EBO的绑定
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, BufferName[buffer::ELEMENT]);
		glBindVertexArray(0);

		return this->checkError("initVertexArray");
	}

	bool begin()
	{
		bool Validated = true;

		if(Validated)
			Validated = initTest();
		if(Validated)
			Validated = initProgram();
		if(Validated)
			Validated = initBuffer();                                              
		if(Validated)
			Validated = initVertexArray();

		return Validated && this->checkError("begin");
	}

	bool end()
	{
		glDeleteBuffers(buffer::MAX, &BufferName[0]);
		glDeleteProgram(ProgramName);
		glDeleteVertexArrays(1, &VertexArrayName);

		return true;
	}

	bool render()
	{
 		glm::vec2 WindowSize(this->getWindowSize());

		{
			glBindBuffer(GL_UNIFORM_BUFFER, BufferName[buffer::TRANSFORM]);
			glm::mat4* Pointer = (glm::mat4*)glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

			glm::mat4 Projection = glm::perspective(glm::pi<float>() * 0.25f, WindowSize.x / 3.0f / WindowSize.y, 0.1f, 100.0f);
			glm::mat4 Model = glm::mat4(1.0f);

			*Pointer = Projection * this->view() * Model;
			glUnmapBuffer(GL_UNIFORM_BUFFER);
		}

		glViewport(0, 0, static_cast<GLsizei>(WindowSize.x), static_cast<GLsizei>(WindowSize.y));

		float Depth(1.0f);
		glClearBufferfv(GL_DEPTH, 0, &Depth);
		glClearBufferfv(GL_COLOR, 0, &glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)[0]);

		glUseProgram(ProgramName);

		glBindBufferBase(GL_UNIFORM_BUFFER, semantic::uniform::TRANSFORM0, BufferName[buffer::TRANSFORM]);
		glBindVertexArray(VertexArrayName);

		glViewport(static_cast<GLint>(WindowSize.x * 0 / 3), 0, static_cast<GLsizei>(WindowSize.x / 3), static_cast<GLsizei>(WindowSize.y));
		glDrawElements(GL_TRIANGLES, ElementCount / 2, GL_UNSIGNED_SHORT, BUFFER_OFFSET(0));

		glViewport(static_cast<GLint>(WindowSize.x * 1 / 3), 0, static_cast<GLsizei>(WindowSize.x / 3), static_cast<GLsizei>(WindowSize.y));
		glDrawElementsInstancedBaseVertex(GL_TRIANGLES, ElementCount / 2, GL_UNSIGNED_SHORT, BUFFER_OFFSET(sizeof(GLushort) * ElementCount / 2), 1, 0);

		glViewport(static_cast<GLint>(WindowSize.x * 2 / 3), 0, static_cast<GLsizei>(WindowSize.x / 3), static_cast<GLsizei>(WindowSize.y));
		glDrawElementsInstancedBaseVertex(GL_TRIANGLES, ElementCount / 2, GL_UNSIGNED_SHORT, BUFFER_OFFSET(0), 1, VertexCount / 2);

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


