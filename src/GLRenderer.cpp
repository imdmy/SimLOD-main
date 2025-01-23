
#include <filesystem>

#include "GLRenderer.h"
#include "Runtime.h"

namespace fs = std::filesystem;

auto _controls = make_shared<OrbitControls>();


static void APIENTRY debugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {

	if (
		severity == GL_DEBUG_SEVERITY_NOTIFICATION 
		|| severity == GL_DEBUG_SEVERITY_LOW 
		|| severity == GL_DEBUG_SEVERITY_MEDIUM
		) {
		return;
	}

	cout << "OPENGL DEBUG CALLBACK: " << message << endl;
}

void error_callback(int error, const char* description){
	fprintf(stderr, "Error: %s\n", description);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods){

	cout << "key: " << key << ", scancode: " << scancode << ", action: " << action << ", mods: " << mods << endl;

	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	}

	Runtime::keyStates[key] = action;

	cout << key << endl;
}

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos){
	ImGuiIO& io = ImGui::GetIO();
	if(io.WantCaptureMouse){
		return;
	}
	
	Runtime::mousePosition = {xpos, ypos};

	_controls->onMouseMove(xpos, ypos);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset){
	ImGuiIO& io = ImGui::GetIO();
	if(io.WantCaptureMouse){
		return;
	}

	_controls->onMouseScroll(xoffset, yoffset);
}


void drop_callback(GLFWwindow* window, int count, const char **paths){
	for(int i = 0; i < count; i++){
		cout <<  paths[i] << endl;
	}
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods){

	// cout << "start button: " << button << ", action: " << action << ", mods: " << mods << endl;

	ImGuiIO& io = ImGui::GetIO();
	if(io.WantCaptureMouse){
		return;
	}

	// cout << "end button: " << button << ", action: " << action << ", mods: " << mods << endl;


	if(action == 1){
		Runtime::mouseButtons = Runtime::mouseButtons | (1 << button);
	}else if(action == 0){
		uint32_t mask = ~(1 << button);
		Runtime::mouseButtons = Runtime::mouseButtons & mask;
	}

	_controls->onMouseButton(button, action, mods);
}

GLRenderer::GLRenderer(){
	cout << "<GLRenderer constructed!>" << endl; // own
	this->controls = _controls;
	camera = make_shared<Camera>();

	init();

	view.framebuffer = this->createFramebuffer(128, 128);
}

void GLRenderer::init(){
	glfwSetErrorCallback(error_callback);

	if (!glfwInit()) {
		// Initialization failed
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_DECORATED, true);
	glfwWindowHint(GLFW_RESIZABLE, true);
	glfwWindowHint(GLFW_VISIBLE, true);

	int numMonitors;
	GLFWmonitor** monitors = glfwGetMonitors(&numMonitors);  // 获取所有显示器

	cout << "<create windows>" << endl;
	cout << "numMonitors: " << numMonitors << endl;
	if (numMonitors > 1) {  // 处理多显示器情形
		const GLFWvidmode * modeLeft = glfwGetVideoMode(monitors[0]);
		const GLFWvidmode * modeRight = glfwGetVideoMode(monitors[1]);

		cout << "modeLeft->width: " << modeLeft->width << ", modeLeft->height: " << modeLeft->height << endl;  // own 主屏幕的分辨率

		window = glfwCreateWindow(1920, 1080, "Simultaneous LOD Generation and Rendering", nullptr, nullptr);
		// window = glfwCreateWindow(modeRight->width, modeRight->height - 300, "Simple example", nullptr, nullptr);

		if (window == nullptr) { // 创建窗口失败
			cout << "Failed to create GLFW window" << endl;
			glfwTerminate();
			exit(EXIT_FAILURE);
		}

		// SECOND MONITOR
		//int xpos;
		//int ypos;
		// glfwGetMonitorPos(monitors[1], &xpos, &ypos);
		// glfwSetWindowPos(window, xpos, ypos);

		// FIRST
		glfwSetWindowPos(window, 210, 100);  // 窗口位置
		// 设置窗口大小
		glfwSetWindowSize(window, 1400, 900);  // 窗口大小

	} else if (numMonitors == 1) {  // 处理单显示器情形
	} else {
		const GLFWvidmode * mode = glfwGetVideoMode(monitors[0]);  // 获取主显示器的的视频模式（分辨率、颜色深度、刷新率）
		/*
		out << "width: " << mode->width << ", height: " << mode->height << endl;
		cout << "refresh rate: " << mode->refreshRate << endl;
		cout << "RGB_bits: " << mode->redBits << " " << mode->greenBits << " " << mode->blueBits << endl;
		*/
		window = glfwCreateWindow(mode->width - 100, mode->height - 100, "Simple example", nullptr, nullptr);

		if (!window) {
			glfwTerminate();
			exit(EXIT_FAILURE);
		}

		glfwSetWindowPos(window, 50, 50);
	}

	cout << "<set input callbacks>" << endl;
	glfwSetKeyCallback(window, key_callback);  // 键盘输入回调
	glfwSetCursorPosCallback(window, cursor_position_callback);  // 鼠标位置回调
	glfwSetMouseButtonCallback(window, mouse_button_callback);  //	鼠标按钮回调
	glfwSetScrollCallback(window, scroll_callback);  // 鼠标滚轮回调
	// glfwSetDropCallback(window, drop_callback);  // 文件拖放回调

	static GLRenderer* ref = this;
	glfwSetDropCallback(window, [](GLFWwindow*, int count, const char **paths){  // 文件拖放回调, lambda

		vector<string> files;
		for(int i = 0; i < count; i++){  // 对拖放的文件进行遍历，存入file容器
			string file = paths[i];
			files.push_back(file);
		}

		for(auto &listener : ref->fileDropListeners){  // 遍历fileDropListeners容器，调用每个listener的operator()
			listener(files);  // 调用listener的operator() 使用lambda回调执行回调函数处理文件
		}
	});

	glfwMakeContextCurrent(window);  // 设置当前窗口的上下文
	glfwSwapInterval(0); // 垂直同步

	GLenum err = glewInit();
	if (GLEW_OK != err) {
		/* Problem: glewInit failed, something is seriously wrong. */
		fprintf(stderr, "glew error: %s\n", glewGetErrorString(err));
	}

	cout << "<glewInit done> " << "(" << now() << ")" << endl;

	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_HIGH, 0, NULL, GL_TRUE);
	glDebugMessageCallback(debugCallback, NULL);

	{ // SETUP IMGUI
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImPlot::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init("#version 450");
		ImGui::StyleColorsDark();
	}
}

