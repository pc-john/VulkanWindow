#include "VulkanWindow.h"
#include <vulkan/vulkan.hpp>
#include <algorithm>
#include <chrono>
#include <iostream>

using namespace std;


// constants
constexpr const char* appName = "MultiWindowTest";


// shader code in SPIR-V binary
static const uint32_t vsSpirv[] = {
#include "shader.vert.spv"
};
static const uint32_t fsSpirv[] = {
#include "shader.frag.spv"
};


// Window class
class Window : public VulkanWindow {
public:
	vk::SurfaceKHR surface;
	vk::SwapchainKHR swapchain;
	vector<vk::ImageView> swapchainImageViews;
	vector<vk::Framebuffer> framebuffers;
	vk::Pipeline pipeline;

	Window(vk::Instance instance, vk::Extent2D surfaceExtent, const char* title = "Vulkan window")
		: VulkanWindow()
		, surface(VulkanWindow::create(instance, surfaceExtent, title)) {}
	Window(Window&& other) noexcept : VulkanWindow(move(other))  {
		surface = other.surface;
		swapchain = other.swapchain; other.swapchain = nullptr;
		swapchainImageViews = move(other.swapchainImageViews);
		framebuffers = move(other.framebuffers);
		pipeline = other.pipeline; other.pipeline = nullptr;
	}
	~Window()  { destroyMembers(); }
	Window& operator=(Window&& other) noexcept {
		destroyMembers();
		VulkanWindow::operator=(move(other));
		surface = other.surface;
		swapchain = other.swapchain; other.swapchain = nullptr;
		swapchainImageViews = move(other.swapchainImageViews);
		framebuffers = move(other.framebuffers);
		pipeline = other.pipeline; other.pipeline = nullptr;
		return *this;
	}
protected:
	void destroyMembers() noexcept;  // destroys only members of this object; not any parent class members
};


void Window::destroyMembers() noexcept
{
	if(!_device)
		return;

	// wait for device idle state
	// (to prevent errors during destruction of Vulkan resources)
	vk::Device device = vk::Device(_device);
	try {
		device.waitIdle();
	} catch(vk::Error& e) {
		cout << "Failed because of Vulkan exception: " << e.what() << endl;
	}

	// destroy resources
	device.destroy(pipeline);
	pipeline = nullptr;
	for(auto f : framebuffers)  device.destroy(f);
	framebuffers.clear();
	for(auto v : swapchainImageViews)  device.destroy(v);
	swapchainImageViews.clear();
	device.destroy(swapchain);
	swapchain = nullptr;
}


// global application data
class App {
public:

	App(int argc, char* argv[]);
	~App();

	void init();
	void resize(VulkanWindow& window,
		const vk::SurfaceCapabilitiesKHR& surfaceCapabilities, vk::Extent2D newSurfaceExtent);
	void frame(VulkanWindow& window);

	// Vulkan instance must be destructed as the last Vulkan handle.
	// It is probably good idea to destroy it after the display connection.
	vk::Instance instance;

	// window needs to be destroyed after the swapchain
	// This is required especially by Wayland.
	vector<Window> windowList;

	// Vulkan variables, handles and objects
	// (they need to be destructed in non-arbitrary order in the destructor)
	vk::PhysicalDevice physicalDevice;
	uint32_t graphicsQueueFamily;
	uint32_t presentationQueueFamily;
	vk::Device device;
	vk::Queue graphicsQueue;
	vk::Queue presentationQueue;
	vk::SurfaceFormatKHR surfaceFormat;
	vk::RenderPass renderPass;
	vk::CommandPool commandPool;
	vk::CommandBuffer commandBuffer;
	vk::Semaphore imageAvailableSemaphore;
	vk::Semaphore renderFinishedSemaphore;
	vk::Fence renderFinishedFence;
	vk::ShaderModule vsModule;
	vk::ShaderModule fsModule;
	vk::PipelineLayout pipelineLayout;

	enum class FrameUpdateMode { OnDemand, Continuous, MaxFrameRate };
	FrameUpdateMode frameUpdateMode = FrameUpdateMode::Continuous;
	size_t frameID = ~size_t(0);
	size_t fpsNumFrames = ~size_t(0);
	chrono::high_resolution_clock::time_point fpsStartTime;

};


