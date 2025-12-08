#include "VulkanWindow.h"
#include <vulkan/vulkan.hpp>
#include <algorithm>
#include <chrono>
#include <iostream>

using namespace std;


// constants
constexpr const char* appName = "ColorSpace";


// shader code in SPIR-V binary
static const uint32_t vsSpirv[] = {
#include "shader.vert.spv"
};
static const uint32_t fsSpirv[] = {
#include "shader.frag.spv"
};


// global application data
class App {
public:

	App(int argc, char* argv[]);
	~App();

	void init();
	void resize(VulkanWindow& window, const vk::SurfaceCapabilitiesKHR& surfaceCapabilities, vk::Extent2D newSurfaceExtent);
	void frame(VulkanWindow& window);

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
	vector<vk::Semaphore> renderingFinishedSemaphores;
	vk::Semaphore imageAvailableSemaphore;
	vk::Fence renderFinishedFence;
	vk::CommandPool commandPool;
	vk::CommandBuffer commandBuffer;
	vk::ShaderModule vsModule;
	vk::ShaderModule fsModule;
	vk::PipelineLayout pipelineLayout;
	vk::Pipeline pipeline;

};


// to be thrown to gracefully terminate the application
// (std::exit() does not run destructors of objects on the stack)
class ExitWithMessage {
protected:
	int _exitCode;
	std::string _what;
public:
	ExitWithMessage(int exitCode, const std::string& msg) : _exitCode(exitCode), _what(msg) {}
	ExitWithMessage(int exitCode, const char* msg) : _exitCode(exitCode), _what(msg) {}
	const char* what() const noexcept  { return _what.c_str(); }
	int exitCode() const noexcept  { return _exitCode; }
};


/// Construct application object
App::App(int argc, char** argv)
{
}


