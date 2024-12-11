
#pragma once

#include <functional>
#include <vector>
#include <string>

#include "GL/glew.h"
#include "GLFW/glfw3.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_glfw.h"
#include "implot.h"
#include "implot_internal.h"

#include "glm/common.hpp"
#include "glm/matrix.hpp"
#include <glm/gtx/transform.hpp>

#include "unsuck.hpp"
#include "OrbitControls.h"

using namespace std;
using glm::dvec3;
using glm::dvec4;
using glm::dmat4;

struct GLRenderer;

// ScrollingBuffer from ImPlot implot_demo.cpp.
// MIT License
// url: https://github.com/epezent/implot
struct ScrollingBuffer {
	int MaxSize;
	int Offset;
	ImVector<ImVec2> Data;
	ScrollingBuffer() {
		MaxSize = 2000;
		Offset = 0;
		Data.reserve(MaxSize);
	}
	void AddPoint(float x, float y) {
		if (Data.size() < MaxSize)
			Data.push_back(ImVec2(x, y));
		else {
			Data[Offset] = ImVec2(x, y);
			Offset = (Offset + 1) % MaxSize;
		}
	}
	void Erase() {
		if (Data.size() > 0) {
			Data.shrink(0);
			Offset = 0;
		}
	}
};

struct GLBuffer{

	GLuint handle = -1;
	int64_t size = 0;

};

struct Texture {

	GLRenderer* renderer = nullptr;
	GLuint handle = -1;
	GLuint colorType = -1;
	int width = 0;
	int height = 0;

	static shared_ptr<Texture> create(int width, int height, GLuint colorType, GLRenderer* renderer);

	void setSize(int width, int height);

};

struct Framebuffer {

	vector<shared_ptr<Texture>> colorAttachments;  // 创建一个存储 指向 Texture 类型的 shared_ptr 智能指针 容器
	shared_ptr<Texture> depth;
	GLuint handle = -1;
	GLRenderer* renderer = nullptr;

	int width = 0;
	int height = 0;

	Framebuffer() {
		
	}

	static shared_ptr<Framebuffer> create(GLRenderer* renderer);

	void setSize(int width, int height) {  // 调整帧缓冲对象的尺寸


		bool needsResize = this->width != width || this->height != height;  // 判断尺寸是否不同

		if (needsResize) {

			// COLOR
			for (int i = 0; i < this->colorAttachments.size(); i++) {
				auto& attachment = this->colorAttachments[i];
				attachment->setSize(width, height);
				glNamedFramebufferTexture(this->handle, GL_COLOR_ATTACHMENT0 + i, attachment->handle, 0);
			}

			{ // DEPTH
				this->depth->setSize(width, height);
				glNamedFramebufferTexture(this->handle, GL_DEPTH_ATTACHMENT, this->depth->handle, 0);
			}
			
			this->width = width;
			this->height = height;
		}
		

	}

};

struct View{
	dmat4 view;
	dmat4 proj;
	shared_ptr<Framebuffer> framebuffer = nullptr; 
};

struct Camera{

	glm::dvec3 position;
	glm::dmat4 rotation;

	glm::dmat4 world;
	glm::dmat4 view;
	glm::dmat4 proj;

	double aspect = 1.0;  // 这是视口的宽高比，通常为窗口宽度与高度的比值。
	double fovy = 60.0;
	double near = 0.1;  // 近剪裁面距离，表示相机能看到的最近的距离。
	double far = 2'000'000.0;  // 远剪裁面距离，表示相机能看到的最远的距离。
	int width = 128;
	int height = 128;

	Camera(){

	}

	void setSize(int width, int height){
		this->width = width;
		this->height = height;
		this->aspect = double(width) / double(height); // Aspect Ratio（宽高比）
	}

	void update(){
		view =  glm::inverse(world);  // 计算world矩阵的逆矩阵
		// constexpr own
		constexpr double pi = glm::pi<double>();  // 使用 GLM 库中的 glm::pi<double>() 来获取 π（圆周率）的值
		proj = glm::perspective(pi * fovy / 180.0, aspect, near, far);  // 函数用于生成透视投影矩阵。
																		// 透视投影使得对象随着距离相机的远近而缩放，从而模拟真实世界中的视角。
	}


};



struct GLRenderer{

	GLFWwindow* window = nullptr;
	double fps = 0.0;
	int64_t frameCount = 0;
	
	shared_ptr<Camera> camera = nullptr;
	shared_ptr<OrbitControls> controls = nullptr;

	bool vrEnabled = false;
	
	View view;

	vector<function<void(vector<string>)>> fileDropListeners; // 创建一个存储function<void(vector<string>)>> 的容器。

	int width = 0;
	int height = 0;
	string selectedMethod = "";

	GLRenderer();

	void init();

	shared_ptr<Texture> createTexture(int width, int height, GLuint colorType);

	shared_ptr<Framebuffer> createFramebuffer(int width, int height);

	inline GLBuffer createBuffer(int64_t size){
		GLuint handle;
		glCreateBuffers(1, &handle);
		glNamedBufferStorage(handle, size, 0, GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT);

		GLBuffer buffer;
		buffer.handle = handle;
		buffer.size = size;

		return buffer;
	}

	inline GLBuffer createSparseBuffer(int64_t size){
		GLuint handle;
		glCreateBuffers(1, &handle);
		glNamedBufferStorage(handle, size, 0, GL_DYNAMIC_STORAGE_BIT | GL_SPARSE_STORAGE_BIT_ARB );

		GLBuffer buffer;
		buffer.handle = handle;
		buffer.size = size;

		return buffer;
	}

	inline GLBuffer createUniformBuffer(int64_t size){
		GLuint handle;
		glCreateBuffers(1, &handle);
		glNamedBufferStorage(handle, size, 0, GL_DYNAMIC_STORAGE_BIT );

		GLBuffer buffer;
		buffer.handle = handle;
		buffer.size = size;

		return buffer;
	}

	inline shared_ptr<Buffer> readBuffer(GLBuffer glBuffer, uint32_t offset, uint32_t size){

		auto target = make_shared<Buffer>(size);

		glGetNamedBufferSubData(glBuffer.handle, offset, size, target->data);

		return target;
	}

	void loop(function<void(void)> update, function<void(void)> render);

	void onFileDrop(function<void(vector<string>)> callback){
		fileDropListeners.push_back(callback);
	}

};