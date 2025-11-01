#include <iostream>

#include <glm/vec3.hpp>

int main() {
  glm::vec3 test = glm::vec3(0.f, 2.f, 0.f);
  std::cout << "Hello, from racecar! test.y: " << test.y;

  return 0;
}