/// Construct application object
App::App(int argc, char** argv)
{
	// process command-line arguments
	for(int i=1; i<argc; i++)
		if(strcmp(argv[i], "--on-demand") == 0)
			frameUpdateMode = FrameUpdateMode::OnDemand;
		else if(strcmp(argv[i], "--continuous") == 0)
			frameUpdateMode = FrameUpdateMode::Continuous;
		else if(strcmp(argv[i], "--max-frame-rate") == 0)
			frameUpdateMode = FrameUpdateMode::MaxFrameRate;
		else {
			if(strcmp(argv[i], "--help") != 0 && strcmp(argv[i], "-h") != 0)
				cout << "Unrecognized option: " << argv[i] << endl;
			cout << appName << " usage:\n"
			        "   --help or -h:  usage information\n"
			        "   --on-demand:   on demand window content refresh,\n"
			        "                  this conserves computing resources\n"
			        "   --continuous:  constantly update window content using\n"
			        "                  screen refresh rate, this is the default\n"
			        "   --max-frame-rate:  ignore screen refresh rate, update\n"
			        "                      window content as often as possible\n" << endl;
			exit(99);
		}
}


App::~App()
{
	// wait for device idle state
	// (to prevent errors during destruction of Vulkan resources)
	if(device) {
		try {
			device.waitIdle();
		} catch(vk::Error& e) {
			cout << "Failed because of Vulkan exception: " << e.what() << endl;
		}
	}

	// destroy windows
	windowList.clear();

	// destroy handles
	// (the handles are destructed in certain (not arbitrary) order)
	if(device) {
		device.destroy(pipelineLayout);
		device.destroy(fsModule);
		device.destroy(vsModule);
		device.destroy(renderFinishedFence);
		device.destroy(renderFinishedSemaphore);
		device.destroy(imageAvailableSemaphore);
		device.destroy(commandPool);
		device.destroy(renderPass);
		device.destroy();
	}

#if defined(USE_PLATFORM_XLIB)
	// On Xlib, VulkanWindow::finalize() needs to be called before instance destroy to avoid crash.
	// It is workaround for the known bug in libXext: https://gitlab.freedesktop.org/xorg/lib/libxext/-/issues/3,
	// that crashes the application inside XCloseDisplay(). The problem seems to be present
	// especially on Nvidia drivers (reproduced on versions 470.129.06 and 515.65.01, for example).
	VulkanWindow::finalize();
#endif
	instance.destroy();
}


