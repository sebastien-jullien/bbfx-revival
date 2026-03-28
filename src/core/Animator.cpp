#include "Animator.h"
#include <boost/graph/breadth_first_search.hpp>
#include <fstream>
#include <set>

namespace bbfx {

// ── AnimationGraph ──────────────────────────────────────────────────────────

AnimationGraph::AnimationGraph() = default;
AnimationGraph::~AnimationGraph() = default;

// ── Animator ────────────────────────────────────────────────────────────────

Animator* Animator::sInstance = nullptr;

Animator::Animator() {
    assert(!sInstance);
    sInstance = this;
}

Animator::~Animator() {
    sInstance = nullptr;
}

Animator* Animator::instance() {
    return sInstance;
}

void Animator::notifyUpdate(AnimationNode* node) {
    Lock guard(mMutex);
    enqueueOutputs(node, mPortQueue);
}

void Animator::add(AnimationPort* port) {
    Lock guard(mMutex);
    if (mVertexMap.find(port) == mVertexMap.end()) {
        Vertex v = boost::add_vertex(mGraph);
        mVertexMap[port] = v;
        mPortMap[v] = port;
    }
}

void Animator::remove(AnimationPort* port) {
    Lock guard(mMutex);
    auto it = mVertexMap.find(port);
    if (it != mVertexMap.end()) {
        boost::clear_vertex(it->second, mGraph);
        boost::remove_vertex(it->second, mGraph);
        mPortMap.erase(it->second);
        mVertexMap.erase(it);
    }
}

bool Animator::contains(AnimationPort* port) const {
    return mVertexMap.find(port) != mVertexMap.end();
}

void Animator::link(AnimationPort* s, AnimationPort* t) {
    Lock guard(mMutex);
    add(s);
    add(t);
    boost::add_edge(mVertexMap[s], mVertexMap[t], mGraph);
}

void Animator::unlink(AnimationPort* s, AnimationPort* t) {
    Lock guard(mMutex);
    auto si = mVertexMap.find(s);
    auto ti = mVertexMap.find(t);
    if (si != mVertexMap.end() && ti != mVertexMap.end()) {
        boost::remove_edge(si->second, ti->second, mGraph);
    }
}

void Animator::schedule(const Operation& op, TimeStamp time) {
    Lock guard(mMutex);
    mPreOpQueue.push(OperationEvent(op, time));
}

void Animator::removeNode(AnimationNode* node) {
    Lock guard(mMutex);
    // Collect all ports belonging to this node
    std::vector<AnimationPort*> ports;
    for (auto& [name, port] : node->getInputs()) {
        if (mVertexMap.find(port) != mVertexMap.end()) {
            ports.push_back(port);
        }
    }
    for (auto& [name, port] : node->getOutputs()) {
        if (mVertexMap.find(port) != mVertexMap.end()) {
            ports.push_back(port);
        }
    }
    // Remove in reverse order to minimize vertex index invalidation issues
    // Since boost::vecS invalidates descriptors on remove, we re-lookup each time
    for (auto* port : ports) {
        auto it = mVertexMap.find(port);
        if (it != mVertexMap.end()) {
            boost::clear_vertex(it->second, mGraph);
            // After remove_vertex, all vertex descriptors > removed are invalidated
            // We need to rebuild mappings for vecS
            Vertex removed = it->second;
            boost::remove_vertex(removed, mGraph);
            // Rebuild mPortMap and mVertexMap after removal
            mPortMap.clear();
            VertexMap newVertexMap;
            for (auto& [p, v] : mVertexMap) {
                if (p == port) continue; // skip removed
                Vertex newV = v;
                if (v > removed) newV = v - 1;
                newVertexMap[p] = newV;
                mPortMap[newV] = p;
            }
            mVertexMap = std::move(newVertexMap);
        }
    }
    node->setListener(nullptr);
    mRegisteredNodes.erase(node->getName());
}

void Animator::registerNode(AnimationNode* node) {
    mRegisteredNodes[node->getName()] = node;
}

std::vector<std::string> Animator::getRegisteredNodeNames() const {
    std::vector<std::string> names;
    names.reserve(mRegisteredNodes.size());
    for (auto& [name, _] : mRegisteredNodes) {
        names.push_back(name);
    }
    return names;
}

AnimationNode* Animator::getRegisteredNode(const std::string& name) const {
    auto it = mRegisteredNodes.find(name);
    return (it != mRegisteredNodes.end()) ? it->second : nullptr;
}

std::vector<Animator::LinkInfo> Animator::getLinks() const {
    std::vector<LinkInfo> result;
    auto edges = boost::edges(mGraph);
    for (auto ei = edges.first; ei != edges.second; ++ei) {
        auto sv = boost::source(*ei, mGraph);
        auto tv = boost::target(*ei, mGraph);
        auto si = mPortMap.find(sv);
        auto ti = mPortMap.find(tv);
        if (si != mPortMap.end() && ti != mPortMap.end()) {
            AnimationPort* sp = si->second;
            AnimationPort* tp = ti->second;
            if (sp->getNode() && tp->getNode()) {
                result.push_back({
                    sp->getNode()->getName(), sp->getName(),
                    tp->getNode()->getName(), tp->getName()
                });
            }
        }
    }
    return result;
}

void Animator::exportDOT(const string& filename) const {
    std::ofstream out(filename);
    if (!out.is_open()) return;

    out << "digraph animator {\n  rankdir=LR;\n";

    // Collect unique nodes and their shapes
    std::set<std::string> writtenNodes;
    for (auto& [port, vertex] : mVertexMap) {
        AnimationNode* node = port->getNode();
        if (node && writtenNodes.find(node->getName()) == writtenNodes.end()) {
            writtenNodes.insert(node->getName());
            out << "  \"" << node->getName() << "\" [shape=box];\n";
        }
    }

    // Write edges
    auto edges = boost::edges(mGraph);
    for (auto ei = edges.first; ei != edges.second; ++ei) {
        auto srcV = boost::source(*ei, mGraph);
        auto tgtV = boost::target(*ei, mGraph);
        auto srcIt = mPortMap.find(srcV);
        auto tgtIt = mPortMap.find(tgtV);
        if (srcIt != mPortMap.end() && tgtIt != mPortMap.end()) {
            AnimationPort* srcPort = srcIt->second;
            AnimationPort* tgtPort = tgtIt->second;
            std::string srcNode = srcPort->getNode() ? srcPort->getNode()->getName() : "?";
            std::string tgtNode = tgtPort->getNode() ? tgtPort->getNode()->getName() : "?";
            out << "  \"" << srcNode << "\":\"" << srcPort->getName()
                << "\" -> \"" << tgtNode << "\":\"" << tgtPort->getName() << "\";\n";
        }
    }

    out << "}\n";
}

void Animator::renderOneFrame() {
    Lock guard(mMutex);
    ++mFrame;

    RootTimeNode* time = RootTimeNode::instance();
    if (time) {
        mTime = time->getTotalTime();
    }

    executePendingPreOps();
    propagateFreshValues();
    executePendingPostOps();
}

void Animator::executePendingPreOps() {
    while (!mPreOpQueue.empty() && mPreOpQueue.top().time <= mTime) {
        auto event = mPreOpQueue.top();
        mPreOpQueue.pop();
        executePreOp(event.action, mTime - event.time);
    }
}

void Animator::executePreOp(const Operation& op, TimeStamp /*delay*/) {
    if (op.link) {
        link(op.from, op.to);
    } else {
        unlink(op.from, op.to);
    }
}

void Animator::enqueueOutputs(AnimationNode* node, PortQueue& queue) {
    for (auto& [name, port] : node->getOutputs()) {
        queue.push_back(port);
    }
}

void Animator::propagateFreshValues() {
    while (!mPortQueue.empty()) {
        AnimationPort* port = mPortQueue.front();
        mPortQueue.pop_front();

        auto vi = mVertexMap.find(port);
        if (vi == mVertexMap.end()) continue;

        // BFS: propagate to adjacent ports
        AdjacencyIterator ai, ai_end;
        for (boost::tie(ai, ai_end) = boost::adjacent_vertices(vi->second, mGraph);
             ai != ai_end; ++ai) {
            auto pi = mPortMap.find(*ai);
            if (pi != mPortMap.end()) {
                AnimationPort* target = pi->second;
                target->setValue(port->getValue());

                // If target port's node has all inputs fresh, call update
                AnimationNode* targetNode = target->getNode();
                if (targetNode) {
                    targetNode->update();
                    enqueueOutputs(targetNode, mPortQueue);
                }
            }
        }
    }
}

void Animator::executePendingPostOps() {
    while (!mPostOpQueue.empty()) {
        Operation op = mPostOpQueue.front();
        mPostOpQueue.pop_front();
        executePostOp(op);
    }
}

void Animator::executePostOp(const Operation& op) {
    if (op.link) {
        link(op.from, op.to);
    } else {
        unlink(op.from, op.to);
    }
}

} // namespace bbfx