shared_ptr<Texture> GLRenderer::createTexture(int width, int height, GLuint colorType) {

	auto texture = Texture::create(width, height, colorType, this);

	return texture;
}

shared_ptr<Framebuffer> GLRenderer::createFramebuffer(int width, int height) {

	auto framebuffer = Framebuffer::create(this);

	GLenum status = glCheckNamedFramebufferStatus(framebuffer->handle, GL_FRAMEBUFFER);

	if (status != GL_FRAMEBUFFER_COMPLETE) {
		cout << "framebuffer incomplete" << endl;
	}

	framebuffer->setSize(width, height);

	return framebuffer;
}

void GLRenderer::loop(function<void(void)> update, function<void(void)> render){

	int fpsCounter = 0;
	double start = now();
	double tPrevious = start; // 定义开始时间
	double tPreviousFPSMeasure = start; 

	vector<float> frameTimes(1000, 0); // 创建一个大小为1000 * sizeof(float)大小的vector，将数组中1000个元素都用0来初始化。

	// 渲染循环
	while (!glfwWindowShouldClose(window)){

		// TIMING
		double timeSinceLastFrame; 
		{
			double tCurrent = now();
			timeSinceLastFrame = tCurrent - tPrevious;  // 从loop开始到while渲染开始的时间
			tPrevious = tCurrent;

			double timeSinceLastFPSMeasure = tCurrent - tPreviousFPSMeasure; // 计算render函数开始到现在的时间，计算上次测量到当前的时间。

			if(timeSinceLastFPSMeasure >= 1.0){
				this->fps = double(fpsCounter) / timeSinceLastFPSMeasure;

				tPreviousFPSMeasure = tCurrent;
				fpsCounter = 0;
			}
			frameTimes[frameCount % frameTimes.size()] = static_cast<float>(timeSinceLastFrame); // 将当前帧渲染的时间记录到数组（依据索引）中
		}
		
		// WINDOW
		int width, height;
		glfwGetWindowSize(window, &width, &height);  // 获取当前的窗口尺寸
		camera->setSize(width, height);
		this->width = width;
		this->height = height;

		EventQueue::instance->process(); //实现了一个事件队列的处理机制，其中的逻辑通过多线程安全方式处理了一个存储事件（或任务）的队列。

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, this->width, this->height);

		glBindFramebuffer(GL_FRAMEBUFFER, view.framebuffer->handle);
		glViewport(0, 0, this->width, this->height);


		{ 
			controls->update();

			camera->world = controls->world;
			camera->position = camera->world * dvec4(0.0, 0.0, 0.0, 1.0);
		}

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		{ // UPDATE & RENDER
			camera->update();
			update();

			camera->update();
			render();
		}

		// IMGUI
		{
			ImGui::SetNextWindowPos(ImVec2(10, 0));
			ImGui::SetNextWindowSize(ImVec2(490, 15));
			ImGui::Begin("Test", nullptr,
				ImGuiWindowFlags_NoTitleBar 
				| ImGuiWindowFlags_NoScrollbar
				| ImGuiWindowFlags_NoMove
				| ImGuiWindowFlags_NoResize
				| ImGuiWindowFlags_NoBackground
			);

			bool toggleGUI = ImGui::Button("Toggle GUI");
			if(toggleGUI){
				Runtime::showGUI = !Runtime::showGUI;
			}
			ImGui::End();
		}

		auto windowSize_perf = ImVec2(490, 260);

		if(Runtime::showGUI && Runtime::showPerfGraph)
		// if(false)
		{ // RENDER IMGUI PERFORMANCE WINDOW

			stringstream ssFPS; 
			ssFPS << this->fps;
			string strFps = ssFPS.str();

			ImGui::SetNextWindowPos(ImVec2(10, 30));
			ImGui::SetNextWindowSize(windowSize_perf);

			ImGui::Begin("Performance");
			string fpsStr = rightPad("FPS:", 30) + strFps;
			ImGui::Text(fpsStr.c_str());

			static float history = 2.0f;
			static ScrollingBuffer sFrames;
			static ScrollingBuffer s60fps;
			static ScrollingBuffer s120fps;
			float t = static_cast<float>(now());

			sFrames.AddPoint(t, 1000.0f * static_cast<float>(timeSinceLastFrame));

			// sFrames.AddPoint(t, 1000.0f * timeSinceLastFrame);
			s60fps.AddPoint(t, 1000.0f / 60.0f);
			s120fps.AddPoint(t, 1000.0f / 120.0f);
			static ImPlotAxisFlags rt_axis = ImPlotAxisFlags_NoTickLabels;
			ImPlot::SetNextPlotLimitsX(t - history, t, ImGuiCond_Always);
			ImPlot::SetNextPlotLimitsY(0, 30, ImGuiCond_Always);

			if (ImPlot::BeginPlot("Timings", nullptr, nullptr, ImVec2(-1, 200))){

				auto x = &sFrames.Data[0].x;
				auto y = &sFrames.Data[0].y;
				ImPlot::PlotShaded("frame time(ms)", x, y, sFrames.Data.size(), -Infinity, sFrames.Offset, 2 * sizeof(float));

				ImPlot::PlotLine("16.6ms (60 FPS)", &s60fps.Data[0].x, &s60fps.Data[0].y, s60fps.Data.size(), s60fps.Offset, 2 * sizeof(float));
				ImPlot::PlotLine(" 8.3ms (120 FPS)", &s120fps.Data[0].x, &s120fps.Data[0].y, s120fps.Data.size(), s120fps.Offset, 2 * sizeof(float));

				ImPlot::EndPlot();
			}

			ImGui::End();
		}

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		auto source = view.framebuffer;
		glBlitNamedFramebuffer(
			source->handle, 0,
			0, 0, source->width, source->height,
			0, 0, 0 + source->width, 0 + source->height,
			GL_COLOR_BUFFER_BIT, GL_LINEAR);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, this->width, this->height);


		glfwSwapBuffers(window);
		glfwPollEvents();

		fpsCounter++;
		frameCount++;
	}

	glfwDestroyWindow(window);
	glfwTerminate();
	exit(EXIT_SUCCESS);
}

