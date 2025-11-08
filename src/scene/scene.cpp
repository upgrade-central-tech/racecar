#include "scene.hpp"

#include <SDL3/SDL.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <filesystem>

namespace racecar::scene {

bool load_binary_json(std::string filepath, Scene& scene) {
  std::filesystem::path path(filepath);

  if (!std::filesystem::exists(path)) {
    SDL_Log("[Scene] File does not exist");
    return false;
  }

  std::string ext = path.extension().string();
  if (ext != ".gltf" && ext != ".glb") {
    SDL_Log("[Scene] Invalid file extension loaded");
    return false;
  }

  tinygltf::Model model;
  tinygltf::TinyGLTF loader;
  std::string err;
  std::string warn;

  bool has_loaded_successfully = false;
  // Binary files
  if (ext == ".glb") {
    has_loaded_successfully = loader.LoadBinaryFromFile(&model, &err, &warn, filepath);
  } else {  // ASCII files
    has_loaded_successfully = loader.LoadASCIIFromFile(&model, &err, &warn, filepath);
  }

  // Check for errors and warnings
  if (!warn.empty()) {
    SDL_Log("[Scene] GLTF load WARNING: %s", warn.c_str());
  }

  if (!err.empty()) {
    SDL_Log("[Scene] GLTF load ERROR: %s!", err.c_str());
  }

  if (!has_loaded_successfully) {
    SDL_Log("[Scene] GLTF parsing error!");
  }

  // TODO @terskayl: Add Material Loading

  // Used for pairing children and parents in the scene graph
  std::vector<std::vector<int>> children_lists;
  // Load Nodes
  for (tinygltf::Node& loaded_node : model.nodes) {
    std::unique_ptr<Node> new_node;

    // Get node transform
    if (loaded_node.matrix.size()) {
      std::vector<double> mat = loaded_node.matrix;
      new_node->transform =
          glm::mat4(mat[0], mat[1], mat[2], mat[3], mat[4], mat[5], mat[6], mat[7], mat[8], mat[9],
                    mat[10], mat[11], mat[12], mat[13], mat[14], mat[15]);
    } else {
      // Attempt to recover from individual transforms

      glm::vec3 translation = glm::vec3(0.f);
      glm::quat rotation = glm::quat();
      glm::vec3 scale = glm::vec3(1.f);

      if (loaded_node.translation.size()) {
        translation = glm::vec3(loaded_node.translation[0], loaded_node.translation[1],
                                loaded_node.translation[2]);
      }
      if (loaded_node.rotation.size()) {
        // GLM's quat expects WXYZ order, but GLTF uses XYZW order
        rotation = glm::quat(static_cast<float>(loaded_node.rotation[3]),
                             static_cast<float>(loaded_node.rotation[0]),
                             static_cast<float>(loaded_node.rotation[1]),
                             static_cast<float>(loaded_node.rotation[2]));
      }
      if (loaded_node.scale.size()) {
        scale = glm::vec3(loaded_node.scale[0], loaded_node.scale[1], loaded_node.scale[2]);
      }
      // Combine translate, rotation, and scale
      // Build transform: translate * rotate * scale
      new_node->transform = glm::translate(glm::mat4(1.0f), translation) *
                            glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);
    }

    new_node->inv_transform = glm::inverse(new_node->transform);
    new_node->inv_transpose = glm::inverseTranspose(new_node->transform);

    // Load the mesh or camera of the node. Potential optimization: Right now we are creating a new
    // mesh on every node. In the case where meshes are instanced, we will created Mesh structs with
    // the same data. We could use a lookup set to resolve this in the future.
    if (loaded_node.mesh != -1) {
      new_node->data = std::make_unique<Mesh>();
      tinygltf::Mesh loaded_mesh = model.meshes[static_cast<size_t>(loaded_node.mesh)];

      // Load primitives
      for (tinygltf::Primitive& loaded_prim : loaded_mesh.primitives) {
        Primitive new_prim;
        new_prim.material_id = loaded_prim.material;
        if (loaded_prim.mode != 4) {
          SDL_Log(
              "[Scene] GLTF Loading: Mesh detected that uses a mode other than TRIANGLES. This is "
              "currently unsupported.");
        }

        int pos_buffer_id = -1;
        int nor_buffer_id = -1;
        int uv_buffer_id = -1;
        int ind_buffer_id = -1;

        if (loaded_prim.attributes.count("POSITION") > 0) {
          int accessor_id = loaded_prim.attributes["POSITION"];
          tinygltf::Accessor accessor = model.accessors[static_cast<size_t>(accessor_id)];
          int buffer_view_id = accessor.bufferView;
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

          tinygltf::BufferView buffer_view = model.bufferViews[static_cast<size_t>(buffer_view_id)];
          new_prim.pos_offset = static_cast<int>(buffer_view.byteOffset);
          pos_buffer_id = buffer_view.buffer;
        }

        if (loaded_prim.attributes.count("NORMAL") > 0) {
          int accessor_id = loaded_prim.attributes["NORMAL"];
          tinygltf::Accessor accessor = model.accessors[static_cast<size_t>(accessor_id)];
          int buffer_view_id = accessor.bufferView;
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

          tinygltf::BufferView buffer_view = model.bufferViews[static_cast<size_t>(buffer_view_id)];
          new_prim.nor_offset = static_cast<int>(buffer_view.byteOffset);
          nor_buffer_id = buffer_view.buffer;
        }

        // Currently only accepting one uv coordinate per primative.
        if (loaded_prim.attributes.count("TEX_COORD0") > 0) {
          int accessor_id = loaded_prim.attributes["TEX_COORD0"];
          tinygltf::Accessor accessor = model.accessors[static_cast<size_t>(accessor_id)];
          int buffer_view_id = accessor.bufferView;
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

          tinygltf::BufferView buffer_view = model.bufferViews[static_cast<size_t>(buffer_view_id)];
          new_prim.uv_offset = static_cast<int>(buffer_view.byteOffset);
          uv_buffer_id = buffer_view.buffer;
        }

        if (loaded_prim.indices != -1) {
          int accessor_id = loaded_prim.indices;
          tinygltf::Accessor accessor = model.accessors[static_cast<size_t>(accessor_id)];
          int buffer_view_id = accessor.bufferView;
          if (accessor.componentType != 5123 && accessor.componentType != 5125) {
            SDL_Log(
                "[Scene] GLTF Loading: Expected all indices to either be a unsigned short or an "
                "unsigned int - unsupported component type detected");
          }
          // Indices uses unsigned ints instead of unsigned shorts
          if (accessor.componentType == 5124) {
            new_prim.sixteen_bit_indices = false;
          }
          if (accessor.type != 65) {
            SDL_Log(
                "[Scene] GLTF Loading: Expected all indices to be scalars - unsupported type "
                "detected");
          }

          size_t count = accessor.count;
          tinygltf::BufferView buffer_view = model.bufferViews[static_cast<size_t>(buffer_view_id)];
          new_prim.ind_offset = static_cast<int>(buffer_view.byteOffset);
          new_prim.ind_count = count;
          ind_buffer_id = buffer_view.buffer;
        }

        // Assume position data always exists
        if ((pos_buffer_id != nor_buffer_id || nor_buffer_id == -1) &&
            (pos_buffer_id != uv_buffer_id || uv_buffer_id == -1) &&
            (pos_buffer_id != ind_buffer_id || ind_buffer_id == -1)) {
          SDL_Log("[Scene] GLTF Loading: Prim with attributes in multiple buffers detected!");
        }
        new_prim.buffer_id = static_cast<size_t>(pos_buffer_id);

        std::get<std::unique_ptr<Mesh>>(new_node->data)->primitives.push_back(new_prim);
      }
    } else {  // Presumably the node is a camera
      std::unique_ptr<Camera> camera = std::make_unique<Camera>();
      tinygltf::Camera loaded_camera = model.cameras[static_cast<size_t>(loaded_node.camera)];
      camera->aspect_ratio = loaded_camera.perspective.aspectRatio;
      camera->near_plane = loaded_camera.perspective.znear;
      camera->far_plane = loaded_camera.perspective.zfar;
      camera->fov_y = loaded_camera.perspective.yfov;
      camera->view_mat = new_node->transform;
      camera->forward =
          glm::normalize(glm::vec3(new_node->transform * glm::vec4(0.f, 0.f, -1.f, 0.f)));
      camera->up = glm::normalize(glm::vec3(new_node->transform * glm::vec4(0.f, 1.f, 0.f, 0.f)));
      camera->right = glm::normalize(glm::cross(camera->forward, camera->up));

      // The main camera will be the first camera loaded.
      if (scene.main_camera == nullptr) {
        scene.main_camera = camera.get();
      }
      new_node->data = std::move(camera);
    }
    scene.nodes.push_back(std::move(new_node));
  }

  // Assumes scene.nodes order is the same as the indices in the GLTF. Will probably break if this
  // function is multithreaded.
  for (size_t i = 0; i < scene.nodes.size(); i++) {
    std::unique_ptr<Node>& node = scene.nodes[i];
    std::vector<int> children = children_lists[i];
    for (int child : children) {
      std::unique_ptr<Node>& child_node = scene.nodes[static_cast<size_t>(child)];
      child_node->parent = node.get();
      node->children.push_back(child_node.get());
    }
  }

  return true;
}

}  // namespace racecar::scene
