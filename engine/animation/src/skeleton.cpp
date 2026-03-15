#include "nexus/animation/skeleton.h"

namespace nexus::anim {

u32 Skeleton::add_bone(const Bone& bone) {
    u32 index = static_cast<u32>(bones_.size());
    bones_.push_back(bone);
    return index;
}

i32 Skeleton::find_bone(const std::string& name) const {
    for (size_t i = 0; i < bones_.size(); ++i) {
        if (bones_[i].name == name) return static_cast<i32>(i);
    }
    return -1;
}

std::vector<BonePose> Skeleton::get_bind_pose() const {
    std::vector<BonePose> poses(bones_.size());
    for (size_t i = 0; i < bones_.size(); ++i) {
        poses[i].position = bones_[i].local_position;
        poses[i].rotation = bones_[i].local_rotation;
        poses[i].scale = bones_[i].local_scale;
    }
    return poses;
}

std::vector<Mat4> Skeleton::compute_skin_matrices(const std::vector<BonePose>& local_poses) const {
    size_t count = bones_.size();
    std::vector<Mat4> world_matrices(count, Mat4(1.0f));
    std::vector<Mat4> skin_matrices(count, Mat4(1.0f));

    for (size_t i = 0; i < count; ++i) {
        Mat4 local = local_poses[i].to_matrix();

        if (bones_[i].parent_index >= 0) {
            world_matrices[i] = world_matrices[static_cast<size_t>(bones_[i].parent_index)] * local;
        } else {
            world_matrices[i] = local;
        }

        skin_matrices[i] = world_matrices[i] * bones_[i].inverse_bind_pose;
    }

    return skin_matrices;
}

} // namespace nexus::anim
