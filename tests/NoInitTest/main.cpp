#include "VulkanWindow.h"
#include <vulkan/vulkan.hpp>
#include <iostream>

using namespace std;


// constants
constexpr const char* appName = "NoInitTest";


int main(int argc, char* argv[])
{
	// catch exceptions
	// (vulkan.hpp functions throw if they fail)
	try {

		cout << "NoInitTest:" << endl;
		cout << "Testing constructor of VulkanWindow without calling\n"
		        "   VulkanWindow::init()..." << flush;
		VulkanWindow window;
		cout << " Done." << endl;

		cout << "Testing VulkanWindow::destroy() on VulkanWindow that was not initialized\n"
		        "   by calling VulkanWindow::create()..." << flush;
		window.destroy();
		cout << " Done." << endl;

		cout << "Testing VulkanWindow destructor..." << flush;

	// catch exceptions
	} catch(vk::Error& e) {
		cout << "Failed because of Vulkan exception: " << e.what() << endl;
	} catch(exception& e) {
		cout << "Failed because of exception: " << e.what() << endl;
	} catch(...) {
		cout << "Failed because of unspecified exception." << endl;
	}

	cout << " Done." << endl;

	cout << "Testing VulkanWindow::finalize() without calling\n"
	        "   VulkanWindow::init()..." << flush;
	VulkanWindow::finalize();
	cout << " Done." << endl;

	cout << "All tests passed." << endl;
	return 0;
}
