#include "cl2.hpp"
#include <iostream>

int main()
{
	std::vector<cl::Platform> platforms;
	std::string               str;

	cl::Platform::get(&platforms);
	platforms[0].getInfo(CL_PLATFORM_VERSION, &str);
	std::cout <<  str << std::endl;
	
	return 0;
}