void App::init()
{
	// init VulkanWindow
	VulkanWindow::init();

	// Vulkan instance
	instance =
		vk::createInstance(
			vk::InstanceCreateInfo{
				vk::InstanceCreateFlags(),  // flags
				&(const vk::ApplicationInfo&)vk::ApplicationInfo{
					appName,                 // application name
					VK_MAKE_VERSION(0,0,0),  // application version
					nullptr,                 // engine name
					VK_MAKE_VERSION(0,0,0),  // engine version
					VK_API_VERSION_1_0,      // api version
				},
				0, nullptr,  // no layers
				VulkanWindow::requiredExtensionCount(),  // enabled extension count
				VulkanWindow::requiredExtensionNames(),  // enabled extension names
			}
		);

	// create surface
	windowList.emplace_back(instance, vk::Extent2D{800, 600}, appName);
	windowList.emplace_back(instance, vk::Extent2D{800, 600}, appName);

	// test for isVisible() returning false
	{
		VulkanWindow tmp;
		if(tmp.isVisible())
			throw runtime_error("VulkanWindow::isVisible() did not returned false immediately after VulkanWindow constructor.");
		tmp.create(instance, vk::Extent2D{800, 600}, appName);
		if(tmp.isVisible())
			throw runtime_error("VulkanWindow::isVisible() did not returned false immediately after VulkanWindow::create().");
		tmp.hide();
		if(tmp.isVisible())
			throw runtime_error("VulkanWindow::isVisible() did not returned false immediately after VulkanWindow::hide().");
	}

	// test for non-initialized window and for move constructors
	{
		VulkanWindow tmp1;
		VulkanWindow tmp2;
		VulkanWindow tmp3;
		tmp3 = move(tmp2);
		vector<VulkanWindow> windowVector;
		windowVector.emplace_back();
	}

	// find compatible devices
	vector<vk::PhysicalDevice> deviceList = instance.enumeratePhysicalDevices();
	vector<tuple<vk::PhysicalDevice, uint32_t, uint32_t, vk::PhysicalDeviceProperties>> compatibleDevices;
	for(vk::PhysicalDevice pd : deviceList) {

		// skip devices without VK_KHR_swapchain
		auto extensionList = pd.enumerateDeviceExtensionProperties();
		for(vk::ExtensionProperties& e : extensionList)
			if(strcmp(e.extensionName, "VK_KHR_swapchain") == 0)
				goto swapchainSupported;
		continue;
		swapchainSupported:

		// select queues for graphics rendering and for presentation
		uint32_t graphicsQueueFamily = UINT32_MAX;
		uint32_t presentationQueueFamily = UINT32_MAX;
		vector<vk::QueueFamilyProperties> queueFamilyList = pd.getQueueFamilyProperties();
		for(uint32_t i=0, c=uint32_t(queueFamilyList.size()); i<c; i++) {

			// test for presentation support
			bool presentationSupport = true;
			for(auto& w : windowList)
				if(!pd.getSurfaceSupportKHR(i, w.surface)) {
					presentationSupport = false;
					break;
				}

			if(presentationSupport) {

				// test for graphics operations support
				if(queueFamilyList[i].queueFlags & vk::QueueFlagBits::eGraphics) {
					// if presentation and graphics operations are supported on the same queue,
					// we will use single queue
					compatibleDevices.emplace_back(pd, i, i, pd.getProperties());
					goto nextDevice;
				}
				else
					// if only presentation is supported, we store the first such queue
					if(presentationQueueFamily == UINT32_MAX)
						presentationQueueFamily = i;
			}
			else {
				if(queueFamilyList[i].queueFlags & vk::QueueFlagBits::eGraphics)
					// if only graphics operations are supported, we store the first such queue
					if(graphicsQueueFamily == UINT32_MAX)
						graphicsQueueFamily = i;
			}
		}

		if(graphicsQueueFamily != UINT32_MAX && presentationQueueFamily != UINT32_MAX)
			// presentation and graphics operations are supported on the different queues
			compatibleDevices.emplace_back(pd, graphicsQueueFamily, presentationQueueFamily, pd.getProperties());
		nextDevice:;
	}

	// print compatible devices
	cout << "Compatible devices:" << endl;
	for(auto& t : compatibleDevices)
		cout << "   " << get<3>(t).deviceName << " (graphics queue: " << get<1>(t)
		     << ", presentation queue: " << get<2>(t)
		     << ", type: " << to_string(get<3>(t).deviceType) << ")" << endl;

	// choose the best device
	auto bestDevice = compatibleDevices.begin();
	if(bestDevice == compatibleDevices.end())
		throw runtime_error("No compatible devices.");
	constexpr const array deviceTypeScore = {
		10, // vk::PhysicalDeviceType::eOther         - lowest score
		40, // vk::PhysicalDeviceType::eIntegratedGpu - high score
		50, // vk::PhysicalDeviceType::eDiscreteGpu   - highest score
		30, // vk::PhysicalDeviceType::eVirtualGpu    - normal score
		20, // vk::PhysicalDeviceType::eCpu           - low score
		10, // unknown vk::PhysicalDeviceType
	};
	int bestScore = deviceTypeScore[clamp(int(get<3>(*bestDevice).deviceType), 0, int(deviceTypeScore.size())-1)];
	if(get<1>(*bestDevice) == get<2>(*bestDevice))
		bestScore++;
	for(auto it=compatibleDevices.begin()+1; it!=compatibleDevices.end(); it++) {
		int score = deviceTypeScore[clamp(int(get<3>(*it).deviceType), 0, int(deviceTypeScore.size())-1)];
		if(get<1>(*it) == get<2>(*it))
			score++;
		if(score > bestScore) {
			bestDevice = it;
			bestScore = score;
		}
	}
	cout << "Using device:\n"
	        "   " << get<3>(*bestDevice).deviceName << endl;
	physicalDevice = get<0>(*bestDevice);
	graphicsQueueFamily = get<1>(*bestDevice);
	presentationQueueFamily = get<2>(*bestDevice);

	// create device
	device =
		physicalDevice.createDevice(
			vk::DeviceCreateInfo{
				vk::DeviceCreateFlags(),  // flags
				graphicsQueueFamily==presentationQueueFamily ? uint32_t(1) : uint32_t(2),  // queueCreateInfoCount
				array{  // pQueueCreateInfos
					vk::DeviceQueueCreateInfo{
						vk::DeviceQueueCreateFlags(),
						graphicsQueueFamily,
						1,
						&(const float&)1.f,
					},
					vk::DeviceQueueCreateInfo{
						vk::DeviceQueueCreateFlags(),
						presentationQueueFamily,
						1,
						&(const float&)1.f,
					},
				}.data(),
				0, nullptr,  // no layers
				1,           // number of enabled extensions
				array<const char*, 1>{ "VK_KHR_swapchain" }.data(),  // enabled extension names
				nullptr,    // enabled features
			}
		);

	// get queues
	graphicsQueue = device.getQueue(graphicsQueueFamily, 0);
	presentationQueue = device.getQueue(presentationQueueFamily, 0);

	// print surface formats
	cout << "Surface formats:" << endl;
	vector<vk::SurfaceFormatKHR> availableSurfaceFormats = physicalDevice.getSurfaceFormatsKHR(windowList.front().surface);
	for(vk::SurfaceFormatKHR sf : availableSurfaceFormats)
		cout << "   " << vk::to_string(sf.format) << ", color space: " << vk::to_string(sf.colorSpace) << endl;

	// choose surface format
	constexpr const array allowedSurfaceFormats{
		vk::SurfaceFormatKHR{ vk::Format::eB8G8R8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear },
		vk::SurfaceFormatKHR{ vk::Format::eR8G8B8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear },
		vk::SurfaceFormatKHR{ vk::Format::eA8B8G8R8SrgbPack32, vk::ColorSpaceKHR::eSrgbNonlinear },
	};
	if(availableSurfaceFormats.size()==1 && availableSurfaceFormats[0].format==vk::Format::eUndefined)
		// Vulkan spec allowed single eUndefined value until 1.1.111 (2019-06-10)
		// with the meaning you can use any valid vk::Format value.
		// Now, it is forbidden, but let's handle any old driver.
		surfaceFormat = allowedSurfaceFormats[0];
	else {
		for(vk::SurfaceFormatKHR sf : availableSurfaceFormats) {
			auto it = std::find(allowedSurfaceFormats.begin(), allowedSurfaceFormats.end(), sf);
			if(it != allowedSurfaceFormats.end()) {
				surfaceFormat = *it;
				goto surfaceFormatFound;
			}
		}
		if(availableSurfaceFormats.size() == 0)  // Vulkan must return at least one format (this is mandated since Vulkan 1.0.37 (2016-10-10), but was missing in the spec before probably because of omission)
			throw std::runtime_error("Vulkan error: getSurfaceFormatsKHR() returned empty list.");
		surfaceFormat = availableSurfaceFormats[0];
	surfaceFormatFound:;
	}
	for(auto it=++windowList.begin(); it!=windowList.end(); it++) {
		availableSurfaceFormats = physicalDevice.getSurfaceFormatsKHR(it->surface);
		if(std::find(availableSurfaceFormats.begin(), availableSurfaceFormats.end(), surfaceFormat) == availableSurfaceFormats.end())
			throw std::runtime_error("Vulkan error: Surface format chosen for the first window is not available in the second window.");
	}
	cout << "Using format:\n"
	     << "   " << to_string(surfaceFormat.format) << ", color space: " << to_string(surfaceFormat.colorSpace) << endl;

	// render pass
	renderPass =
		device.createRenderPass(
			vk::RenderPassCreateInfo(
				vk::RenderPassCreateFlags(),  // flags
				1,      // attachmentCount
				array{  // pAttachments
					vk::AttachmentDescription(
						vk::AttachmentDescriptionFlags(),  // flags
						surfaceFormat.format,              // format
						vk::SampleCountFlagBits::e1,       // samples
						vk::AttachmentLoadOp::eClear,      // loadOp
						vk::AttachmentStoreOp::eStore,     // storeOp
						vk::AttachmentLoadOp::eDontCare,   // stencilLoadOp
						vk::AttachmentStoreOp::eDontCare,  // stencilStoreOp
						vk::ImageLayout::eUndefined,       // initialLayout
						vk::ImageLayout::ePresentSrcKHR    // finalLayout
					),
				}.data(),
				1,      // subpassCount
				array{  // pSubpasses
					vk::SubpassDescription(
						vk::SubpassDescriptionFlags(),     // flags
						vk::PipelineBindPoint::eGraphics,  // pipelineBindPoint
						0,        // inputAttachmentCount
						nullptr,  // pInputAttachments
						1,        // colorAttachmentCount
						array{    // pColorAttachments
							vk::AttachmentReference(
								0,  // attachment
								vk::ImageLayout::eColorAttachmentOptimal  // layout
							),
						}.data(),
						nullptr,  // pResolveAttachments
						nullptr,  // pDepthStencilAttachment
						0,        // preserveAttachmentCount
						nullptr   // pPreserveAttachments
					),
				}.data(),
				1,      // dependencyCount
				array{  // pDependencies
					vk::SubpassDependency(
						VK_SUBPASS_EXTERNAL,   // srcSubpass
						0,                     // dstSubpass
						vk::PipelineStageFlags(vk::PipelineStageFlagBits::eColorAttachmentOutput),  // srcStageMask
						vk::PipelineStageFlags(vk::PipelineStageFlagBits::eColorAttachmentOutput),  // dstStageMask
						vk::AccessFlags(),     // srcAccessMask
						vk::AccessFlags(vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite),  // dstAccessMask
						vk::DependencyFlags()  // dependencyFlags
					),
				}.data()
			)
		);

	// commandPool and commandBuffer
	commandPool =
		device.createCommandPool(
			vk::CommandPoolCreateInfo(
				vk::CommandPoolCreateFlagBits::eTransient |  // flags
					vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
				graphicsQueueFamily  // queueFamilyIndex
			)
		);
	commandBuffer =
		device.allocateCommandBuffers(
			vk::CommandBufferAllocateInfo(
				commandPool,  // commandPool
				vk::CommandBufferLevel::ePrimary,  // level
				1  // commandBufferCount
			)
		)[0];

	// rendering semaphores and fences
	imageAvailableSemaphore =
		device.createSemaphore(
			vk::SemaphoreCreateInfo(
				vk::SemaphoreCreateFlags()  // flags
			)
		);
	renderFinishedSemaphore =
		device.createSemaphore(
			vk::SemaphoreCreateInfo(
				vk::SemaphoreCreateFlags()  // flags
			)
		);
	renderFinishedFence =
		device.createFence(
			vk::FenceCreateInfo(
				vk::FenceCreateFlagBits::eSignaled  // flags
			)
		);

	// create shader modules
	vsModule =
		device.createShaderModule(
			vk::ShaderModuleCreateInfo(
				vk::ShaderModuleCreateFlags(),  // flags
				sizeof(vsSpirv),  // codeSize
				vsSpirv  // pCode
			)
		);
	fsModule =
		device.createShaderModule(
			vk::ShaderModuleCreateInfo(
				vk::ShaderModuleCreateFlags(),  // flags
				sizeof(fsSpirv),  // codeSize
				fsSpirv  // pCode
			)
		);

	// pipeline layout
	pipelineLayout =
		device.createPipelineLayout(
			vk::PipelineLayoutCreateInfo{
				vk::PipelineLayoutCreateFlags(),  // flags
				0,       // setLayoutCount
				nullptr, // pSetLayouts
				0,       // pushConstantRangeCount
				nullptr  // pPushConstantRanges
			}
		);
}


