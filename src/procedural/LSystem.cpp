#include "LSystem.h"

#include <cmath>
#include <iostream>
#include <stack>

#include <OgreManualObject.h>
#include <OgreMesh.h>
#include <OgreResourceGroupManager.h>

namespace bbfx {

std::string LSystem::derive() const {
    std::string cur = mAxiom;
    for (int i = 0; i < mIterations; ++i) {
        std::string next;
        next.reserve(cur.size() * 2);
        for (char c : cur) {
            auto it = mRules.find(c);
            if (it != mRules.end()) next += it->second;
            else                     next += c;
        }
        cur = std::move(next);
    }
    return cur;
}

namespace {
// 3×3 matrix to orient the turtle. Columns are heading / left / up.
struct M3 {
    float m[3][3];
    static M3 identity() {
        M3 r{};
        r.m[0][0] = 1; r.m[1][1] = 1; r.m[2][2] = 1;
        return r;
    }
    M3 rotate(float angleRad, int axis) const {
        // axis: 0 = heading (roll), 1 = left (pitch), 2 = up (yaw)
        float c = std::cos(angleRad), s = std::sin(angleRad);
        M3 rot = M3::identity();
        if (axis == 2) { // yaw around up (column 2)
            rot.m[0][0] = c; rot.m[0][1] = -s;
            rot.m[1][0] = s; rot.m[1][1] =  c;
        } else if (axis == 1) { // pitch around left (column 1)
            rot.m[0][0] = c; rot.m[0][2] = s;
            rot.m[2][0] = -s; rot.m[2][2] = c;
        } else { // roll around heading (column 0)
            rot.m[1][1] = c; rot.m[1][2] = -s;
            rot.m[2][1] = s; rot.m[2][2] =  c;
        }
        // this * rot (apply rot in the turtle's local frame)
        M3 out{};
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) {
                out.m[i][j] = 0;
                for (int k = 0; k < 3; ++k)
                    out.m[i][j] += m[i][k] * rot.m[k][j];
            }
        return out;
    }
    void heading(float& x, float& y, float& z) const { x = m[0][0]; y = m[1][0]; z = m[2][0]; }
};
}

std::vector<LSystem::Segment> LSystem::segments() const {
    std::string s = derive();
    std::vector<Segment> out;
    out.reserve(s.size());

    struct State { float x, y, z; M3 orient; };
    std::stack<State> stk;

    State cur{ 0, 0, 0, M3::identity() };
    float ang = mAngleDeg * 3.14159265f / 180.0f;

    for (char c : s) {
        switch (c) {
            case 'F': {
                float hx, hy, hz; cur.orient.heading(hx, hy, hz);
                float nx = cur.x + hx * mStep;
                float ny = cur.y + hy * mStep;
                float nz = cur.z + hz * mStep;
                out.push_back(Segment{ cur.x, cur.y, cur.z, nx, ny, nz });
                cur.x = nx; cur.y = ny; cur.z = nz;
                break;
            }
            case 'f': {
                float hx, hy, hz; cur.orient.heading(hx, hy, hz);
                cur.x += hx * mStep;
                cur.y += hy * mStep;
                cur.z += hz * mStep;
                break;
            }
            case '+': cur.orient = cur.orient.rotate(+ang, 2); break;
            case '-': cur.orient = cur.orient.rotate(-ang, 2); break;
            case '&': cur.orient = cur.orient.rotate(+ang, 1); break;
            case '^': cur.orient = cur.orient.rotate(-ang, 1); break;
            case '/': cur.orient = cur.orient.rotate(+ang, 0); break;
            case '\\':cur.orient = cur.orient.rotate(-ang, 0); break;
            case '[': stk.push(cur); break;
            case ']': if (!stk.empty()) { cur = stk.top(); stk.pop(); } break;
            default: break;
        }
    }
    return out;
}

std::string LSystem::generateMesh(const std::string& meshName) const {
    auto segs = segments();
    if (segs.empty()) return {};
    try {
        Ogre::ManualObject mo(meshName);
        mo.begin("BaseWhiteNoLighting", Ogre::RenderOperation::OT_LINE_LIST);
        for (auto& s : segs) {
            mo.position(s.x0, s.y0, s.z0);
            mo.position(s.x1, s.y1, s.z1);
        }
        mo.end();
        auto mesh = mo.convertToMesh(meshName,
                     Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
        if (mesh) return meshName;
    } catch (const std::exception& e) {
        std::cerr << "[LSystem] generateMesh(" << meshName << ") failed: "
                   << e.what() << std::endl;
    }
    return {};
}

} // namespace bbfx
