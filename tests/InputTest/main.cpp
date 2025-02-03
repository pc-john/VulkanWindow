#include "VulkanWindow.h"
#include <vulkan/vulkan.hpp>
#include <algorithm>
#include <iostream>
#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN  // reduce amount of included files by windows.h
# include <windows.h>  // needed for SetConsoleOutputCP()
#endif

using namespace std;


// constants
constexpr const char* appName = "InputTest";


// global application data
class App {
public:

	App(int argc, char* argv[]);
	~App();

	void init();
	void resize(VulkanWindow& window, const vk::SurfaceCapabilitiesKHR& surfaceCapabilities, vk::Extent2D newSurfaceExtent);
	void frame(VulkanWindow& window);
	void mouseMove(VulkanWindow& window, const VulkanWindow::MouseState& mouseState);
	void mouseButton(VulkanWindow&, size_t button, VulkanWindow::ButtonState buttonState, const VulkanWindow::MouseState& mouseState);
	void mouseWheel(VulkanWindow& window, float wheelX, float wheelY, const VulkanWindow::MouseState& mouseState);
	void key(VulkanWindow& window, VulkanWindow::KeyState keyState, VulkanWindow::ScanCode scanCode, VulkanWindow::KeyCode key);

	// Vulkan instance must be destructed as the last Vulkan handle.
	// It is probably good idea to destroy it after the display connection.
	vk::Instance instance;

	// window needs to be destroyed after the swapchain
	// This is required especially by Wayland.
	VulkanWindow window;

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
	vk::SwapchainKHR swapchain;
	vector<vk::ImageView> swapchainImageViews;
	vector<vk::Framebuffer> framebuffers;
	vk::CommandPool commandPool;
	vk::CommandBuffer commandBuffer;
	vk::Semaphore imageAvailableSemaphore;
	vk::Semaphore renderFinishedSemaphore;
	vk::Fence renderFinishedFence;

};


/// Construct application object
App::App(int argc, char** argv)
{
}


App::~App()
{
	if(device) {

		// wait for device idle state
		// (to prevent errors during destruction of Vulkan resources)
		try {
			device.waitIdle();
		} catch(vk::Error& e) {
			cout << "Failed because of Vulkan exception: " << e.what() << endl;
		}

		// destroy handles
		// (the handles are destructed in certain (not arbitrary) order)
		device.destroy(renderFinishedFence);
		device.destroy(renderFinishedSemaphore);
		device.destroy(imageAvailableSemaphore);
		device.destroy(commandPool);
		for(auto f : framebuffers)  device.destroy(f);
		for(auto v : swapchainImageViews)  device.destroy(v);
		device.destroy(swapchain);
		device.destroy(renderPass);
		device.destroy();
	}

	window.destroy();
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
	vk::SurfaceKHR surface =
		window.create(instance, {1024, 768}, appName);

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
			if(pd.getSurfaceSupportKHR(i, surface)) {

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

	// give window Vulkan device used for rendering
	window.setDevice(device, physicalDevice);

	// print surface formats
	cout << "Surface formats:" << endl;
	vector<vk::SurfaceFormatKHR> availableSurfaceFormats = physicalDevice.getSurfaceFormatsKHR(surface);
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
}


/** Recreate swapchain and pipeline callback method.
 *  The method is usually called after the window resize and on the application start. */
void App::resize(VulkanWindow&, const vk::SurfaceCapabilitiesKHR& surfaceCapabilities,
                 vk::Extent2D newSurfaceExtent)
{
	// clear resources
	for(auto v : swapchainImageViews)  device.destroy(v);
	swapchainImageViews.clear();
	for(auto f : framebuffers)  device.destroy(f);
	framebuffers.clear();

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
				window.surface(),               // surface
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
				vk::PresentModeKHR::eFifo,  // presentMode
				VK_TRUE,  // clipped
				swapchain  // oldSwapchain
			)
		);
	device.destroy(swapchain);
	swapchain = newSwapchain.release();

	// swapchain images and image views
	vector<vk::Image> swapchainImages = device.getSwapchainImagesKHR(swapchain);
	swapchainImageViews.reserve(swapchainImages.size());
	for(vk::Image image : swapchainImages)
		swapchainImageViews.emplace_back(
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
	framebuffers.reserve(swapchainImages.size());
	for(size_t i=0, c=swapchainImages.size(); i<c; i++)
		framebuffers.emplace_back(
			device.createFramebuffer(
				vk::FramebufferCreateInfo(
					vk::FramebufferCreateFlags(),  // flags
					renderPass,  // renderPass
					1,  // attachmentCount
					&swapchainImageViews[i],  // pAttachments
					newSurfaceExtent.width,  // width
					newSurfaceExtent.height,  // height
					1  // layers
				)
			)
		);
}