/** Recreate swapchain and pipeline callback method.
 *  The method is usually called after the window resize and on the application start. */
void App::resize(VulkanWindow& w, const vk::SurfaceCapabilitiesKHR& surfaceCapabilities, vk::Extent2D newSurfaceExtent)
{
	Window& window = static_cast<Window&>(w);

	// clear resources
	for(auto v : window.swapchainImageViews)  device.destroy(v);
	window.swapchainImageViews.clear();
	for(auto f : window.framebuffers)  device.destroy(f);
	window.framebuffers.clear();
	device.destroy(window.pipeline);
	window.pipeline = nullptr;

	// print info
	cout << "Recreating swapchain (extent: " << newSurfaceExtent.width << "x" << newSurfaceExtent.height
	     << ", extent by surfaceCapabilities: " << surfaceCapabilities.currentExtent.width << "x"
	     << surfaceCapabilities.currentExtent.height << ", minImageCount: " << surfaceCapabilities.minImageCount
	     << ", maxImageCount: " << surfaceCapabilities.maxImageCount << ")" << endl;

	// create new swapchain
	constexpr const uint32_t requestedImageCount = 2;
	vk::UniqueSwapchainKHR newSwapchain =
		device.createSwapchainKHRUnique(
			vk::SwapchainCreateInfoKHR(
				vk::SwapchainCreateFlagsKHR(),  // flags
				window.surface,                 // surface
				surfaceCapabilities.maxImageCount==0  // minImageCount
					? max(requestedImageCount, surfaceCapabilities.minImageCount)
					: clamp(requestedImageCount, surfaceCapabilities.minImageCount, surfaceCapabilities.maxImageCount),
				surfaceFormat.format,           // imageFormat
				surfaceFormat.colorSpace,       // imageColorSpace
				newSurfaceExtent,               // imageExtent
				1,                              // imageArrayLayers
				vk::ImageUsageFlagBits::eColorAttachment,  // imageUsage
				(graphicsQueueFamily==presentationQueueFamily) ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent, // imageSharingMode
				uint32_t(2),  // queueFamilyIndexCount
				array<uint32_t, 2>{graphicsQueueFamily, presentationQueueFamily}.data(),  // pQueueFamilyIndices
				surfaceCapabilities.currentTransform,    // preTransform
				vk::CompositeAlphaFlagBitsKHR::eOpaque,  // compositeAlpha
				[](FrameUpdateMode frameUpdateMode, vk::PhysicalDevice physicalDevice, VulkanWindow& window)  // presentMode
					{
						// for MaxFrameRate, try Mailbox and Immediate if they are available
						if(frameUpdateMode == FrameUpdateMode::MaxFrameRate) {
							vector<vk::PresentModeKHR> modes =
								physicalDevice.getSurfacePresentModesKHR(window.surface());
							if(find(modes.begin(), modes.end(), vk::PresentModeKHR::eMailbox) != modes.end())
								return vk::PresentModeKHR::eMailbox;
							if(find(modes.begin(), modes.end(), vk::PresentModeKHR::eImmediate) != modes.end())
								return vk::PresentModeKHR::eImmediate;
						}

						// return Fifo that is always supported
						return vk::PresentModeKHR::eFifo;
					}(frameUpdateMode, physicalDevice, window),
				VK_TRUE,  // clipped
				window.swapchain  // oldSwapchain
			)
		);
	device.destroy(window.swapchain);
	window.swapchain = newSwapchain.release();

	// swapchain images and image views
	vector<vk::Image> swapchainImages = device.getSwapchainImagesKHR(window.swapchain);
	window.swapchainImageViews.reserve(swapchainImages.size());
	for(vk::Image image : swapchainImages)
		window.swapchainImageViews.emplace_back(
			device.createImageView(
				vk::ImageViewCreateInfo(
					vk::ImageViewCreateFlags(),  // flags
					image,                       // image
					vk::ImageViewType::e2D,      // viewType
					surfaceFormat.format,        // format
					vk::ComponentMapping(),      // components
					vk::ImageSubresourceRange(   // subresourceRange
						vk::ImageAspectFlagBits::eColor,  // aspectMask
						0,  // baseMipLevel
						1,  // levelCount
						0,  // baseArrayLayer
						1   // layerCount
					)
				)
			)
		);

	// framebuffers
	window.framebuffers.reserve(swapchainImages.size());
	for(size_t i=0, c=swapchainImages.size(); i<c; i++)
		window.framebuffers.emplace_back(
			device.createFramebuffer(
				vk::FramebufferCreateInfo(
					vk::FramebufferCreateFlags(),  // flags
					renderPass,  // renderPass
					1,  // attachmentCount
					&window.swapchainImageViews[i],  // pAttachments
					newSurfaceExtent.width,  // width
					newSurfaceExtent.height,  // height
					1  // layers
				)
			)
		);

	// pipeline
	window.pipeline =
		device.createGraphicsPipeline(
			nullptr,  // pipelineCache
			vk::GraphicsPipelineCreateInfo(
				vk::PipelineCreateFlags(),  // flags

				// shader stages
				2,  // stageCount
				array{  // pStages
					vk::PipelineShaderStageCreateInfo{
						vk::PipelineShaderStageCreateFlags(),  // flags
						vk::ShaderStageFlagBits::eVertex,  // stage
						vsModule,  // module
						"main",  // pName
						nullptr  // pSpecializationInfo
					},
					vk::PipelineShaderStageCreateInfo{
						vk::PipelineShaderStageCreateFlags(),  // flags
						vk::ShaderStageFlagBits::eFragment,  // stage
						fsModule,  // module
						"main",  // pName
						nullptr  // pSpecializationInfo
					},
				}.data(),

				// vertex input
				&(const vk::PipelineVertexInputStateCreateInfo&)vk::PipelineVertexInputStateCreateInfo{  // pVertexInputState
					vk::PipelineVertexInputStateCreateFlags(),  // flags
					0,        // vertexBindingDescriptionCount
					nullptr,  // pVertexBindingDescriptions
					0,        // vertexAttributeDescriptionCount
					nullptr   // pVertexAttributeDescriptions
				},

				// input assembly
				&(const vk::PipelineInputAssemblyStateCreateInfo&)vk::PipelineInputAssemblyStateCreateInfo{  // pInputAssemblyState
					vk::PipelineInputAssemblyStateCreateFlags(),  // flags
					vk::PrimitiveTopology::eTriangleList,  // topology
					VK_FALSE  // primitiveRestartEnable
				},

				// tessellation
				nullptr, // pTessellationState

				// viewport
				&(const vk::PipelineViewportStateCreateInfo&)vk::PipelineViewportStateCreateInfo{  // pViewportState
					vk::PipelineViewportStateCreateFlags(),  // flags
					1,  // viewportCount
					array{  // pViewports
						vk::Viewport(0.f, 0.f, float(newSurfaceExtent.width), float(newSurfaceExtent.height), 0.f, 1.f),
					}.data(),
					1,  // scissorCount
					array{  // pScissors
						vk::Rect2D(vk::Offset2D(0,0), newSurfaceExtent)
					}.data(),
				},

				// rasterization
				&(const vk::PipelineRasterizationStateCreateInfo&)vk::PipelineRasterizationStateCreateInfo{  // pRasterizationState
					vk::PipelineRasterizationStateCreateFlags(),  // flags
					VK_FALSE,  // depthClampEnable
					VK_FALSE,  // rasterizerDiscardEnable
					vk::PolygonMode::eFill,  // polygonMode
					vk::CullModeFlagBits::eNone,  // cullMode
					vk::FrontFace::eCounterClockwise,  // frontFace
					VK_FALSE,  // depthBiasEnable
					0.f,  // depthBiasConstantFactor
					0.f,  // depthBiasClamp
					0.f,  // depthBiasSlopeFactor
					1.f   // lineWidth
				},

				// multisampling
				&(const vk::PipelineMultisampleStateCreateInfo&)vk::PipelineMultisampleStateCreateInfo{  // pMultisampleState
					vk::PipelineMultisampleStateCreateFlags(),  // flags
					vk::SampleCountFlagBits::e1,  // rasterizationSamples
					VK_FALSE,  // sampleShadingEnable
					0.f,       // minSampleShading
					nullptr,   // pSampleMask
					VK_FALSE,  // alphaToCoverageEnable
					VK_FALSE   // alphaToOneEnable
				},

				// depth and stencil
				nullptr,  // pDepthStencilState

				// blending
				&(const vk::PipelineColorBlendStateCreateInfo&)vk::PipelineColorBlendStateCreateInfo{  // pColorBlendState
					vk::PipelineColorBlendStateCreateFlags(),  // flags
					VK_FALSE,  // logicOpEnable
					vk::LogicOp::eClear,  // logicOp
					1,  // attachmentCount
					array{  // pAttachments
						vk::PipelineColorBlendAttachmentState{
							VK_FALSE,  // blendEnable
							vk::BlendFactor::eZero,  // srcColorBlendFactor
							vk::BlendFactor::eZero,  // dstColorBlendFactor
							vk::BlendOp::eAdd,       // colorBlendOp
							vk::BlendFactor::eZero,  // srcAlphaBlendFactor
							vk::BlendFactor::eZero,  // dstAlphaBlendFactor
							vk::BlendOp::eAdd,       // alphaBlendOp
							vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
								vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA  // colorWriteMask
						},
					}.data(),
					array<float,4>{0.f,0.f,0.f,0.f}  // blendConstants
				},

				nullptr,  // pDynamicState
				pipelineLayout,  // layout
				renderPass,  // renderPass
				0,  // subpass
				vk::Pipeline(nullptr),  // basePipelineHandle
				-1 // basePipelineIndex
			)
		).value;
}


