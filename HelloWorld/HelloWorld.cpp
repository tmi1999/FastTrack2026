#include <iostream>

int main() {
    #ifdef _WIN32
        std::cout << "Hello World! Windows" << std::endl;
    #elif __linux__
        std::cout << "Hello World! Linux" << std::endl;
    #else
        std::cout << "Hello World! Other" << std::endl;
    #endif
    return 0;
}