void App::frame(VulkanWindow&)
{
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

	// acquire image
	uint32_t imageIndex;
	r =
		device.acquireNextImageKHR(
			swapchain,                // swapchain
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
			framebuffers[imageIndex],  // framebuffer
			vk::Rect2D(vk::Offset2D(0, 0), window.surfaceExtent()),  // renderArea
			1,  // clearValueCount
			&(const vk::ClearValue&)vk::ClearValue(  // pClearValues
				vk::ClearColorValue(array<float, 4>{0.0f, 0.5f, 1.0f, 1.f})
			)
		),
		vk::SubpassContents::eInline
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
				1, &swapchain, &imageIndex,  // swapchainCount + pSwapchains + pImageIndices
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
}


static void printModifiers(const VulkanWindow::MouseState& s)
{
	cout << ", modifiers: ";
	if(s.modifiers.none()) {
		cout << "none";
		return;
	}

	if(s.modifiers.test(VulkanWindow::Modifier::Ctrl)) {
		cout << "Ctrl";
		goto testForShift;
	}
	if(s.modifiers.test(VulkanWindow::Modifier::Shift)) {
		cout << "Shift";
		goto testForAlt;
	}
	if(s.modifiers.test(VulkanWindow::Modifier::Alt)) {
		cout << "Alt";
		goto testForMeta;
	}
	if(s.modifiers.test(VulkanWindow::Modifier::Meta))
		cout << "Meta";
	return;

testForShift:
	if(s.modifiers.test(VulkanWindow::Modifier::Shift))
		cout << "+Shift";
testForAlt:
	if(s.modifiers.test(VulkanWindow::Modifier::Alt))
		cout << "+Alt";
testForMeta:
	if(s.modifiers.test(VulkanWindow::Modifier::Meta))
		cout << "+Meta";
}


void App::mouseMove(VulkanWindow&, const VulkanWindow::MouseState& s)
{
	cout << "mouseMove " << s.posX << "," << s.posY;
	printModifiers(s);
	cout << endl;
}


void App::mouseButton(VulkanWindow&, size_t button, VulkanWindow::ButtonState buttonState, const VulkanWindow::MouseState& s)
{
	string d = (buttonState == VulkanWindow::ButtonState::Pressed) ? "down, " : "up,   ";
	cout << "mouseButton " << d << "button: " << button << ", buttonState: 0x" << hex << s.buttons.to_ulong() << dec;
	printModifiers(s);
	cout << endl;
}


void App::mouseWheel(VulkanWindow&, float wheelX, float wheelY, const VulkanWindow::MouseState& s)
{
	cout << "mouseWheel " << wheelX << "," << wheelY;
	printModifiers(s);
	cout << endl;
}


void App::key(VulkanWindow&, VulkanWindow::KeyState keyState, VulkanWindow::ScanCode scanCode, VulkanWindow::KeyCode key)
{
	if(keyState == VulkanWindow::KeyState::Pressed)
		cout << "key down, ";
	else
		cout << "key up,   ";

	cout << "scanCode: " << uint16_t(scanCode) << " (0x" << hex << uint16_t(scanCode) << dec;
	cout << "), keyCode: " << uint16_t(key) << " (0x" << hex << uint16_t(key) << dec;
	string s;
	s.reserve(4);  // this shall allocate at least 4 chars and one null byte, e.g. at least 5 bytes
	s.assign(VulkanWindow::toCharArray(key).data());
	cout << "), character: " << s << endl;
}


int main(int argc, char* argv[])
{
	// set console code page to utf-8 to print non-ASCII characters correctly
#ifdef _WIN32
	if(!SetConsoleOutputCP(CP_UTF8))
		cout << "Failed to set console code page to utf-8." << endl;
#endif

	// catch exceptions
	// (vulkan.hpp functions throw if they fail)
	try {

		App app(argc, argv);
		app.init();
		app.window.setResizeCallback(
			bind(
				&App::resize,
				&app,
				placeholders::_1,
				placeholders::_2,
				placeholders::_3
			)
		);
		app.window.setFrameCallback(
			bind(&App::frame, &app, placeholders::_1)
		);
		app.window.setMouseMoveCallback(bind(&App::mouseMove, &app, placeholders::_1, placeholders::_2));
		app.window.setMouseButtonCallback(bind(&App::mouseButton, &app, placeholders::_1, placeholders::_2, placeholders::_3, placeholders::_4));
		app.window.setMouseWheelCallback(bind(&App::mouseWheel, &app, placeholders::_1, placeholders::_2, placeholders::_3, placeholders::_4));
		app.window.setKeyCallback(bind(&App::key, &app, placeholders::_1, placeholders::_2, placeholders::_3, placeholders::_4));
		app.window.show();
		app.window.mainLoop();

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