void App::frame(VulkanWindow& w)
{
	Window& window = static_cast<Window&>(w);
	cout << "x" << flush;

	// wait for previous frame rendering work
	// if still not finished
	vk::Result r =
		device.waitForFences(
			renderFinishedFence,  // fences
			VK_TRUE,  // waitAll
			uint64_t(3e9)  // timeout
		);
	if(r != vk::Result::eSuccess) {
		if(r == vk::Result::eTimeout)
			throw runtime_error("GPU timeout. Task is probably hanging on GPU.");
		throw runtime_error("Vulkan error: vkWaitForFences failed with error " + to_string(r) + ".");
	}
	device.resetFences(renderFinishedFence);

	// increment frame counter
	frameID++;

	// measure FPS
	fpsNumFrames++;
	if(fpsNumFrames == 0)
		fpsStartTime = chrono::high_resolution_clock::now();
	else {
		auto t = chrono::high_resolution_clock::now();
		auto dt = t - fpsStartTime;
		if(dt >= chrono::seconds(2)) {
			cout << "FPS: " << fpsNumFrames/chrono::duration<double>(dt).count() << endl;
			fpsNumFrames = 0;
			fpsStartTime = t;
		}
	}

	// acquire image
	uint32_t imageIndex;
	r =
		device.acquireNextImageKHR(
			window.swapchain,         // swapchain
			uint64_t(3e9),            // timeout (3s)
			imageAvailableSemaphore,  // semaphore to signal
			vk::Fence(nullptr),       // fence to signal
			&imageIndex               // pImageIndex
		);
	if(r != vk::Result::eSuccess) {
		if(r == vk::Result::eSuboptimalKHR) {
			window.scheduleResize();
			cout << "acquire result: Suboptimal" << endl;
			return;
		} else if(r == vk::Result::eErrorOutOfDateKHR) {
			window.scheduleResize();
			cout << "acquire error: OutOfDate" << endl;
			return;
		} else
			throw runtime_error("Vulkan error: vkAcquireNextImageKHR failed with error " + to_string(r) + ".");
	}

	// record command buffer
	commandBuffer.begin(
		vk::CommandBufferBeginInfo(
			vk::CommandBufferUsageFlagBits::eOneTimeSubmit,  // flags
			nullptr  // pInheritanceInfo
		)
	);
	commandBuffer.beginRenderPass(
		vk::RenderPassBeginInfo(
			renderPass,  // renderPass
			window.framebuffers[imageIndex],  // framebuffer
			vk::Rect2D(vk::Offset2D(0, 0), window.surfaceExtent()),  // renderArea
			1,  // clearValueCount
			&(const vk::ClearValue&)vk::ClearValue(  // pClearValues
				vk::ClearColorValue(array<float, 4>{0.0f, 0.0f, 0.0f, 1.f})
			)
		),
		vk::SubpassContents::eInline
	);

	// rendering commands
	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, window.pipeline);  // bind pipeline
	commandBuffer.draw(  // draw single triangle
		3,  // vertexCount
		1,  // instanceCount
		0,  // firstVertex
		uint32_t(frameID)  // firstInstance
	);

	// end render pass and command buffer
	commandBuffer.endRenderPass();
	commandBuffer.end();

	// submit frame
	graphicsQueue.submit(
		vk::ArrayProxy<const vk::SubmitInfo>(
			1,
			&(const vk::SubmitInfo&)vk::SubmitInfo(
				1, &imageAvailableSemaphore,  // waitSemaphoreCount + pWaitSemaphores +
				&(const vk::PipelineStageFlags&)vk::PipelineStageFlags(  // pWaitDstStageMask
					vk::PipelineStageFlagBits::eColorAttachmentOutput),
				1, &commandBuffer,  // commandBufferCount + pCommandBuffers
				1, &renderFinishedSemaphore  // signalSemaphoreCount + pSignalSemaphores
			)
		),
		renderFinishedFence  // fence
	);

	// present
	r =
		presentationQueue.presentKHR(
			&(const vk::PresentInfoKHR&)vk::PresentInfoKHR(
				1, &renderFinishedSemaphore,  // waitSemaphoreCount + pWaitSemaphores
				1, &window.swapchain, &imageIndex,  // swapchainCount + pSwapchains + pImageIndices
				nullptr  // pResults
			)
		);
	if(r != vk::Result::eSuccess) {
		if(r == vk::Result::eSuboptimalKHR) {
			window.scheduleResize();
			cout << "present result: Suboptimal" << endl;
		} else if(r == vk::Result::eErrorOutOfDateKHR) {
			window.scheduleResize();
			cout << "present error: OutOfDate" << endl;
		} else
			throw runtime_error("Vulkan error: vkQueuePresentKHR() failed with error " + to_string(r) + ".");
	}

	// schedule next frame
	if(frameUpdateMode != FrameUpdateMode::OnDemand)
		window.scheduleFrame();
}


