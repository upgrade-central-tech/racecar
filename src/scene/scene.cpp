#include "scene.hpp"

#include <SDL3/SDL.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <filesystem>

namespace racecar::scene {

bool loadBinaryJson(std::string filepath, Scene& scene) {
  std::filesystem::path path(filepath);
  
  if (!std::filesystem::exists(path)) {
    SDL_Log("[Scene] File does not exist");
    return false;
  }
  
  auto ext = path.extension().string();
  if (ext != ".gltf" && ext != ".glb") {
    SDL_Log("[Scene] Invalid file extension loaded");
    return false;
  }

  tinygltf::Model model;
  tinygltf::TinyGLTF loader;
  std::string err;
  std::string warn;

  bool hasLoadedSuccessfully = false;
  // Binary files
  if (ext == ".glb") {
    hasLoadedSuccessfully = loader.LoadBinaryFromFile(&model, &err, &warn, filepath);
  } else {  // ASCII files
    hasLoadedSuccessfully = loader.LoadASCIIFromFile(&model, &err, &warn, filepath);
  }

  // Check for errors and warnings
  if (!warn.empty()) {
    SDL_Log("[Scene] GLTF load WARNING: %s", warn.c_str());
  }

  if (!err.empty()) {
    SDL_Log("[Scene] GLTF load ERROR: %s!", err.c_str());
  }

  if (!hasLoadedSuccessfully) {
    SDL_Log("[Scene] GLTF parsing error!");
  }

  // TODO @terskayl: Add Material Loading

  // Used for pairing children and parents in the scene graph
  std::vector<std::vector<int>> childrenLists;
  // Load Nodes
  for (tinygltf::Node& loadedNode : model.nodes) {
    std::unique_ptr<Node> newNode;

    // Get node transform
    if (loadedNode.matrix.size()) {
      std::vector<double> mat = loadedNode.matrix;
      newNode->transform =
          glm::mat4(mat[0], mat[1], mat[2], mat[3], mat[4], mat[5], mat[6], mat[7], mat[8], mat[9],
                    mat[10], mat[11], mat[12], mat[13], mat[14], mat[15]);
    } else {
      // Attempt to recover from individual transforms

      glm::vec3 translation = glm::vec3(0.f);
      glm::quat rotation = glm::quat();
      glm::vec3 scale = glm::vec3(1.f);

      if (loadedNode.translation.size()) {
        translation = glm::vec3(loadedNode.translation[0], loadedNode.translation[1],
                                loadedNode.translation[2]);
      }
      if (loadedNode.rotation.size()) {
        // GLM's quat expects WXYZ order, but GLTF uses XYZW order
        rotation = glm::quat(
            static_cast<float>(loadedNode.rotation[3]), static_cast<float>(loadedNode.rotation[0]),
            static_cast<float>(loadedNode.rotation[1]), static_cast<float>(loadedNode.rotation[2]));
      }
      if (loadedNode.scale.size()) {
        scale = glm::vec3(loadedNode.scale[0], loadedNode.scale[1], loadedNode.scale[2]);
      }
      // Combine translate, rotation, and scale
      // Build transform: translate * rotate * scale
      newNode->transform = glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation) *
                           glm::scale(glm::mat4(1.0f), scale);
    }

    newNode->inv_transform = glm::inverse(newNode->transform);
    newNode->inv_transpose = glm::inverseTranspose(newNode->transform);

    // Load the mesh or camera of the node. Potential optimization: Right now we are creating a new
    // mesh on every node. In the case where meshes are instanced, we will created Mesh structs with
    // the same data. We could use a lookup set to resolve this in the future.
    if (loadedNode.mesh != -1) {
      newNode->data = std::make_unique<Mesh>();
      tinygltf::Mesh loadedMesh = model.meshes[static_cast<size_t>(loadedNode.mesh)];

      // Load primitives
      for (tinygltf::Primitive& loadedPrim : loadedMesh.primitives) {
        Primitive newPrim;
        newPrim.material_id = loadedPrim.material;
        if (loadedPrim.mode != 4) {
          SDL_Log(
              "[Scene] GLTF Loading: Mesh detected that uses a mode other than TRIANGLES. This is "
              "currently unsupported.");
        }

        int posBufferId = -1;
        int norBufferId = -1;
        int uvBufferId = -1;
        int indBufferId = -1;

        if (loadedPrim.attributes.count("POSITION") > 0) {
          int accessorId = loadedPrim.attributes["POSITION"];
          tinygltf::Accessor accessor = model.accessors[static_cast<size_t>(accessorId)];
          int bufferViewId = accessor.bufferView;
          if (accessor.componentType != 5126) {
            SDL_Log(
                "[Scene] GLTF Loading: Expected all vertex attributes to be float - unsupported "
                "component type detected");
          }
          if (accessor.componentType != 3) {
            SDL_Log(
                "[Scene] GLTF Loading: Expected all positions to be in vec3 - unsupported type "
                "detected");
          }

          tinygltf::BufferView bufferView = model.bufferViews[static_cast<size_t>(bufferViewId)];
          newPrim.pos_offset = static_cast<int>(bufferView.byteOffset);
          posBufferId = bufferView.buffer;
        }

        if (loadedPrim.attributes.count("NORMAL") > 0) {
          int accessorId = loadedPrim.attributes["NORMAL"];
          tinygltf::Accessor accessor = model.accessors[static_cast<size_t>(accessorId)];
          int bufferViewId = accessor.bufferView;
          if (accessor.componentType != 5126) {
            SDL_Log(
                "[Scene] GLTF Loading: Expected all vertex attributes to be float - unsupported "
                "component type detected");
          }
          if (accessor.componentType != 3) {
            SDL_Log(
                "[Scene] GLTF Loading: Expected all normals to be in vec3 - unsupported type "
                "detected");
          }

          tinygltf::BufferView bufferView = model.bufferViews[static_cast<size_t>(bufferViewId)];
          newPrim.nor_offset = static_cast<int>(bufferView.byteOffset);
          norBufferId = bufferView.buffer;
        }

        // Currently only accepting one uv coordinate per primative.
        if (loadedPrim.attributes.count("TEX_COORD0") > 0) {
          int accessorId = loadedPrim.attributes["TEX_COORD0"];
          tinygltf::Accessor accessor = model.accessors[static_cast<size_t>(accessorId)];
          int bufferViewId = accessor.bufferView;
          if (accessor.componentType != 5126) {
            SDL_Log(
                "[Scene] GLTF Loading: Expected all vertex attributes to be float - unsupported "
                "component type detected");
          }
          if (accessor.componentType != 2) {
            SDL_Log(
                "[Scene] GLTF Loading: Expected all uvs to be in vec2 - unsupported type "
                "detected");
          }

          tinygltf::BufferView bufferView = model.bufferViews[static_cast<size_t>(bufferViewId)];
          newPrim.uv_offset = static_cast<int>(bufferView.byteOffset);
          uvBufferId = bufferView.buffer;
        }

        if (loadedPrim.indices != -1) {
          int accessorId = loadedPrim.indices;
          tinygltf::Accessor accessor = model.accessors[static_cast<size_t>(accessorId)];
          int bufferViewId = accessor.bufferView;
          if (accessor.componentType != 5123 && accessor.componentType != 5125) {
            SDL_Log(
                "[Scene] GLTF Loading: Expected all indices to either be a unsigned short or an "
                "unsigned int - unsupported component type detected");
          }
          // Indices uses unsigned ints instead of unsigned shorts
          if (accessor.componentType == 5124) {
            newPrim.sixteen_bit_indices = false;
          }
          if (accessor.type != 65) {
            SDL_Log(
                "[Scene] GLTF Loading: Expected all indices to be scalars - unsupported type "
                "detected");
          }

          size_t count = accessor.count;
          tinygltf::BufferView bufferView = model.bufferViews[static_cast<size_t>(bufferViewId)];
          newPrim.ind_offset = static_cast<int>(bufferView.byteOffset);
          newPrim.ind_count = count;
          indBufferId = bufferView.buffer;
        }

        // Assume position data always exists
        if ((posBufferId != norBufferId || norBufferId == -1) &&
            (posBufferId != uvBufferId || uvBufferId == -1) &&
            (posBufferId != indBufferId || indBufferId == -1)) {
          SDL_Log("[Scene] GLTF Loading: Prim with attributes in multiple buffers detected!");
        }
        newPrim.buffer_id = static_cast<size_t>(posBufferId);

        std::get<std::unique_ptr<Mesh>>(newNode->data)->primitives.push_back(newPrim);
      }
    } else {  // Presumably the node is a camera
      std::unique_ptr<Camera> camera = std::make_unique<Camera>();
      tinygltf::Camera loadedCamera = model.cameras[static_cast<size_t>(loadedNode.camera)];
      camera->aspect_ratio = loadedCamera.perspective.aspectRatio;
      camera->near_plane = loadedCamera.perspective.znear;
      camera->far_plane = loadedCamera.perspective.zfar;
      camera->fovy = loadedCamera.perspective.yfov;
      camera->view_mat = newNode->transform;
      camera->forward =
          glm::normalize(glm::vec3(newNode->transform * glm::vec4(0.f, 0.f, -1.f, 0.f)));
      camera->up = glm::normalize(glm::vec3(newNode->transform * glm::vec4(0.f, 1.f, 0.f, 0.f)));
      camera->right = glm::normalize(glm::cross(camera->forward, camera->up));

      // The main camera will be the first camera loaded.
      if (scene.main_camera == nullptr) {
        scene.main_camera = camera.get();
      }
      newNode->data = std::move(camera);
    }
    scene.nodes.push_back(std::move(newNode));
  }

  // Assumes scene.nodes order is the same as the indices in the GLTF. Will probably break if this
  // function is multithreaded.
  for (size_t i = 0; i < scene.nodes.size(); i++) {
    std::unique_ptr<Node>& node = scene.nodes[i];
    std::vector<int> children = childrenLists[i];
    for (int child : children) {
      std::unique_ptr<Node>& childNode = scene.nodes[static_cast<size_t>(child)];
      childNode->parent = node.get();
      node->children.push_back(childNode.get());
    }
  }

  return true;
}

}  // namespace racecar::scene