shared_ptr<Framebuffer> Framebuffer::create(GLRenderer* renderer) {

	auto fbo = make_shared<Framebuffer>();
	fbo->renderer = renderer;

	glCreateFramebuffers(1, &fbo->handle);

	{ // COLOR ATTACHMENT 0

		auto texture = renderer->createTexture(fbo->width, fbo->height, GL_RGBA8);
		fbo->colorAttachments.push_back(texture);

		glNamedFramebufferTexture(fbo->handle, GL_COLOR_ATTACHMENT0, texture->handle, 0);
	}

	{ // DEPTH ATTACHMENT

		auto texture = renderer->createTexture(fbo->width, fbo->height, GL_DEPTH_COMPONENT32F);
		fbo->depth = texture;

		glNamedFramebufferTexture(fbo->handle, GL_DEPTH_ATTACHMENT, texture->handle, 0);
	}

	fbo->setSize(128, 128);

	return fbo;
}

shared_ptr<Texture> Texture::create(int width, int height, GLuint colorType, GLRenderer* renderer){

	GLuint handle;
	glCreateTextures(GL_TEXTURE_2D, 1, &handle);

	auto texture = make_shared<Texture>();
	texture->renderer = renderer;
	texture->handle = handle;
	texture->colorType = colorType;

	texture->setSize(width, height);

	return texture;
}

void Texture::setSize(int width, int height) {

	bool needsResize = this->width != width || this->height != height;

	if (needsResize) {

		glDeleteTextures(1, &this->handle);  // 删除 ‘1’ 个纹理对象
		glCreateTextures(GL_TEXTURE_2D, 1, &this->handle);

		glTextureParameteri(this->handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);  // GL_CLAMP_TO_EDGE：表示纹理坐标超出 0.0 到 1.0 的范围时，纹理会被拉伸到边缘颜色。这通常用于防止纹理在重复时产生边缘效应。
		glTextureParameteri(this->handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTextureParameteri(this->handle, GL_TEXTURE_MIN_FILTER, GL_NEAREST);  // 分别指定纹理在缩小和放大时的过滤方式：GL_NEAREST：表示使用最近邻过滤，这种方法比较简单直接，不进行插值。优点是速度快，但放大纹理时可能显得不平滑。
		glTextureParameteri(this->handle, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		glTextureStorage2D(this->handle, 1, this->colorType, width, height);

		this->width = width;
		this->height = height;
	}

}