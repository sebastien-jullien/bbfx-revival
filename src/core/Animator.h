#pragma once

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>
#include <map>
#include <set>
#include <queue>
#include <deque>
#include <vector>

#include "../bbfx.h"
#include "AnimationNode.h"
#include "PrimitiveNodes.h"
#include "Mutex.h"

namespace bbfx {

class AnimationGraph {
public:
    AnimationGraph();
    virtual ~AnimationGraph();
protected:
    using Graph = boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS>;
    using Vertex = boost::graph_traits<Graph>::vertex_descriptor;
    using IndexMap = boost::property_map<Graph, boost::vertex_index_t>::type;
    using VertexIterator = boost::graph_traits<Graph>::vertex_iterator;
    using AdjacencyIterator = boost::graph_traits<Graph>::adjacency_iterator;

    using VertexMap = std::map<AnimationPort*, Vertex>;
    using PortMap = std::map<Vertex, AnimationPort*>;

    Graph mGraph;
    VertexMap mVertexMap;
    PortMap mPortMap;
};

struct Operation {
    bool link;
    AnimationPort* from;
    AnimationPort* to;
    Operation(bool l, AnimationPort* s, AnimationPort* t) : link(l), from(s), to(t) {}
};

template <typename Action, typename TimeStamp>
struct Event {
    Action action;
    TimeStamp time;
    bool operator<(const Event& e) const { return e.time < time; }
    Event(Action a, TimeStamp t) : action(std::move(a)), time(t) {}
};

class Animator : public AnimationGraph, public AnimationNodeListener {
public:
    Animator();
    virtual ~Animator();
    static Animator* instance();
    void notifyUpdate(AnimationNode* node) override;

    void add(AnimationPort* port);
    void remove(AnimationPort* port);
    bool contains(AnimationPort* port) const;

    void link(AnimationPort* s, AnimationPort* t);
    void unlink(AnimationPort* s, AnimationPort* t);

    using TimeStamp = float;
    void schedule(const Operation& op, TimeStamp time);

    void removeNode(AnimationNode* node);
    void registerNode(AnimationNode* node);
    bool renameNode(const std::string& oldName, const std::string& newName);
    std::vector<std::string> getRegisteredNodeNames() const;
    AnimationNode* getRegisteredNode(const std::string& name) const;
    void exportDOT(const string& filename) const;

    struct LinkInfo {
        std::string fromNode, fromPort, toNode, toPort;
    };
    std::vector<LinkInfo> getLinks() const;

    void renderOneFrame();

protected:
    unsigned long mFrame = 0;
    TimeStamp mTime = 0.0f;
    Mutex mMutex;

    using OperationEvent = Event<Operation, TimeStamp>;
    using PreOpQueue = std::priority_queue<OperationEvent>;
    using PostOpQueue = std::deque<Operation>;
    using PortQueue = std::deque<AnimationPort*>;

    PreOpQueue mPreOpQueue;
    void executePendingPreOps();
    void executePreOp(const Operation& op, TimeStamp delay);

    PortQueue mPortQueue;
    void enqueueOutputs(AnimationNode* node, PortQueue& queue);
    void propagateFreshValues();

    PostOpQueue mPostOpQueue;
    void executePendingPostOps();
    void executePostOp(const Operation& op);

    std::map<std::string, AnimationNode*> mRegisteredNodes;

private:
    static Animator* sInstance;
};

} // namespace bbfx