App::~App()
{
	if(device)
	{
		// wait for device idle state
		// (to prevent errors during destruction of Vulkan resources);
		// we ignore any returned error codes here
		// because the device might be in the lost state already, etc.
		vkDeviceWaitIdle(device);

		// destroy handles
		// (the handles are destructed in certain (not arbitrary) order)
		device.destroy(pipeline);
		device.destroy(pipelineLayout);
		device.destroy(fsModule);
		device.destroy(vsModule);
		device.destroy(commandPool);
		device.destroy(renderFinishedFence);
		device.destroy(imageAvailableSemaphore);
		for(auto s : renderingFinishedSemaphores)  device.destroy(s);
		for(auto f : framebuffers)  device.destroy(f);
		for(auto v : swapchainImageViews)  device.destroy(v);
		device.destroy(swapchain);
		device.destroy(renderPass);
		device.destroy();
	}

	// destroy window
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


static int getScore(vk::SurfaceFormatKHR surfaceFormat)
{
	auto getScoreForPassThroughFormat =
		[](vk::Format f, int baseScoreForGoodPrecision, int baseScoreForBadPrecision) -> int
		{
			switch(f) {
				case vk::Format::eR16G16B16A16Unorm:  return baseScoreForGoodPrecision + 700;
				case vk::Format::eR16G16B16Unorm:     return baseScoreForGoodPrecision + 600;
				case vk::Format::eR16G16B16A16Sfloat: return baseScoreForGoodPrecision + 700;
				case vk::Format::eR16G16B16Sfloat:    return baseScoreForGoodPrecision + 600;
				case vk::Format::eB10G11R11UfloatPack32: return baseScoreForGoodPrecision + 500;
				case vk::Format::eE5B9G9R9UfloatPack32:  return baseScoreForGoodPrecision + 500;
				case vk::Format::eA2R10G10B10UnormPack32: return baseScoreForGoodPrecision + 400;
				case vk::Format::eA2B10G10R10UnormPack32: return baseScoreForGoodPrecision + 400;
				case vk::Format::eR8G8B8A8Unorm:       return baseScoreForBadPrecision + 300;
				case vk::Format::eR8G8B8Unorm:         return baseScoreForBadPrecision + 200;
				case vk::Format::eB8G8R8A8Unorm:       return baseScoreForBadPrecision + 300;
				case vk::Format::eB8G8R8Unorm:         return baseScoreForBadPrecision + 200;
				case vk::Format::eA8B8G8R8UnormPack32: return baseScoreForBadPrecision + 100;
				default: return baseScoreForBadPrecision;  // lets expect we covered all good formats above, so return bad precision score
			}
		};
	auto getScoreForNonlinearFormatSimilarToSrgb =
		[](vk::Format f, int baseScore) -> int
		{
			switch(f) {
				case vk::Format::eR8G8B8A8Unorm:       return baseScore + 800;
				case vk::Format::eR8G8B8Unorm:         return baseScore + 600;
				case vk::Format::eB8G8R8A8Unorm:       return baseScore + 800;
				case vk::Format::eB8G8R8Unorm:         return baseScore + 600;
				case vk::Format::eA8B8G8R8UnormPack32: return baseScore + 700;
				case vk::Format::eA2R10G10B10UnormPack32: return baseScore + 500;
				case vk::Format::eA2B10G10R10UnormPack32: return baseScore + 500;
				case vk::Format::eR16G16B16A16Unorm:  return baseScore + 400;
				case vk::Format::eR16G16B16Unorm:     return baseScore + 400;
				case vk::Format::eR16G16B16A16Sfloat: return baseScore + 400;
				case vk::Format::eR16G16B16Sfloat:    return baseScore + 400;
				case vk::Format::eB10G11R11UfloatPack32: return baseScore + 300;
				case vk::Format::eE5B9G9R9UfloatPack32:  return baseScore + 300;
				case vk::Format::eR8G8B8A8Srgb:       return baseScore + 200;
				case vk::Format::eR8G8B8Srgb:         return baseScore + 100;
				case vk::Format::eB8G8R8A8Srgb:       return baseScore + 200;
				case vk::Format::eB8G8R8Srgb:         return baseScore + 100;
				case vk::Format::eA8B8G8R8SrgbPack32: return baseScore + 200;
				default: return baseScore;
			}
		};

	switch(surfaceFormat.colorSpace) {

		// sRGB: BT709 primaries (~35% of visible color spectrum),
		// D65 white point, sRGB transfer function
		case vk::ColorSpaceKHR::eSrgbNonlinear: {
			const int scoreBase = 10000;
			switch(surfaceFormat.format) {
			case vk::Format::eR8G8B8A8Srgb:       return scoreBase + 600;
			case vk::Format::eR8G8B8Srgb:         return scoreBase + 400;
			case vk::Format::eB8G8R8A8Srgb:       return scoreBase + 600;
			case vk::Format::eB8G8R8Srgb:         return scoreBase + 400;
			case vk::Format::eA8B8G8R8SrgbPack32: return scoreBase + 500;
			case vk::Format::eA2R10G10B10UnormPack32: return scoreBase + 300;
			case vk::Format::eA2B10G10R10UnormPack32: return scoreBase + 300;
			case vk::Format::eR16G16B16A16Unorm:  return scoreBase + 200;
			case vk::Format::eR16G16B16Unorm:     return scoreBase + 200;
			case vk::Format::eR16G16B16A16Sfloat: return scoreBase + 200;
			case vk::Format::eR16G16B16Sfloat:    return scoreBase + 200;
			case vk::Format::eB10G11R11UfloatPack32: return scoreBase + 100;
			case vk::Format::eE5B9G9R9UfloatPack32:  return scoreBase + 100;
			default: return scoreBase;
			}
		}

		// BT709: BT709 primaries (~35% of visible color spectrum),
		// D65 white point, BT709 transfer function
		case vk::ColorSpaceKHR::eBt709LinearEXT:
			return getScoreForPassThroughFormat(surfaceFormat.format, 22000, 20000);
		case vk::ColorSpaceKHR::eBt709NonlinearEXT:
			return getScoreForNonlinearFormatSimilarToSrgb(surfaceFormat.format, 21000);

		// Display-P3: P3 primaries (~45% of visible color spectrum),
		// D65 white point, Display-P3 transfer function (very similar to sRGB);
		// Display-P3 is distinct from DCI-P3 by using different white point
		// and different transfer function
		case vk::ColorSpaceKHR::eDisplayP3LinearEXT:
			return getScoreForPassThroughFormat(surfaceFormat.format, 32000, 30000);
		case vk::ColorSpaceKHR::eDisplayP3NonlinearEXT:
			return getScoreForNonlinearFormatSimilarToSrgb(surfaceFormat.format, 31000);

		// Adobe RGB: Adobe RGB primaries (~50% of visible color spectrum),
		// D65 white point, Adobe RGB (1998) transfer function
		case vk::ColorSpaceKHR::eAdobergbLinearEXT:
			return getScoreForPassThroughFormat(surfaceFormat.format, 42000, 40000);
		case vk::ColorSpaceKHR::eAdobergbNonlinearEXT:
			return getScoreForPassThroughFormat(surfaceFormat.format, 41000, 41000);

		// extended sRGB: BT709 primaries while allowing values <0 and >1
		// (100% of visible color spectrum),
		// D65 while point, scRGB transfer function
		case vk::ColorSpaceKHR::eExtendedSrgbLinearEXT: {
			const int baseScore = 52000;
			switch(surfaceFormat.format) {
			case vk::Format::eR16G16B16A16Sfloat: return baseScore + 300;
			case vk::Format::eR16G16B16Sfloat:    return baseScore + 300;
			case vk::Format::eR32G32B32A32Sfloat: return baseScore + 200;
			case vk::Format::eR32G32B32Sfloat:    return baseScore + 200;
			case vk::Format::eR64G64B64A64Sfloat: return baseScore + 100;
			case vk::Format::eR64G64B64Sfloat:    return baseScore + 100;
			default: return baseScore;
			}
		}
		case vk::ColorSpaceKHR::eExtendedSrgbNonlinearEXT: {
			const int baseScore = 50000;
			switch(surfaceFormat.format) {
			case vk::Format::eR16G16B16A16Sfloat: return baseScore + 300;
			case vk::Format::eR16G16B16Sfloat:    return baseScore + 300;
			case vk::Format::eR32G32B32A32Sfloat: return baseScore + 200;
			case vk::Format::eR32G32B32Sfloat:    return baseScore + 200;
			case vk::Format::eR64G64B64A64Sfloat: return baseScore + 100;
			case vk::Format::eR64G64B64Sfloat:    return baseScore + 100;
			default: return baseScore;
			}
		}

		// DCI-P3: DCI-P3 color space using imaginary colors to encompass
		// all visible colors (100% of visible color spectrum),
		// white point in 1/3,1/3, transfer function DCI P3
		//
		// note: eDciP3LinearEXT is not DCI-P3, but the enum value maps to Display-P3 linear,
		// because it was misnamed in old version of VK_EXT_swapchain_colorspace;
		// no linear DCI-P3 exists in VK_EXT_swapchain_colorspace;
		// see docs for more info
		case vk::ColorSpaceKHR::eDciP3NonlinearEXT:
			return getScoreForPassThroughFormat(surfaceFormat.format, 62000, 60000);

		// BT2020: BT2020 primaries (~75% of visible color spectrum),
		// D65 white point, linear transfer function
		case vk::ColorSpaceKHR::eBt2020LinearEXT:
			return getScoreForPassThroughFormat(surfaceFormat.format, 73000, 70000);

		// HDR10: BT2020 primaries (~75% of visible color spectrum),
		// D65 white point, Hybrid Log Gamma (HLG)
		case vk::ColorSpaceKHR::eHdr10HlgEXT:
			return getScoreForPassThroughFormat(surfaceFormat.format, 74000, 71000);

		// HDR10: BT2020 primaries (~75% of visible color spectrum),
		// D65 white point, SMPTE ST2084 Perceptual Quantizer
		case vk::ColorSpaceKHR::eHdr10St2084EXT:
			return getScoreForPassThroughFormat(surfaceFormat.format, 75000, 72000);

		// Display Native is provided by VK_AMD_display_native_hdr extension
		// built for FreeSync2 standard
		case vk::ColorSpaceKHR::eDisplayNativeAMD:
			return getScoreForPassThroughFormat(surfaceFormat.format, 0, 0);

		// Pass Through might be useful when colors are managed outside of
		// Vulkan, for instance by Wayland wp_color_management_surface_v1 object
		case vk::ColorSpaceKHR::ePassThroughEXT:
			return getScoreForPassThroughFormat(surfaceFormat.format, 0, 0);

		// Dolby Vision: legacy according to Vulkan spec;
		// so it is disabled to not produce warnings about deprecation
		// case vk::ColorSpaceKHR::eDolbyvisionEXT:
		//    return getScoreForPassThroughFormat(surfaceFormat.format, 0, 0);

		// unknown future color spaces
		default: return getScoreForPassThroughFormat(surfaceFormat.format, 0, 0);
	}
}


void App::init()
{
	// init VulkanWindow
	VulkanWindow::init();

	// devices without VK_KHR_surface
	cout << "Color space related instance extensions support:\n"
	        "   VK_KHR_surface:               ";
	auto extensionList = vk::enumerateInstanceExtensionProperties();
	for(vk::ExtensionProperties& e : extensionList)
		if(strcmp(e.extensionName, "VK_KHR_surface") == 0) {
			cout << "supported" << endl;
			goto surfaceSupported;
		}
	cout << "not supported" << endl;
	surfaceSupported:;
	cout << "   VK_EXT_swapchain_colorspace:  ";
	for(vk::ExtensionProperties& e : extensionList)
		if(strcmp(e.extensionName, "VK_EXT_swapchain_colorspace") == 0) {
			cout << "supported" << endl;
			goto colorSpaceSupported;
		}
	cout << "not supported" << endl;
	colorSpaceSupported:;
	cout << endl;

	// Vulkan instance
	vector<const char*> requiredExtensions = VulkanWindow::requiredExtensions();
	requiredExtensions.push_back("VK_EXT_swapchain_colorspace");
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
				{},  // no layers
				requiredExtensions,  // enabled extensions
			}
		);

	// create surface
	vk::SurfaceKHR surface =
		window.create(instance, {1024, 768}, appName);

	// get devices:
	// - compatibleDevices - devices that can present in the created window
	// - incompatibleDevices - devices that do not support presentation into the created window
	vector<vk::PhysicalDevice> deviceList = instance.enumeratePhysicalDevices();
	vector<tuple<vk::PhysicalDevice, uint32_t, uint32_t, vk::PhysicalDeviceProperties>> compatibleDevices;
	vector<tuple<vk::PhysicalDevice, vk::PhysicalDeviceProperties>> incompatibleDevices;
	for(vk::PhysicalDevice pd : deviceList) {

		// devices without VK_KHR_swapchain
		auto extensionList = pd.enumerateDeviceExtensionProperties();
		for(vk::ExtensionProperties& e : extensionList)
			if(strcmp(e.extensionName, "VK_KHR_swapchain") == 0)
				goto swapchainSupported;
		incompatibleDevices.emplace_back(pd, pd.getProperties());
		continue;
		swapchainSupported:;

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
		else
			// no presentation possible
			incompatibleDevices.emplace_back(pd, pd.getProperties());
		nextDevice:;
	}

	// color spaces print
	auto printColorSpaces =
		[](const vector<vk::SurfaceFormatKHR>& surfaceFormatList)
		{
			cout << "         Color spaces:" << endl;
			for(vk::SurfaceFormatKHR sf : surfaceFormatList)
				cout << "            " << vk::to_string(sf.colorSpace) << ", format: " << vk::to_string(sf.format) << endl;
		};

	// function to select best surface format
	decltype(compatibleDevices)::value_type* bestDevice;
	vk::SurfaceFormatKHR bestSurfaceFormat;
	int bestScore = 0;
	auto evaluateBestSurfaceFormat =
		[&](decltype(compatibleDevices)::value_type& t, const vector<vk::SurfaceFormatKHR>& surfaceFormatList) {

			// get best surface format
			constexpr const array deviceTypeScoreTable = {
				20, // vk::PhysicalDeviceType::eOther         - lowest score
				50, // vk::PhysicalDeviceType::eIntegratedGpu - high score
				60, // vk::PhysicalDeviceType::eDiscreteGpu   - highest score
				40, // vk::PhysicalDeviceType::eVirtualGpu    - normal score
				30, // vk::PhysicalDeviceType::eCpu           - low score
				10, // unknown vk::PhysicalDeviceType
			};
			int deviceScore = deviceTypeScoreTable[clamp(int(get<3>(t).deviceType), 0, int(deviceTypeScoreTable.size())-1)];
			int score;
			if(get<1>(t) == get<2>(t))
				deviceScore++;

			for(vk::SurfaceFormatKHR sf : surfaceFormatList) {
				score = getScore(sf) + deviceScore;
				if(score > bestScore) {
					bestDevice = &t;
					bestSurfaceFormat = sf;
					bestScore = score;
				}
			}

		};

	// print devices, their color spaces and surface formats,
	// and select best surface format
	cout << "Device list:" << endl;
	size_t i = 1;
	for(auto& t : compatibleDevices) {
		cout << "   " << i << ": " << get<3>(t).deviceName << endl;
		vk::PhysicalDevice pd = get<0>(t);
		vector<vk::SurfaceFormatKHR> surfaceFormatList = pd.getSurfaceFormatsKHR(surface);
		printColorSpaces(surfaceFormatList);
		evaluateBestSurfaceFormat(t, surfaceFormatList);
		i++;
	}
	for(auto& t : incompatibleDevices) {
		cout << "   incompatible: " << get<1>(t).deviceName << endl;
		vk::PhysicalDevice pd = get<0>(t);
		vector<vk::SurfaceFormatKHR> surfaceFormatList = pd.getSurfaceFormatsKHR(surface);
		printColorSpaces(surfaceFormatList);
		i++;
	}
	cout << endl;
	if(compatibleDevices.empty() && incompatibleDevices.empty())
		throw ExitWithMessage(0, "No compatible devices.");

	// print info about window we are going to create
	// (based on best surface format and the device that provided the best surface format)
	cout << "Creating window using\n"
	        "   Device:       " << get<3>(*bestDevice).deviceName << "\n"
	        "   Color space:  " << vk::to_string(bestSurfaceFormat.colorSpace) << "\n"
	        "   Format:       " << vk::to_string(bestSurfaceFormat.format) << "\n" << endl;

	// set app variables
	physicalDevice = get<0>(*bestDevice);
	graphicsQueueFamily = get<1>(*bestDevice);
	presentationQueueFamily = get<2>(*bestDevice);
	surfaceFormat = bestSurfaceFormat;

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

	// provide window with Vulkan device
	window.setDevice(device, physicalDevice);

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

	// rendering semaphore and fence
	imageAvailableSemaphore =
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
void App::resize(VulkanWindow&, const vk::SurfaceCapabilitiesKHR& surfaceCapabilities,
                 vk::Extent2D newSurfaceExtent)
{
	// clear resources
	for(auto v : swapchainImageViews)  device.destroy(v);
	swapchainImageViews.clear();
	for(auto f : framebuffers)  device.destroy(f);
	framebuffers.clear();
	device.destroy(pipeline);
	pipeline = nullptr;

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

	// rendering finished semaphores
	if(renderingFinishedSemaphores.size() != swapchainImages.size())
	{
		for(auto s : renderingFinishedSemaphores)  device.destroy(s);
		renderingFinishedSemaphores.clear();
		renderingFinishedSemaphores.reserve(swapchainImages.size());
		vk::SemaphoreCreateInfo semaphoreCreateInfo{
			vk::SemaphoreCreateFlags()  // flags
		};
		for(size_t i=0,c=swapchainImages.size(); i<c; i++)
			renderingFinishedSemaphores.emplace_back(
				device.createSemaphore(semaphoreCreateInfo));
	}

	// pipeline
	pipeline =
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


void App::frame(VulkanWindow&)
{
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
				vk::ClearColorValue(array<float, 4>{0.0f, 0.0f, 0.0f, 1.f})
			)
		),
		vk::SubpassContents::eInline
	);

	// rendering commands
	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);  // bind pipeline
	commandBuffer.draw(  // draw single triangle
		3,  // vertexCount
		1,  // instanceCount
		0,  // firstVertex
		0   // firstInstance
	);

	// end render pass and command buffer
	commandBuffer.endRenderPass();
	commandBuffer.end();

	// submit frame
	vk::Semaphore renderingFinishedSemaphore = renderingFinishedSemaphores[imageIndex];
	graphicsQueue.submit(
		vk::ArrayProxy<const vk::SubmitInfo>(
			1,
			&(const vk::SubmitInfo&)vk::SubmitInfo(
				1, &imageAvailableSemaphore,  // waitSemaphoreCount + pWaitSemaphores +
				&(const vk::PipelineStageFlags&)vk::PipelineStageFlags(  // pWaitDstStageMask
					vk::PipelineStageFlagBits::eColorAttachmentOutput),
				1, &commandBuffer,  // commandBufferCount + pCommandBuffers
				1, &renderingFinishedSemaphore  // signalSemaphoreCount + pSignalSemaphores
			)
		),
		renderFinishedFence  // fence
	);

	// present
	r =
		presentationQueue.presentKHR(
			&(const vk::PresentInfoKHR&)vk::PresentInfoKHR(
				1, &renderingFinishedSemaphore,  // waitSemaphoreCount + pWaitSemaphores
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


int main(int argc, char* argv[])
{
	// catch exceptions
	// (vulkan.hpp functions throw if they fail)
	int exitCode = 0;
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
		app.window.show();
		app.window.mainLoop();

	// catch exceptions
	} catch(vk::Error& e) {
		cout << "Failed because of Vulkan exception: " << e.what() << endl;
		exitCode = 9;
	} catch(ExitWithMessage &e) {
		cout << e.what() << endl;
		exitCode = e.exitCode();
	} catch(exception& e) {
		cout << "Failed because of exception: " << e.what() << endl;
		exitCode = 9;
	} catch(...) {
		cout << "Failed because of unspecified exception." << endl;
		exitCode = 9;
	}

	VulkanWindow::finalize();
	return exitCode;
}