int main(int argc, char* argv[])
{
	// catch exceptions
	// (vulkan.hpp functions throw if they fail)
	try {

		App app(argc, argv);
		app.init();
		for(Window& w : app.windowList) {
			w.setDevice(app.device, app.physicalDevice);
			w.setResizeCallback(
				bind(
					&App::resize,
					&app,
					placeholders::_1,
					placeholders::_2,
					placeholders::_3
				)
			);
			w.setFrameCallback(
				bind(&App::frame, &app, placeholders::_1)
			);
			w.setCloseCallback(
				bind(
					[](VulkanWindow& window, App& app){
#if 0
						window.hide();
						for(Window& w : app.windowList)
							if(w.isVisible())
								return;
						VulkanWindow::exitMainLoop();
#elif 0
						// destroy window
						auto it = find_if(app.windowList.begin(), app.windowList.end(),
						                  [&window](const Window& w){ return &window==&w; });
						app.windowList.erase(it);

						// if no more windows, exit application
						if(app.windowList.empty())
							VulkanWindow::exitMainLoop();
#else
						static int counter = int(app.windowList.size());
						if(counter > 0) {

							// test of hide()
							counter--;
							window.hide();
							if(window.isVisible())
								throw runtime_error("VulkanWindow::hide() does not work properly.");

							// test of move operators, doing windowList order reverse
							vector<Window> tmp;
							while(app.windowList.size() > 0) {
								tmp.push_back(move(app.windowList.back()));
								app.windowList.pop_back();
							}
							app.windowList.swap(tmp);
						}
						if(counter == 0) {

							// test of show()
							counter = -1;
							for(auto& w : app.windowList) {
								w.show();
								if(!w.isVisible())
									throw runtime_error("VulkanWindow::show() does not work properly.");
							}
							return;
						}
						if(counter < 0) {

							// delete window
							auto it = find_if(app.windowList.begin(), app.windowList.end(),
							                  [&window](const Window& w){ return &window==&w; });
							if(it == app.windowList.end())
								throw runtime_error("Window is not in the window list.");
							app.windowList.erase(it);

							// if no more windows, exit application
							if(app.windowList.empty())
								VulkanWindow::exitMainLoop();
						}
#endif
					},
					placeholders::_1,
					ref(app)
				)
			);

			// test of hide()/show()/isVisible()
			w.show();
#if 1
			if(!w.isVisible())
				throw runtime_error("VulkanWindow::isVisible() did not returned true immediately after VulkanWindow::show().");
			w.hide();
			if(w.isVisible())
				throw runtime_error("VulkanWindow::hide() does not work properly.");
			w.show();
			if(!w.isVisible())
				throw runtime_error("VulkanWindow::isVisible() did not returned true immediately after VulkanWindow::show().");
#endif
		}

		// main loop
		VulkanWindow::mainLoop();

	// catch exceptions
	} catch(vk::Error& e) {
		cout << "Failed because of Vulkan exception: " << e.what() << endl;
	} catch(exception& e) {
		cout << "Failed because of exception: " << e.what() << endl;
	} catch(...) {
		cout << "Failed because of unspecified exception." << endl;
	}

	VulkanWindow::finalize();
	return 0;
}
